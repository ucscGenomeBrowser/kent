/* gbAlignTbl - loading of PSL alignment tables. This defines per-table
 * objects that handling loading and updating. */
#include "common.h"
#include "gbAlignTbl.h"
#include "errabort.h"
#include "hash.h"
#include "portable.h"
#include "linefile.h"
#include "jksql.h"
#include "psl.h"
#include "estOrientInfo.h"
#include "hdb.h"
#include "gbIndex.h"
#include "gbRelease.h"
#include "gbEntry.h"
#include "dbLoadOptions.h"
#include "gbAligned.h"
#include "gbGenome.h"
#include "gbStatusTbl.h"
#include "gbProcessed.h"
#include "gbFileOps.h"
#include "gbVerb.h"
#include "sqlUpdater.h"
#include "gbBuildState.h"
#include "gbSql.h"
#include "sqlDeleter.h"

/* table names and per-chrom suffixes */
char *ALL_MRNA_TBL = "all_mrna";
char *MRNA_TBL_SUF = "mrna";
char *MRNA_ORIENTINFO_TBL = "mrnaOrientInfo";
char *ALL_EST_TBL = "all_est";
char *EST_TBL_SUF = "est";
char *INTRON_EST_TBL = "intronEst";
char *INTRON_EST_TBL_SUF = "intronEst";
char *EST_ORIENTINFO_TBL = "estOrientInfo";
char *XENO_MRNA_TBL = "xenoMrna";
char *XENO_EST_TBL = "xenoEst";
char *REFSEQ_ALI_TBL = "refSeqAli";
char *XENO_REFSEQ_ALI_TBL = "xenoRefSeqAli";
char *MGC_FULL_MRNA_TBL = "mgcFullMrna";
char *ORFEOME_MRNA_TBL = "orfeomeMrna";

/* forwards */
static struct sqlUpdater *gbAlignTblSetOiUpdGet(struct gbAlignTblSet *gats,
                                                unsigned type,
                                                struct sqlConnection *conn);

struct gbAlignTbl
/* Object associated with an alignment table and associated tables. */
{
    struct gbAlignTblSet *gats; // set contained this object
    char *tbl;                  // table name, or NULL if only per-chrom
    unsigned type;              // GB_MRNA or GB_EST
    boolean large;              // could be >= 4gb
    char *perChromSuf;          // per-chrom suffix for table, or NULL
    struct sqlUpdater* upd;     // updater for table
    struct hash *perChromUpds;  // hash by chrom of per-chrom updaters
    struct sqlUpdater *oiUpd;   // orientInfo table updater
    char tmpDir[PATH_LEN];      // tmp directory
};

static struct gbAlignTbl *gbAlignTblNew(struct gbAlignTblSet *gats, char *tbl, unsigned type, boolean large,
                                        char *perChromSuf, char *tmpDir)
/* construct a new gbAlignTbl object, tbl can be NULL if only per-chrom tables
 * are created.  perChromSuf can be NULL if no per-chrom tables are
 * created.  */
{
struct gbAlignTbl *gat;
AllocVar(gat);
gat->gats = gats;
gat->tbl = cloneString(tbl);
gat->type = type;
gat->large = large;
gat->perChromSuf = cloneString(perChromSuf);
safecpy(gat->tmpDir, sizeof(gat->tmpDir), tmpDir);
return gat;
}

static void gbAlignTblFree(struct gbAlignTbl **gatPtr)
/* free a gbAlignTbl object */
{
struct gbAlignTbl *gat = *gatPtr;
if (gat != NULL)
    {
    freeMem(gat->tbl);
    freeMem(gat->perChromSuf);
    sqlUpdaterFree(&gat->upd);
    if (gat->perChromUpds != NULL)
        {
        struct hashCookie hc = hashFirst(gat->perChromUpds);
        struct hashEl *hel;
        while ((hel = hashNext(&hc)) != NULL)
            sqlUpdaterFree((struct sqlUpdater**)&hel->val);
        hashFree(&gat->perChromUpds);
        freeMem(gat->oiUpd);
        }
    *gatPtr = NULL;
    }
}

