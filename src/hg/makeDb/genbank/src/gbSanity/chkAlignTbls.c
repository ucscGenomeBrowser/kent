/* validation of all alignment-based tables */
#include "chkAlignTbls.h"
#include "chkCommon.h"
#include "metaData.h"
#include "localmem.h"
#include "gbFileOps.h"
#include "gbRelease.h"
#include "gbVerb.h"
#include "jksql.h"
#include "hdb.h"
#include "hash.h"
#include "genePred.h"
#include "gbGenome.h"
#include "../dbload/dbLoadOptions.h"
#include "psl.h"

static char const rcsid[] = "$Id: chkAlignTbls.c,v 1.12.26.1 2008/07/31 02:24:34 markd Exp $";

/* FIXME: check native vs xeno, flag in metaData. */
/* FIXME: check OI tables */

/* FIXME: remove when bugs fixes */
#define WARN_BLAT_BUGS 1

static struct slName* gChroms = NULL;
static struct hash* gChromSizes = NULL; /* table of chromsome sizes */

static void buildChromSizes(char *db)
/* build table of chromosome sizes and list of chromosomes */
{
struct sqlConnection *conn =  hAllocConn(db);
struct sqlResult *sr;
char **row;
gChromSizes = hashNew(8);

sr = sqlGetResult(conn, "SELECT chrom,size FROM chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned sz = gbParseUnsigned(NULL, row[1]);
    hashAddInt(gChromSizes, row[0], sz);
    slSafeAddHead(&gChroms, newSlName(row[0]));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}

static unsigned getChromSize(char *db, char* chrom)
/* get chromosome size, or 0 if invalid */
{
struct hashEl* hel;
if (gChromSizes == NULL)
    buildChromSizes(db);

hel = hashLookup(gChromSizes, chrom);
if (hel == NULL)
    return 0;
else
    return (unsigned)(size_t)hel->val;
}

static void chkPsl(struct psl* psl, unsigned iRow, char* database,
                   char* table, struct metaDataTbls* metaDataTbls,
                   unsigned typeFlags)
/* Validate a PSL of a mrna/est to genome alignment against the metadata.
 * Also count the number of alignments of a mrna. */
{
unsigned chromSize = getChromSize(database, psl->tName);
struct metaData* md = metaDataTblsFind(metaDataTbls, psl->qName);
char pslDesc[128];
if (gbVerbose >= 3)
    gbVerbMsg(3, "chkPsl %s:%d %s %s",  table, iRow, psl->qName, psl->tName);

safef(pslDesc, sizeof(pslDesc), "psl %s.%s row %u", database, table, iRow);

/* check that we have sequence info and compare sizes sizes */
if (chromSize == 0)
    gbError("%s: tName not a valid chromosome: \"%s\"", pslDesc, psl->tName);
else
    if (chromSize != psl->tSize)
        gbError("%s: tSize %u != chromosome %s size %u",
                pslDesc, psl->tSize, psl->tName, chromSize);

if (md == NULL)
    gbError("%s: qName not in mrna table as type %s: \"%s\"",
            pslDesc, gbFmtSelect(typeFlags & GB_TYPE_MASK), psl->qName);
else if (md->inSeq)
    {
    if (!md->inGbIndex)
        gbError("%s: qName not in gbIndex as type %s: \"%s\"", pslDesc,
                gbFmtSelect(typeFlags & GB_TYPE_MASK), psl->qName);
    else
        {
        if (typeFlags != md->typeFlags)
            gbError("%s: alignment for %s type %s doesn't match expected %s",
                    pslDesc, psl->qName, gbFmtSelect(md->typeFlags),
                    gbFmtSelect(typeFlags));
        }
    if (md->seqSize != psl->qSize)
        gbError("%s: qSize %u != %s size %u",
                pslDesc, psl->qSize, psl->qName, md->seqSize);
    md->numAligns++;
    }

/* validate consistency of PSL */
if (pslCheck(pslDesc, stderr, psl))
    errorCnt++;
}

static void chkPslTable(struct gbSelect* select, struct sqlConnection* conn,
                        char* rootTable, char* chrom,
                        struct metaDataTbls* metaDataTbls,
                        unsigned typeFlags)
/* Validate a PSL of a mrna/est to genome alignment against the metadata.  If
 * not a chromosome-specific table, chrom should be null.  Chromosome-specific
 * tables are not required to exist (for testing purposes).  Also count the
 * number of alignments of a mrna. */
{
struct hTableInfo* tableInfo;
char table[64];
unsigned iRow = 0;
unsigned rowOffset;
char accWhere[64];
char query[512];
struct sqlResult *sr;
char **row;

/* need to specify an explicit chrom table, as there is an mrna table which is
 * not psl, so using mrna as a root name with a chrom that doesn't exist
 * returns the mrna instead of null */

if (chrom != NULL)
    safef(table, sizeof(table), "%s_%s", chrom, rootTable);
else
    safef(table, sizeof(table), "%s", rootTable);

gbVerbEnter(3, "chkPslTable %s", table);

tableInfo = hFindTableInfoDb(select->release->genome->database, chrom, table);
if (tableInfo == NULL)
    {
    /* If all table, require it */
    if (chrom == NULL)
        {
        if (testMode)
            fprintf(stderr, "Warning: no psl table %s.%s\n",
                    select->release->genome->database, table);
        else
            gbError("no psl table %s.%s", select->release->genome->database,
                    table);
        }
    }
else
    {
    rowOffset = (tableInfo->hasBin) ? 1 : 0;
    accWhere[0] = '\0';
    if (select->accPrefix != NULL)
        safef(accWhere, sizeof(accWhere), " WHERE qName LIKE '%s%%'",
              select->accPrefix);
    safef(query, sizeof(query), "SELECT * FROM %s%s", table, accWhere);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        struct psl* psl = pslLoad(row+rowOffset);
        chkPsl(psl, iRow, select->release->genome->database, table,
               metaDataTbls, typeFlags);
        pslFree(&psl);
        iRow++;
        }
    sqlFreeResult(&sr);
    }
gbVerbLeave(3, "chkPslTable %s", table);
}

