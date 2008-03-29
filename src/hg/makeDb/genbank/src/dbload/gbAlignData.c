/* Sorting and loading of PSL align records into the database.  This module
 * functions as a singlton object, with global state that can be reset.
 * Refer to the doc/database-update-step.html before modifying. */
#include "common.h"
#include "gbAlignData.h"
#include "gbAlignTbl.h"
#include "gbGeneTbl.h"
#include "gbConf.h"
#include "errabort.h"
#include "hash.h"
#include "portable.h"
#include "linefile.h"
#include "jksql.h"
#include "psl.h"
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
#include "estOrientInfo.h"
#include "gbBuildState.h"
#include "gbSql.h"
#include "sqlDeleter.h"

static char const rcsid[] = "$Id: gbAlignData.c,v 1.29 2008/03/29 04:46:51 markd Exp $";

/* objects handling table loads */
static struct gbAlignTblSet *alignTblSet = NULL;
static struct gbGeneTblSet *geneTblSet = NULL;
static boolean haveMgc = FALSE; /* does this organism have MGC tables */
static boolean haveOrfeome = FALSE; /* does this organism have ORFeome tables */

static void setDerivedTblFlags(struct sqlConnection *conn, struct dbLoadOptions* options)
/* determine if we have MGC or orfeome tables */
{
haveMgc = gbConfGetDbBoolean(options->conf, sqlGetDatabase(conn), "mgc");
haveOrfeome = gbConfGetDbBoolean(options->conf, sqlGetDatabase(conn), "orfeome");
}

void gbAlignDataInit(char *tmpDir, struct dbLoadOptions* options,
                     struct sqlConnection *conn)
/* initialize. called once per genbank type.  tmpDirPath can be null to setup
 * just for deletion.*/
{
if (alignTblSet == NULL)
    {
    assert(geneTblSet == NULL);
    alignTblSet = gbAlignTblSetNew(((options->flags & DBLOAD_PER_CHROM_ALIGN) != 0),
                                   tmpDir);
    geneTblSet = gbGeneTblSetNew(tmpDir);
    }
setDerivedTblFlags(conn, options);
}

static struct gbStatus *lookupStatus(char *accver, struct gbStatusTbl* statusTbl,
                                     struct lineFile *lf, short *versionPtr)
/* get the status entry for an accession if it is selected, return NULL if not
 * selected */
{
char acc[GB_ACC_BUFSZ];
short version = gbSplitAccVer(accver, acc);
if (version < 0)
    errAbort("entry name \"%s\" is not a genbank accession with version: %s:%d",
             accver, lf->fileName, lf->lineIx);

struct gbStatus* status = gbStatusTblFind(statusTbl, acc);
if ((status != NULL) && (status->selectAlign != NULL)
    && (status->selectAlign->version == version))
    {
    if (versionPtr != NULL)
        *versionPtr = version;
    return status;
    }
else
    return NULL;
}

static void processSelectedPsl(struct sqlConnection *conn, struct gbSelect* select,
                               struct gbStatus* status, struct psl* psl)
/* Process the next PSL that is selected for load */
{
struct gbAlignTbl *gat = gbAlignTblSetGet(alignTblSet, status);
gbAlignTblWrite(gat, psl, conn);

if (status->srcDb == GB_REFSEQ)
    {
    /* create refSeq genePred */
    struct gbGeneTbl *ggt = (status->orgCat == GB_NATIVE)
        ? gbGeneTblSetRefGeneGet(geneTblSet, conn)
        : gbGeneTblSetXenoRefGeneGet(geneTblSet, conn);
    gbGeneTblWrite(ggt, status, psl, conn);
    }
if (status->isMgcFull)
    {
    /* create MGC genePred and psl */
    struct gbGeneTbl *ggt = gbGeneTblSetMgcGenesGet(geneTblSet, conn);
    gbGeneTblWrite(ggt, status, psl, conn);
    struct gbAlignTbl *mgat = gbAlignTblSetGetMgc(alignTblSet);
    gbAlignTblWrite(mgat, psl, conn);
    }
if (status->isOrfeome)
    {
    /* create ORFeome genePred and psl */
    struct gbGeneTbl *ggt = gbGeneTblSetOrfeomeGenesGet(geneTblSet, conn);
    gbGeneTblWrite(ggt, status, psl, conn);
    struct gbAlignTbl *ogat = gbAlignTblSetOrfeomeGet(alignTblSet);
    gbAlignTblWrite(ogat, psl, conn);
    }
}