static void gbAlignTblCommit(struct gbAlignTbl *gat,
                             struct sqlConnection *conn)
/* commit pending gbAlignTbl updates */
{
/* commit in reverse order so missing main table entry will trigger others */
if (gat->perChromUpds != NULL)
    {
    struct hashCookie hc = hashFirst(gat->perChromUpds);
    struct hashEl *hel;
    while ((hel = hashNext(&hc)) != NULL)
        sqlUpdaterCommit(hel->val, conn);
    }
if (gat->upd != NULL)
    sqlUpdaterCommit(gat->upd, conn);

}

static void createPslTable(struct sqlConnection *conn, char* table,
                           unsigned pslOptions, boolean large)
/* create a PSL table. will create an over 4gb table if in large is set */
{
int tNameIdxLen = (pslOptions & PSL_TNAMEIX) ? hGetMinIndexLength(sqlGetDatabase(conn)) : 0;
char *sqlCmd = pslGetCreateSql(table, pslOptions, tNameIdxLen);
char extra[64];

if (large)
    {
    /* add options to create table to allow over 4gb */
    static unsigned avgRowLength = 200;
    static unsigned long long maxRows = 0xffffffffffffLL;
    safef(extra, sizeof(extra), " AVG_ROW_LENGTH=%u MAX_ROWS=%llu",
          avgRowLength, maxRows);
    sqlCmd = needMoreMem(sqlCmd, strlen(sqlCmd)+1, strlen(sqlCmd)+strlen(extra)+1);
    strcat(sqlCmd, extra);
    }
sqlRemakeTable(conn, table, sqlCmd);
freez(&sqlCmd);
}

static void createPerChromPslTables(char* rootTable, struct sqlConnection* conn)
/* create per-chromosome PSL tables if they don't exist.  This creates tables
 * for all chromosomes, as this makes various queries easier (table browser
 * depends on this). */
{
struct slName* chrom;
char table[64];
for (chrom = getChromNames(sqlGetDatabase(conn)); chrom != NULL; chrom = chrom->next)
    {
    safef(table, sizeof(table), "%s_%s", chrom->name, rootTable);
    if (!sqlTableExists(conn, table))
        createPslTable(conn, table, PSL_WITH_BIN, FALSE);
    }
}

static FILE* getPslTabFh(struct gbAlignTbl *gat, struct sqlConnection* conn)
/* Get the tab file for a PSL table, creating table or updater as need. */
{
if (gat->upd == NULL)
    {
    if (!sqlTableExists(conn, gat->tbl))
        createPslTable(conn, gat->tbl, (PSL_TNAMEIX|PSL_WITH_BIN), gat->large);
    gat->upd = sqlUpdaterNew(gat->tbl, gat->tmpDir, (gbVerbose >= 4), NULL);
    }
return sqlUpdaterGetFh(gat->upd, 1);
}

static FILE* getChromPslTabFh(struct gbAlignTbl *gat, char *chrom,
                              struct sqlConnection* conn)
/* get the tab file for a per-chrom PSL, creating and opening as needed */
{
if (gat->perChromUpds == NULL)
    {
    gat->perChromUpds = hashNew(12);
    createPerChromPslTables(gat->perChromSuf, conn);
    }
struct hashEl* hel = hashLookup(gat->perChromUpds, chrom);
if (hel == NULL)
    {
    char table[64];
    safef(table, sizeof(table), "%s_%s", chrom, gat->perChromSuf);
    struct sqlUpdater* upd = sqlUpdaterNew(table, gat->tmpDir, (gbVerbose >= 4), NULL);
    hel = hashAdd(gat->perChromUpds, chrom, upd);
    }
return sqlUpdaterGetFh((struct sqlUpdater*)hel->val, 1);
}