static void chkGenePred(struct genePred* gene, char *geneName, unsigned iRow,
                        char* database, char* table,
                        struct metaDataTbls* metaDataTbls, unsigned typeFlags)
/* Validate a genePred of a refSeq to genome alignment against the metadata.
 * Also count the number of alignments, and check the geneName, if available */
{
char desc[512];
unsigned chromSize = getChromSize(database, gene->chrom);
struct metaData* md = metaDataTblsFind(metaDataTbls, gene->name);

if (gbVerbose >= 3)
    gbVerbMsg(3, "chkGenePred %s:%d %s %s",  table, iRow, 
              gene->name, gene->chrom);
safef(desc, sizeof(desc), "gene %s.%s:%u %s %s", database, table,
      iRow, gene->name, gene->chrom);

/* basic sanity checks */
if (genePredCheck(desc, stderr, chromSize, gene))
    errorCnt++;

/* check if in mrna table */
if (md == NULL)
    gbError("%s: %s in not in mrna table", desc, gene->name);
else
    {
    if (typeFlags != md->typeFlags)
        gbError("%s: alignment of %s type %s doesn't match expected %s",
                desc, gene->name, gbFmtSelect(md->typeFlags),
                gbFmtSelect(typeFlags));
    md->numAligns++;
    }

/* check gene name */
if ((md != NULL) && (geneName != NULL))
    {
    char* rlName = (md->rlName == NULL) ? "" : md->rlName;
    if (!sameString(geneName, rlName))
        gbError("%s: %s geneName \"%s\" does not match refLink name \"%s\"",
                desc, gene->name, geneName, rlName);
    }
}

static void chkGenePredRows(struct gbSelect* select,
                             struct sqlConnection* conn,
                             char* table, boolean isRefFlat, 
                             struct metaDataTbls* metaDataTbls,
                             unsigned typeFlags)
/* check rows of genePred or refFlat table */
{
unsigned iRow = 0;
char **row;
char *geneName = NULL;

int rowOff = (isRefFlat ? 1 : 0);  /* columns to skip to genePred */
if (sqlFieldIndex(conn, table, "bin") >= 0)
    rowOff++;

char query[512];
safef(query, sizeof(query), "SELECT * FROM %s", table);
struct sqlResult *sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct genePred* gene = genePredLoad(row+rowOff);
    if (isRefFlat)
        geneName = row[0];
    chkGenePred(gene, geneName, iRow, select->release->genome->database, table,
                metaDataTbls, typeFlags);
    genePredFree(&gene);
    iRow++;
    }