static void processPsl(struct sqlConnection *conn, struct gbSelect* select,
                       struct gbStatusTbl* statusTbl, struct psl* psl,
                       struct lineFile *pslLf)
/* Process the next PSL */
{
short version;
struct gbStatus *status = lookupStatus(psl->qName, statusTbl, pslLf, &version);
if (status != NULL)
    {
    processSelectedPsl(conn, select, status, psl);
    status->version = version;
    status->numAligns++;
    }
}

static void processPslFile(struct sqlConnection *conn, struct gbSelect* select,
                           struct gbStatusTbl* statusTbl, char* pslPath)
/* Parse a psl file looking for accessions to add to the database. */
{
char* row[PSL_NUM_COLS];
struct lineFile *pslLf = gzLineFileOpen(pslPath);
while (lineFileNextRow(pslLf, row, PSL_NUM_COLS))
    {
    struct psl* psl = pslLoad(row);
    processPsl(conn, select, statusTbl, psl, pslLf);
    pslFree(&psl);
    }
gzLineFileClose(&pslLf);
}

static void processOI(struct sqlConnection *conn, struct gbSelect* select,
                      struct gbStatusTbl* statusTbl,
                      struct estOrientInfo* oi, struct lineFile *oiLf)
/* Process the next orientationInfo entry */
{
struct gbStatus *status = lookupStatus(oi->name, statusTbl, oiLf, NULL);
if (status != NULL)
    {
    struct gbAlignTbl *gat = gbAlignTblSetGet(alignTblSet, status);
    gbAlignTblWriteOi(gat, oi, conn);
    }
}

static void processOIFile(struct sqlConnection *conn, struct gbSelect* select,
                          struct gbStatusTbl* statusTbl, char* oiPath)
/* Parse a psl file looking for accessions to add to the database. */
{
char *row[EST_ORIENT_INFO_NUM_COLS];
struct lineFile *oiLf = gzLineFileOpen(oiPath);
while (lineFileNextRow(oiLf, row, EST_ORIENT_INFO_NUM_COLS))
    {
    struct estOrientInfo* oi = estOrientInfoLoad(row);
    processOI(conn, select, statusTbl, oi, oiLf);
    estOrientInfoFree(&oi);
    }
gzLineFileClose(&oiLf);
}

static void processIntronPsl(struct sqlConnection *conn,
                             struct gbSelect* select,
                             struct gbStatusTbl* statusTbl, struct psl* psl,
                             struct lineFile *pslLf)
/* Process the next intron PSL entry */
{
struct gbStatus *status = lookupStatus(psl->qName, statusTbl, pslLf, NULL);
if (status != NULL)
    {
    struct gbAlignTbl *gat = gbAlignTblSetGetIntronEst(alignTblSet);
    gbAlignTblWrite(gat, psl, conn);
    }
}

static void processIntronPslFile(struct sqlConnection *conn,
                                 struct gbSelect* select,
                                 struct gbStatusTbl* statusTbl, char* pslPath)
/* Parse an intron psl file, looking for accessions to add to the database. */
{
char* row[PSL_NUM_COLS];
struct lineFile *pslLf = gzLineFileOpen(pslPath);
while (lineFileNextRow(pslLf, row, PSL_NUM_COLS))
    {
    struct psl* psl = pslLoad(row);
    processIntronPsl(conn, select, statusTbl, psl, pslLf);
    pslFree(&psl);
    }
gzLineFileClose(&pslLf);
}

void gbAlignDataProcess(struct sqlConnection *conn, struct gbSelect* select,
                        struct gbStatusTbl* statusTbl)