static FILE* getOiTabFh(struct gbAlignTbl *gat, struct sqlConnection* conn)
/* Get the tab file for orientInfo table, creating table or updater as
 * need. */
{
struct sqlUpdater *upd = gbAlignTblSetOiUpdGet(gat->gats, gat-> type, conn);
return sqlUpdaterGetFh(upd, 1);
}

static void writePsl(FILE* fh, struct psl* psl)
/* write a psl with bin */
{
/* FIXME: this looked liked a BLAT bug that may have been fixed 
 * see ../blat-bug */
if ((psl->qBaseInsert < 0) || (psl->tBaseInsert < 0))
    {
    fprintf(stderr, "Warning: PSL with negative insert counts: ");
    pslTabOut(psl, stderr);
    if (psl->qBaseInsert < 0)
        psl->qBaseInsert = 0;
    if (psl->tBaseInsert < 0)
        psl->tBaseInsert = 0;
    }
fprintf(fh, "%u\t", hFindBin(psl->tStart, psl->tEnd));
pslTabOut(psl, fh);
}

void gbAlignTblWrite(struct gbAlignTbl *gat, struct psl* psl,
                     struct sqlConnection *conn)
/* write new align to PSL tables */
{
char *vdot = gbDropVer(psl->qName);
if (gat->tbl != NULL)
    {
    FILE* fh = getPslTabFh(gat, conn);
    writePsl(fh, psl);
    }
if (gat->perChromSuf != NULL)
    {
    FILE* fh = getChromPslTabFh(gat, psl->tName, conn);
    writePsl(fh, psl);
    }
gbRestoreVer(psl->qName, vdot);
}

void gbAlignTblWriteOi(struct gbAlignTbl *gat, struct estOrientInfo* oi,
                       struct sqlConnection *conn)
/* write new align orientInfo */
{
FILE* fh = getOiTabFh(gat, conn);
char *vdot = gbDropVer(oi->name);
fprintf(fh, "%u\t", hFindBin(oi->chromStart, oi->chromEnd));
estOrientInfoTabOut(oi, fh);
gbRestoreVer(oi->name, vdot);
}

static void addPerChromTbls(struct sqlConnection *conn, char *suffix,
                            struct slName** tables)
/* add per-chrom tables to the list, if they exist */
{
char like[64];
safef(like, sizeof(like), "%%__%s", suffix);
struct slName* tbls = gbSqlListTablesLike(conn, like);
struct slName* tbl;
for (tbl = tbls; tbl != NULL; tbl = tbl->next)
    {
    if (!(sameString(tbl->name, ALL_MRNA_TBL) || sameString(tbl->name, ALL_EST_TBL)))
        slSafeAddHead(tables, slNameNew(tbl->name));
    }
slFreeList(&tbls);
}

struct slName* gbAlignTblList(struct sqlConnection *conn)
/* Get list of genePred tables in database. */
{
/* always try for perChrom tables, in case config has been changed */
struct slName* tables = NULL;
gbAddTableIfExists(conn, ALL_MRNA_TBL, &tables);
addPerChromTbls(conn, MRNA_TBL_SUF, &tables);
gbAddTableIfExists(conn, MRNA_ORIENTINFO_TBL, &tables);
gbAddTableIfExists(conn, ALL_EST_TBL, &tables);
addPerChromTbls(conn, EST_TBL_SUF, &tables);
gbAddTableIfExists(conn, INTRON_EST_TBL, &tables);
addPerChromTbls(conn, INTRON_EST_TBL_SUF, &tables);
gbAddTableIfExists(conn, EST_ORIENTINFO_TBL, &tables);
gbAddTableIfExists(conn, XENO_MRNA_TBL, &tables);
gbAddTableIfExists(conn, XENO_EST_TBL, &tables);
gbAddTableIfExists(conn, REFSEQ_ALI_TBL, &tables);
gbAddTableIfExists(conn, XENO_REFSEQ_ALI_TBL, &tables);
gbAddTableIfExists(conn, MGC_FULL_MRNA_TBL, &tables);
gbAddTableIfExists(conn, ORFEOME_MRNA_TBL, &tables);
return tables;
}