sqlFreeResult(&sr);

}

static void chkGenePredTable(struct gbSelect* select,
                             struct sqlConnection* conn,
                             char* table, boolean isRefFlat, 
                             struct metaDataTbls* metaDataTbls,
                             unsigned typeFlags)
/* Validate a genePred table.  Also count the number of genePreds for a
 * mrna.  If this is refFlat, also check the geneName.  Return numbner of
 * rows. */
{
gbVerbEnter(3, "chkGenePredTable %s", table);
if (!sqlTableExists(conn, table))
    gbError("no genePred table %s.%s", select->release->genome->database,
            table);
else
    chkGenePredRows(select, conn, table, isRefFlat, metaDataTbls, typeFlags);
gbVerbLeave(3, "chkGenePredTable %s", table);
}

static void chkAlignCount(struct metaData* md, struct metaDataTbls* metaDataTbls,
                          char* dbTableDesc, unsigned typeFlags)
/* check alignment counts or gene counts for an accession if it matches the
 * type, also reset the counts. */
{
if (typeFlags == md->typeFlags)
    {
    if (md->numAligns != md->gbsNumAligns)
        gbError("%s: number of alignments found in %s (%d) does not match "
                "expected (%d) from gbStatus",
                md->acc, dbTableDesc, md->numAligns, md->gbsNumAligns);
    }
md->numAligns = 0; /* reset counts */
}

static void chkAlignCounts(struct metaDataTbls* metaDataTbls,
                           char* dbTableDesc, unsigned typeFlags)
/* check alignment counts found in tables, also reset the counts
 * Type includes GB_NATIVE or GB_XENO. */
{
struct metaData* md;

gbVerbEnter(3, "chkAlignCounts %s", dbTableDesc);

/* Traverse all metadata entries, comparing number of alignments with
 * gbStatus. */
metaDataTblsFirst(metaDataTbls);
while ((md = metaDataTblsNext(metaDataTbls)) != NULL)
    chkAlignCount(md, metaDataTbls, dbTableDesc, typeFlags);
gbVerbLeave(3, "chkAlignCounts %s", dbTableDesc);
}

static void chkNativeGenBankAlignTables(struct gbSelect* select,
                                        struct sqlConnection* conn,
                                        struct metaDataTbls* metaDataTbls,
                                        struct dbLoadOptions *options)
/* Verify native genbank PSL tables. */
{
char dbTableDesc[256];
char *chromTable = ((select->type == GB_MRNA) ? "mrna" : "est");
char *allTable = ((select->type == GB_MRNA) ? "all_mrna" : "all_est");

safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s",
      select->release->genome->database, allTable);
chkPslTable(select, conn, allTable, NULL, metaDataTbls,
            (GB_GENBANK|GB_NATIVE|select->type));
chkAlignCounts(metaDataTbls, dbTableDesc, (GB_GENBANK|GB_NATIVE|select->type));

if (options->flags & DBLOAD_PER_CHROM_ALIGN)
    {
    struct slName* chrom;
    safef(dbTableDesc, sizeof(dbTableDesc), "%s:chr*_%s",
          select->release->genome->database, chromTable);
    for (chrom = gChroms; chrom != NULL; chrom = chrom->next)
        chkPslTable(select, conn, chromTable, chrom->name, metaDataTbls,
                    (GB_GENBANK|GB_NATIVE|select->type));
    chkAlignCounts(metaDataTbls, dbTableDesc,
                   (GB_GENBANK|GB_NATIVE|select->type));
    }
}

static void chkXenoGenBankAlignTables(struct gbSelect* select,
                                      struct sqlConnection* conn,
                                      struct metaDataTbls* metaDataTbls,
                                      struct dbLoadOptions *options)
/* Verify xeno genbank PSL tables. */
{
char dbTableDesc[256];
char *xenoTable = ((select->type == GB_MRNA) ? "xenoMrna" : "xenoEst");
safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s",
      select->release->genome->database, xenoTable);
chkPslTable(select, conn, xenoTable, NULL, metaDataTbls,
            (GB_GENBANK|GB_XENO|select->type));
chkAlignCounts(metaDataTbls, dbTableDesc, (GB_GENBANK|GB_XENO|select->type));
}