/* Parse a psl file looking for accessions to add to the database.  If the
 * entry matches the status->selectAlign field, it will be saved for loading
 * and the count of aligned entries will be incremented. */
{
char pslPath[PATH_LEN];
char oiPath[PATH_LEN];

gbAlignedGetPath(select, "psl.gz", NULL, pslPath);
/* shouldn't have called this method if there no alignments counted */
if (!fileExists(pslPath))
    errAbort("PSL file does exist, yet genbank index indicates that it should: %s",
             pslPath);

processPslFile(conn, select, statusTbl, pslPath);

/* load the associated orientInfo file if native */
if (select->orgCats == GB_NATIVE)
    {
    strcpy(oiPath, pslPath);
    assert(endsWith(pslPath, ".psl.gz"));
    strcpy(oiPath + strlen(oiPath) - 7, ".oi.gz");
    processOIFile(conn, select, statusTbl, oiPath);
    }

/* for native ESTs, we might have an intronPsl file */
if ((select->type == GB_EST) && (select->orgCats == GB_NATIVE))
    {
    char intronPslPath[PATH_LEN];
    gbAlignedGetPath(select, "intronPsl.gz", NULL, intronPslPath);
    if (fileExists(intronPslPath))
        processIntronPslFile(conn, select, statusTbl, intronPslPath);
    }
}

void gbAlignDataSetStatus(struct gbStatusTbl* statusTbl)
/* Set the status entries to have the version of the alignments.
 * This is used to set the version on entries that did not align
 * and is called after scanning all of the PSLs.*/
{
struct hashCookie cookie = hashFirst(statusTbl->accHash);
struct hashEl *hel;
/* if any aligned, versions must match selected aligned. */
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct gbStatus* status = (struct gbStatus*)hel->val;
    if ((status->selectAlign != NULL) && (!(status->stateChg & GB_DELETED)))
        {
        if (status->numAligns == 0)
            status->version = status->selectAlign->version;
        else
            assert(status->version == status->selectAlign->version);
        }
    }
}

void gbAlignDataDbLoad(struct sqlConnection *conn)
/* load the alignments into the database */
{
gbGeneTblSetCommit(geneTblSet, conn);
gbAlignTblSetCommit(alignTblSet, conn);
gbGeneTblSetFree(&geneTblSet);
gbAlignTblSetFree(&alignTblSet);
}

static void deleteGenBankChromAligns(struct sqlConnection *conn,
                                     struct sqlDeleter* deleter,
                                     unsigned type, char *typeStr)
/* delete outdated genbank alignments from per-chrom tables. */
{
struct slName* chrom;
char table[64];
for (chrom = getChromNames(); chrom != NULL; chrom = chrom->next)
    {
    safef(table, sizeof(table), "%s_%s", chrom->name, typeStr);
    sqlDeleterDel(deleter, conn, table, "qName");
    if (type == GB_EST)
        {
        safef(table, sizeof(table), "%s_intronEst", chrom->name);
        sqlDeleterDel(deleter, conn, table, "qName");
        }
    }
}

static void deleteGenBankAligns(struct sqlConnection *conn,
                                struct sqlDeleter* deleter, unsigned type,
                                struct dbLoadOptions* options)
/* delete outdated genbank alignments from the database. */
{
char table[64];
char* typeStr = ((type == GB_MRNA) ? "mrna" : "est");
char *xenoTable = ((type == GB_MRNA) ? XENO_MRNA_TBL : XENO_EST_TBL);

safef(table, sizeof(table), "all_%s", typeStr);
sqlDeleterDel(deleter, conn, table, "qName");

if (options->flags & DBLOAD_PER_CHROM_ALIGN)
    deleteGenBankChromAligns(conn, deleter, type, typeStr);
else 
    {
    if (type == GB_EST)
        sqlDeleterDel(deleter, conn, INTRON_EST_TBL, "qName");
    }

sqlDeleterDel(deleter, conn, xenoTable, "qName");

safef(table, sizeof(table), "%sOrientInfo", typeStr);
sqlDeleterDel(deleter, conn, table, "name");

if (haveMgc)
    {
    sqlDeleterDel(deleter, conn, MGC_FULL_MRNA_TBL, "qName");
    sqlDeleterDel(deleter, conn, MGC_GENES_TBL, "name");
    }
if (haveOrfeome)
    {
    sqlDeleterDel(deleter, conn, ORFEOME_MRNA_TBL, "qName");
    sqlDeleterDel(deleter, conn, ORFEOME_GENES_TBL, "name");
    }
}

static void deleteRefSeqAligns(struct sqlConnection *conn,
                               struct sqlDeleter* deleter)