struct gbAlignTblSet
/* set of gbAlignTbl objects for all possible tables, created in
 * a lazy manner. */
{
    boolean perChrom;      // create per-chrom tables where approriate
    char tmpDir[PATH_LEN];
    
    // standard  tables, addressed by srcDb|type|orgCat
    struct gbAlignTbl *stdTbls[GB_NUM_ATTRS];

    // orientInfo updaters, mRNA table is shared by genbank and refSeq,
    // so can't be directly in gbAlignTbl objects
    struct sqlUpdater *oiUpds[GB_NUM_TYPES];

    // special tables
    struct gbAlignTbl *intronEst;
    struct gbAlignTbl *mgcMRna;
    struct gbAlignTbl *orfeomeMRna;
};

struct gbAlignTblSet *gbAlignTblSetNew(boolean perChrom, char *tmpDir)
/* construct a new gbAlignTblSet object */
{
struct gbAlignTblSet *gats;
AllocVar(gats);
gats->perChrom = perChrom;
safecpy(gats->tmpDir, sizeof(gats->tmpDir), tmpDir);
return gats;
}

void gbAlignTblSetFree(struct gbAlignTblSet **gatsPtr)
/* free a gbAlignTblSet object */
{
struct gbAlignTblSet *gats = *gatsPtr;
if (gats != NULL)
    {
    int i;
    for (i = 0; i < GB_NUM_TYPES; i++)
        gbAlignTblFree(&gats->stdTbls[i]);
    gbAlignTblFree(&gats->intronEst);
    gbAlignTblFree(&gats->mgcMRna);
    gbAlignTblFree(&gats->orfeomeMRna);
    for (i = 0; i < GB_NUM_TYPES; i++)
        sqlUpdaterFree(&gats->oiUpds[i]);
    freeMem(gats);
    *gatsPtr = NULL;
    }
}

static struct gbAlignTbl * createStdTbl(struct gbAlignTblSet *gats,
                                        unsigned spec)
/* create an entry based on srcDb|type|orgCat */
{
switch (spec) {
case GB_GENBANK|GB_NATIVE|GB_MRNA:
    return gbAlignTblNew(gats, ALL_MRNA_TBL, GB_MRNA, FALSE, (gats->perChrom ? MRNA_TBL_SUF : NULL), gats->tmpDir);
case GB_GENBANK|GB_NATIVE|GB_EST:
    return gbAlignTblNew(gats, ALL_EST_TBL, GB_EST, FALSE, (gats->perChrom ?  EST_TBL_SUF : NULL), gats->tmpDir);
case GB_GENBANK|GB_XENO|GB_MRNA:
    return gbAlignTblNew(gats, XENO_MRNA_TBL, GB_MRNA, FALSE, NULL, gats->tmpDir);
case GB_GENBANK|GB_XENO|GB_EST:
    return gbAlignTblNew(gats, XENO_EST_TBL, GB_EST, TRUE, NULL, gats->tmpDir);
case GB_REFSEQ|GB_NATIVE|GB_MRNA:
    return gbAlignTblNew(gats, REFSEQ_ALI_TBL, GB_MRNA, FALSE, NULL, gats->tmpDir);
case GB_REFSEQ|GB_XENO|GB_MRNA:
    return gbAlignTblNew(gats, XENO_REFSEQ_ALI_TBL, GB_MRNA, FALSE, NULL, gats->tmpDir);
}
errAbort("BUG: invalid spec");
return NULL;
}

static void createOiTable(struct sqlConnection *conn, char* table)
/* create an orientInfo table */
{
char *sql = estOrientInfoGetCreateSql(table, hGetMinIndexLength(sqlGetDatabase(conn)));
sqlRemakeTable(conn, table, sql);
freez(&sql);
}