static void chkGenBankAlignTables(struct gbSelect* select,
                                  struct sqlConnection* conn,
                                  struct metaDataTbls* metaDataTbls,
                                  struct dbLoadOptions *options)
/* Verify all of the PSL tables, including checking the count of
 * alignments for either mRNA or ESTs. */
{
if ((select->orgCats & GB_NATIVE)
    && dbLoadOptionsGetAttr(options, GB_GENBANK, select->type, GB_NATIVE)->load)
    {
    chkNativeGenBankAlignTables(select, conn, metaDataTbls, options);
    }
if ((select->orgCats & GB_XENO)
    && dbLoadOptionsGetAttr(options, GB_GENBANK, select->type, GB_XENO)->load)
    {
    chkXenoGenBankAlignTables(select, conn, metaDataTbls, options);
    }
}

static void chkNativeRefSeqAlignTables(struct gbSelect* select,
                                       struct sqlConnection* conn,
                                       struct metaDataTbls* metaDataTbls,
                                       struct dbLoadOptions *options)
/* Verify native refseq alignment tables */
{
char* database = select->release->genome->database;
char dbTableDesc[256];
unsigned typeFlags = GB_REFSEQ|GB_MRNA|GB_NATIVE;
safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "refSeqAli");
chkPslTable(select, conn, "refSeqAli", NULL, metaDataTbls, typeFlags);
chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);

safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "refGene");
chkGenePredTable(select, conn, "refGene", FALSE, metaDataTbls, typeFlags);
chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);

safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "refFlat");
chkGenePredTable(select, conn, "refFlat", TRUE, metaDataTbls, typeFlags);
chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);
}

static void chkXenoRefSeqAlignTables(struct gbSelect* select,
                                     struct sqlConnection* conn,
                                     struct metaDataTbls* metaDataTbls,
                                     struct dbLoadOptions *options)
/* Verify native refseq alignment tables */
{
char* database = select->release->genome->database;
char dbTableDesc[256];
unsigned typeFlags = GB_REFSEQ|GB_MRNA|GB_XENO;
safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "xenoRefSeqAli");
chkPslTable(select, conn, "xenoRefSeqAli", NULL, metaDataTbls, typeFlags);
chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);

safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "xenoRefGene");
chkGenePredTable(select, conn, "xenoRefGene", FALSE, metaDataTbls, typeFlags);
chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);

safef(dbTableDesc, sizeof(dbTableDesc), "%s:%s", database, "xenoRefFlat");
chkGenePredTable(select, conn, "xenoRefFlat", TRUE, metaDataTbls, typeFlags);
chkAlignCounts(metaDataTbls, dbTableDesc, typeFlags);
}

static void chkRefSeqAlignTables(struct gbSelect* select,
                                 struct sqlConnection* conn,
                                 struct metaDataTbls* metaDataTbls,
                                 struct dbLoadOptions *options)
/* Verify all of the refseq alignment tables, including checking the count of
 * genePred. */
{
if ((select->orgCats & GB_NATIVE)
    && dbLoadOptionsGetAttr(options, GB_REFSEQ, select->type, GB_NATIVE)->load)
    {
    chkNativeRefSeqAlignTables(select, conn, metaDataTbls, options);
    }
if ((select->orgCats & GB_XENO)
    && dbLoadOptionsGetAttr(options, GB_REFSEQ, select->type, GB_XENO)->load)
    {
    chkXenoRefSeqAlignTables(select, conn, metaDataTbls, options);
    }
}

int chkAlignTables(char *db, struct gbSelect* select, struct sqlConnection* conn,
                   struct metaDataTbls* metaDataTbls, struct dbLoadOptions *options)
/* Verify all of the alignment-related. */
{
int cnt = 0;
if (gChromSizes == NULL)
    buildChromSizes(db);
gbVerbEnter(1, "validating alignment tables: %s", gbSelectDesc(select));
if (select->release->srcDb & GB_GENBANK)
    {
    chkGenBankAlignTables(select, conn, metaDataTbls, options);
    cnt++;
    }
if (select->release->srcDb & GB_REFSEQ)
    {
    chkRefSeqAlignTables(select, conn, metaDataTbls, options);
    cnt++;
    }
gbVerbLeave(1, "validated alignment tables: %s", gbSelectDesc(select));
return cnt;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