/* delete outdated refseq alignments from the database. */
{
sqlDeleterDel(deleter, conn, "refSeqAli", "qName");
sqlDeleterDel(deleter, conn, "refGene", "name");
sqlDeleterDel(deleter, conn, "refFlat", "name");
sqlDeleterDel(deleter, conn, "xenoRefSeqAli", "qName");
sqlDeleterDel(deleter, conn, "xenoRefGene", "name");
sqlDeleterDel(deleter, conn, "xenoRefFlat", "name");
sqlDeleterDel(deleter, conn, "mrnaOrientInfo", "name");
}

void gbAlignDataDeleteFromTables(struct sqlConnection *conn,
                                 unsigned srcDb, unsigned type,
                                 struct sqlDeleter* deleter,
                                 struct dbLoadOptions* options)
/* delete alignment data from tables */
{
if (srcDb == GB_REFSEQ)
    deleteRefSeqAligns(conn, deleter);
else
    deleteGenBankAligns(conn, deleter, type, options);
}

void gbAlignDataDeleteOutdated(struct sqlConnection *conn,
                               struct gbSelect* select, 
                               struct gbStatusTbl* statusTbl,
                               struct dbLoadOptions* options,
                               char *tmpDir)
/* delete outdated alignment data */
{
struct sqlDeleter* deleter = sqlDeleterNew(tmpDir, (gbVerbose >= 4));
struct gbStatus* status;

/* delete seqChg, deleted, and orphans from alignments; clearing count of
 * number aligned. */
for (status = statusTbl->deleteList; status != NULL; status = status->next)
    {
    sqlDeleterAddAcc(deleter, status->acc);
    status->numAligns = 0;
    }
for (status = statusTbl->seqChgList; status != NULL; status = status->next)
    {
    sqlDeleterAddAcc(deleter, status->acc);
    status->numAligns = 0;
    }
for (status = statusTbl->orphanList; status != NULL; status = status->next)
    {
    sqlDeleterAddAcc(deleter, status->acc);
    status->numAligns = 0;
    }

gbAlignDataDeleteFromTables(conn, select->release->srcDb, select->type,
                            deleter, options);

sqlDeleterFree(&deleter);
} 

static void removeRefSeq(struct sqlConnection *conn, struct gbSelect* select,
                         struct sqlDeleter* deleter)
/* delete all refseq alignments */
{
if (select->orgCats & GB_NATIVE)
    {
    sqlDropTable(conn, "refSeqAli");
    sqlDeleterDel(deleter, conn, "mrnaOrientInfo", "name");
    }
if (select->orgCats & GB_XENO)
    {
    sqlDropTable(conn, "xenoRefSeqAli");
    }
}

static void removeGenBankMrna(struct sqlConnection *conn, struct gbSelect* select,
                              struct sqlDeleter* deleter)
/* delete all genbank mRNA alignments */
{
if (select->orgCats & GB_NATIVE)
    {
    struct slName* chrom;
    char table[64];
    sqlDropTable(conn, "all_mrna");
    sqlDeleterDel(deleter, conn, "mrnaOrientInfo", "name");
    for (chrom = getChromNames(); chrom != NULL; chrom = chrom->next)
        {
        safef(table, sizeof(table), "%s_mrna", chrom->name);
        sqlDropTable(conn, table);
        }
    if (haveMgc)
        sqlDropTable(conn, "mgcFullMrna");
    if (haveOrfeome)
        sqlDropTable(conn, "orfeomeMrna");
    }
if (select->orgCats & GB_XENO)
    {
    sqlDropTable(conn, "xenoMrna");
    }
    
}

void gbAlignRemove(struct sqlConnection *conn, struct dbLoadOptions* options,
                   struct gbSelect* select, struct sqlDeleter* deleter)
/* Delete all alignments for the selected categories.  Used when reloading
 * alignments.*/
{
setDerivedTblFlags(conn, options);
/* ESTs not implemented, which gets rid of complexities of accPrefix */
if (select->type & GB_EST)
    errAbort("gbAlignRemove doesn't handle ESTs");
if (select->release->srcDb & GB_REFSEQ)
    removeRefSeq(conn, select, deleter);
if (select->release->srcDb & GB_GENBANK)
    removeGenBankMrna(conn, select, deleter);
}

struct slName* gbAlignDataListTables(struct sqlConnection *conn)
/* Get list of alignment tables from database. */
{
return slCat(gbAlignTblList(conn),
             gbGeneTblList(conn));
}