static struct sqlUpdater *gbAlignTblSetOiUpdGet(struct gbAlignTblSet *gats,
                                                unsigned type,
                                                struct sqlConnection *conn)
/* Get the orientInfo updater for the specified cDNA type */
{
unsigned typeIdx = gbTypeIdx(type);
if (gats->oiUpds[typeIdx] == NULL)
    {
    char *tbl = (type == GB_MRNA) ? MRNA_ORIENTINFO_TBL : EST_ORIENTINFO_TBL;
    if (!sqlTableExists(conn, tbl))
        createOiTable(conn, tbl);
    gats->oiUpds[typeIdx] = sqlUpdaterNew(tbl, gats->tmpDir, (gbVerbose >= 4), NULL);
    }
return gats->oiUpds[typeIdx];
}

struct gbAlignTbl *gbAlignTblSetGet(struct gbAlignTblSet *gats,
                                    struct gbStatus *status)
/* get an entry from the table based on srcDb|type|orgCat,
 * creating if it doesn't exist */
{
unsigned key = status->srcDb|status->type|status->orgCat;
assert(key < GB_NUM_ATTRS);
assert((key&GB_TYPE_MASK) && (key&GB_ORG_CAT_MASK) && (key&GB_SRC_DB_MASK));

if (gats->stdTbls[key] == NULL)
    gats->stdTbls[key] = createStdTbl(gats, key);
return gats->stdTbls[key];
}

struct gbAlignTbl *gbAlignTblSetGetIntronEst(struct gbAlignTblSet *gats)
/* get gbAlignTbl for the intronEst table */
{
if (gats->intronEst == NULL)
    {
    if (gats->perChrom)
        gats->intronEst = gbAlignTblNew(gats, NULL, GB_EST, FALSE, INTRON_EST_TBL_SUF, gats->tmpDir);
    else
        gats->intronEst = gbAlignTblNew(gats, INTRON_EST_TBL, GB_EST, FALSE, NULL, gats->tmpDir);
    }
return gats->intronEst;
}

struct gbAlignTbl *gbAlignTblSetGetMgc(struct gbAlignTblSet *gats)
/* get gbAlignTbl for the MGC table */
{
if (gats->mgcMRna == NULL)
    gats->mgcMRna = gbAlignTblNew(gats, MGC_FULL_MRNA_TBL, GB_MRNA, FALSE, NULL, gats->tmpDir);
return gats->mgcMRna;
}

struct gbAlignTbl *gbAlignTblSetOrfeomeGet(struct gbAlignTblSet *gats)
/* construct a new gbAlignTbl for the orfeome table */
{
if (gats->orfeomeMRna == NULL)
    gats->orfeomeMRna = gbAlignTblNew(gats, ORFEOME_MRNA_TBL, GB_MRNA, FALSE, NULL, gats->tmpDir);
return gats->orfeomeMRna;
}

void gbAlignTblSetCommit(struct gbAlignTblSet *gats,
                         struct sqlConnection *conn)
/* commit all gbAlignTbl objects in an gbAlignTblSet */
{
int i;
for (i = 0; i < GB_NUM_TYPES; i++)
    {
    if (gats->oiUpds[i] != NULL)
        {
        sqlUpdaterCommit(gats->oiUpds[i], conn);
        sqlUpdaterFree(&gats->oiUpds[i]);
        }
    }
if (gats->intronEst != NULL)
    gbAlignTblCommit(gats->intronEst, conn);
if (gats->mgcMRna != NULL)
    gbAlignTblCommit(gats->mgcMRna, conn);
if (gats->orfeomeMRna != NULL)
    gbAlignTblCommit(gats->orfeomeMRna, conn);
/* commit standard table last, so missing will trigger reload of above */
for (i = 0; i < GB_NUM_ATTRS; i++)
    {
    if (gats->stdTbls[i] != NULL)
        gbAlignTblCommit(gats->stdTbls[i], conn);
    }
}
