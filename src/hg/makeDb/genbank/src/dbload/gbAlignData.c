/* Sorting and loading of PSL align records into the database.  This module
 * functions as a singlton object, with global state that can be reset.
 * Refer to the doc/database-update-step.html before modifying. */
#include "gbAlignData.h"
#include "common.h"
#include "errabort.h"
#include "hash.h"
#include "portable.h"
#include "linefile.h"
#include "jksql.h"
#include "psl.h"
#include "nib.h"
#include "hdb.h"
#include "genePred.h"
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

static char const rcsid[] = "$Id: gbAlignData.c,v 1.25 2006/06/03 07:34:25 markd Exp $";

/* table names */
static char *REF_SEQ_ALI = "refSeqAli";
static char *REF_GENE_TBL = "refGene";
static char *REF_FLAT_TBL = "refFlat";
static char *XENO_REF_SEQ_ALI = "xenoRefSeqAli";
static char *XENO_REF_GENE_TBL = "xenoRefGene";
static char *XENO_REF_FLAT_TBL = "xenoRefFlat";

/* tables to make larger than 4gb */
static char *gLargeTables[] = {
    "xenoEst", NULL
};

/* information on RefSeq genePred tables */
struct refSeqTblInfo {
    char *tbl;             /* table name */
    char *createSql;       /* SQL for creating table */
    char *flatTbl;         /* flat table name */
    char *flatCreateSql;   /* SQL for creating refFlat table */
    boolean hasBin;        /* table has or will have bin column */
    boolean hasExtCols;    /* table has or will have exonFrames, etc columns */
    struct sqlUpdater* upd; /* updater for table */
    struct sqlUpdater* flatUpd; /* updater for flat table */
    
};

/* strings to create refSeq tables are put here */
static struct refSeqTblInfo *gRefGeneInfo = NULL;
static struct refSeqTblInfo *gXenoRefGeneInfo = NULL;

/* global conf */
static struct dbLoadOptions* gOptions; /* options from cmdline and conf */
static char gTmpDir[PATH_LEN];
static struct slName* gChroms = NULL;  /* global list of chromsome */

/* objects used to build tab files */
static struct sqlUpdater* gAllUpdaters = NULL;  /* open tab files */

/* hash tables of per-chromosome sqlUpdater objects */
static struct hash* gPerChromMRnaPsls = NULL;
static struct hash* gPerChromEstPsls = NULL;
static struct hash* gIntronEstPsls = NULL;

/* genome-wide sqlUpdater objects */
static struct sqlUpdater* gNativeMrnaUpd = NULL;
static struct sqlUpdater* gNativeEstUpd = NULL;
static struct sqlUpdater* gXenoMrnaUpd = NULL;
static struct sqlUpdater* gXenoEstUpd = NULL;
static struct sqlUpdater* gMrnaOIUpd = NULL;
static struct sqlUpdater* gEstOIUpd = NULL;
static struct sqlUpdater* gIntronEstUpd = NULL;  /* when doing single table */
static struct sqlUpdater* gRefSeqAliUpd = NULL;
static struct sqlUpdater* gXenoRefSeqAliUpd = NULL;

static boolean isLargeTable(char *table)
/* see if a table is in the large tables list */
{
int i;
for (i = 0; gLargeTables[i] != NULL; i++)
    {
    if (sameString(table, gLargeTables[i]))
        return TRUE;
    }
return FALSE;
}

void makeRefGeneCreateSql(struct refSeqTblInfo *tblInfo)
/* generate the string for create one of the refGene/refFlat pair tables */
{
char editDef[1024], *tmpDef, *part2, *p;
unsigned optFields = 0, options = 0;
if (tblInfo->hasBin)
    options |= genePredWithBin;
if (tblInfo->hasExtCols)
    optFields |= genePredAllFlds;

tblInfo->createSql = genePredGetCreateSql(tblInfo->tbl, optFields, options, hGetMinIndexLength());
    
/* edit generated SQL to add geneName column and index */
tmpDef = genePredGetCreateSql(tblInfo->flatTbl, 0, 0, hGetMinIndexLength());
part2 = strchr(tmpDef, '(');
*(part2++) = '\0';
p = strrchr(part2, ')');
*p = '\0';
safef(editDef, sizeof(editDef),
      "%s(geneName varchar(255) not null, %s, INDEX(geneName(10)))",
      tmpDef, part2);
tblInfo->flatCreateSql = cloneString(editDef);
free(tmpDef);
}

void checkRefSeqTblOpts(struct sqlConnection *conn, struct refSeqTblInfo *tblInfo)
/* determine if a refSeq genePred table has a bin and/or extended columns.  Since
 * it was a little ugly to slot this check into the lazy creation of the
 * tables, an existing table is checked and non-existant one assume new tables
 * are created with bin and extended columns. Returns true if extended stuff
 * is available */
{
if (!sqlTableExists(conn, tblInfo->tbl))
    {
    tblInfo->hasBin = TRUE;
    tblInfo->hasExtCols = TRUE;
    }
else
    {
    tblInfo->hasBin = (sqlFieldIndex(conn, tblInfo->tbl, "bin") >= 0);
    tblInfo->hasExtCols = (sqlFieldIndex(conn, tblInfo->tbl, "exonFrames") >= 0);
    }
}

struct refSeqTblInfo *getRefTblInfo(char *geneTable, char *flatTable)
/* get information about a table */
{
struct sqlConnection *conn = hAllocConn();
struct refSeqTblInfo *tblInfo;
AllocVar(tblInfo);
tblInfo->tbl = cloneString(geneTable);
tblInfo->flatTbl = cloneString(flatTable);
checkRefSeqTblOpts(conn, tblInfo);
makeRefGeneCreateSql(tblInfo);
hFreeConn(&conn);
return tblInfo;
}

void gbAlignDataInit(char *tmpDirPath, struct dbLoadOptions* options)
/* initialize for outputing PSL files, called once per genbank type.
 * tmpDirPath can be null to setup just for deletion.*/
{
gOptions = options;
if (tmpDirPath != NULL)
    strcpy(gTmpDir, tmpDirPath);
if (gChroms == NULL)
    gChroms = hAllChromNames();

/* get sql to create tables for first go, always get xeno, even if not
 * used  */
if (gRefGeneInfo == NULL)
    {
    gRefGeneInfo = getRefTblInfo(REF_GENE_TBL, REF_FLAT_TBL);
    gXenoRefGeneInfo = getRefTblInfo(XENO_REF_GENE_TBL, XENO_REF_FLAT_TBL);
    }
}

static void createPslTable(struct sqlConnection *conn, char* table,
                           unsigned options)
/* create a PSL table. Will create a over 4gb table if in gLargeTables */
{
int tNameIdxLen = (options & PSL_TNAMEIX) ? hGetMinIndexLength() : 0;
char *sqlCmd = pslGetCreateSql(table, options, tNameIdxLen);
char extra[64];

if (isLargeTable(table))
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

static FILE* getPslTabFile(char* table, struct sqlConnection* conn,
                           struct sqlUpdater **tabFileVar)
/* Get the tab file for a combined PSL table, opening if necessary. */
{
if (*tabFileVar == NULL)
    {
    *tabFileVar = sqlUpdaterNew(table, gTmpDir, (gbVerbose >= 4), &gAllUpdaters);
    if (!sqlTableExists(conn, table))
        {
        /* create with tName index and bin */
        createPslTable(conn, table, (PSL_TNAMEIX|PSL_WITH_BIN));
        }
    }
return sqlUpdaterGetFh(*tabFileVar, 1);
}

static void createPerChromPslTables(char* rootTable, struct sqlConnection* conn)
/* create per-chromosome PSL tables if they don't exist.  This creates tables
 * for all chromosomes, as this makes various queries easier (table browser
 * depends on this). */
{
struct slName* chrom;
char table[64];
for (chrom = gChroms; chrom != NULL; chrom = chrom->next)
    {
    safef(table, sizeof(table), "%s_%s", chrom->name, rootTable);
    if (!sqlTableExists(conn, table))
        {
        /* create with bin */
        createPslTable(conn, table, PSL_WITH_BIN);
        }
    }
}

static FILE* getChromPslTabFile(char* rootTable, char* chrom,
                                struct hash ** chromHashPtr,
                                struct sqlConnection* conn)
/* get the tab file for a per-chrom psl, opening if necessary */
{
struct hashEl* hel;
if (*chromHashPtr == NULL)
    {
    *chromHashPtr = hashNew(12);
    createPerChromPslTables(rootTable, conn);
    }
hel = hashLookup(*chromHashPtr, chrom);
if (hel == NULL)
    {
    char table[64];
    struct sqlUpdater* tabFile;
    safef(table, sizeof(table), "%s_%s", chrom, rootTable);
    tabFile = sqlUpdaterNew(table, gTmpDir, (gbVerbose >= 4), &gAllUpdaters);
    hel = hashAdd(*chromHashPtr, chrom, tabFile);
    }
return sqlUpdaterGetFh((struct sqlUpdater*)hel->val, 1);
}

static FILE* getOITabFile(char* table, struct sqlConnection* conn,
                          struct sqlUpdater **tabFileVar)
/* Get the tab file for an estOrientInfo table, opening if necessary. */
{
if (*tabFileVar == NULL)
    {
    *tabFileVar = sqlUpdaterNew(table, gTmpDir, (gbVerbose >= 4), &gAllUpdaters);
    if (!sqlTableExists(conn, table))
        {
        char *createSql = estOrientInfoGetCreateSql(table, hGetMinIndexLength());
        sqlRemakeTable(conn, table, createSql);
        freez(&createSql);
        }
    }
return sqlUpdaterGetFh(*tabFileVar, 1);
}

static FILE* getOtherTabFile(char* table, struct sqlConnection* conn,
                             char *createSql, struct sqlUpdater **tabFileVar)
/* Get the tab file for some table type, opening if necessary. */
{
if (*tabFileVar == NULL)
    {
    *tabFileVar = sqlUpdaterNew(table, gTmpDir, (gbVerbose >= 4), &gAllUpdaters);
    if (!sqlTableExists(conn, table))
        sqlRemakeTable(conn, table, createSql);
    }
return sqlUpdaterGetFh(*tabFileVar, 1);
}

static void writePsl(FILE* outFh, struct psl* psl)
/* write a psl with bin */
{
/* FIXME: this looked liked a BLAT bug that may have been fixed 
 * see ../blat-bug */
if ((psl->qBaseInsert < 0) || (psl->tBaseInsert < 0))
    {
    fprintf(stderr, "BAD psl: ");
    pslTabOut(psl, stderr);
    if (psl->qBaseInsert < 0)
        psl->qBaseInsert = 0;
    if (psl->tBaseInsert < 0)
        psl->tBaseInsert = 0;
    }

fprintf(outFh, "%u\t", hFindBin(psl->tStart, psl->tEnd));
pslTabOut(psl, outFh);
}

static void writeGenBankPsl(struct sqlConnection *conn, struct gbSelect* select,
                            struct gbStatus* status, int version,
                            struct psl* psl)
/* write a genbank PSL */
{
FILE *fh = NULL;

/* genome-wide table */
switch (select->type|status->entry->orgCat) {
case GB_NATIVE|GB_MRNA:
    fh = getPslTabFile("all_mrna", conn, &gNativeMrnaUpd);
    break;
case GB_NATIVE|GB_EST:
    fh = getPslTabFile("all_est", conn, &gNativeEstUpd);
    break;
case GB_XENO|GB_MRNA:
    fh = getPslTabFile("xenoMrna", conn, &gXenoMrnaUpd);
    break;
case GB_XENO|GB_EST:
    fh = getPslTabFile("xenoEst", conn, &gXenoEstUpd);
    break;
}
writePsl(fh, psl);

/* per-chromosome for table for native */
if ((status->entry->orgCat == GB_NATIVE) && 
    (gOptions->flags & DBLOAD_PER_CHROM_ALIGN))
    {
    if (select->type == GB_MRNA)
        fh = getChromPslTabFile("mrna", psl->tName, &gPerChromMRnaPsls, conn);
    else
        fh = getChromPslTabFile("est", psl->tName, &gPerChromEstPsls, conn);
    writePsl(fh, psl);
    }

status->version = version;
status->numAligns++;
}

static void writeRefSeqGenePred(struct sqlConnection *conn, struct gbStatus* status,
                                struct psl* psl, struct refSeqTblInfo *tblInfo)
/* create and write genePred row (include refFlat tables) */
{
struct genePred* gene;
unsigned optFields = 0;
FILE *fh;

if (tblInfo->hasExtCols)
    optFields |= genePredAllFlds;

/* refGene */
gene = genePredFromPsl3(psl, &status->cds, optFields, 0,
                        genePredStdInsertMergeSize, genePredStdInsertMergeSize);
fh = getOtherTabFile(tblInfo->tbl, conn, tblInfo->createSql, &tblInfo->upd);
if (tblInfo->hasExtCols)
    {
    /* add gene name */
    freeMem(gene->name2);
    gene->name2 = cloneString(status->geneName);
    }
if (tblInfo->hasBin)
    fprintf(fh, "%u\t", hFindBin(gene->txStart, gene->txEnd));
genePredTabOut(gene, fh);
genePredFree(&gene);

/* refFlat */
gene = genePredFromPsl3(psl, &status->cds, 0, 0,
                        genePredStdInsertMergeSize, genePredStdInsertMergeSize);
fh = getOtherTabFile(tblInfo->flatTbl, conn, tblInfo->flatCreateSql, &tblInfo->flatUpd);
fprintf(fh, "%s\t", ((status->geneName == NULL) ? "" : status->geneName));
genePredTabOut(gene, fh);
genePredFree(&gene);
}

static void writeRefSeqPsl(struct sqlConnection *conn, struct gbSelect* select,
                           struct gbStatus* status, int version,
                           struct psl* psl)
/* write a refseq PSL and associated genePred */
{
FILE *fh;

if (select->orgCats == GB_NATIVE)
    fh = getPslTabFile(REF_SEQ_ALI, conn, &gRefSeqAliUpd);
else
    fh = getPslTabFile(XENO_REF_SEQ_ALI, conn, &gXenoRefSeqAliUpd);
writePsl(fh, psl);

if (select->orgCats == GB_NATIVE)
    writeRefSeqGenePred(conn, status, psl, gRefGeneInfo);
else
    writeRefSeqGenePred(conn, status, psl, gXenoRefGeneInfo);

status->version = version;
status->numAligns++;
}

static void processPsl(struct sqlConnection *conn, struct gbSelect* select,
                       struct gbStatusTbl* statusTbl, struct psl* psl,
                       struct lineFile *pslLf)
/* Parse and process the next PSL entry */
{
char acc[GB_ACC_BUFSZ];
short version;
struct gbStatus* status;

/* qName has accession with version */
version = gbSplitAccVer(psl->qName, acc);
if (version < 0)
    errAbort("PSL entry qName \"%s\" is not a genback accession with version: %s:%d",
             psl->qName, pslLf->fileName, pslLf->lineIx);

status = gbStatusTblFind(statusTbl, acc);
if ((status != NULL) && (status->selectAlign != NULL)
    && (status->selectAlign->version == version))
    {
    /* replace with acc without version */
    strcpy(psl->qName, acc);
    if (select->release->srcDb == GB_REFSEQ)
        writeRefSeqPsl(conn, select, status, version, psl);
    else
        writeGenBankPsl(conn, select, status, version, psl);
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
/* Parse and process the next PSL entry */
{
char acc[GB_ACC_BUFSZ];
short version;
struct gbStatus* status;
FILE *fh;

/* qName has accession with version */
version = gbSplitAccVer(oi->name, acc);
if (version < 0)
    errAbort("orientInfo entry \"%s\" is not a genback accession with version: %s:%d",
             oi->name, oiLf->fileName, oiLf->lineIx);

status = gbStatusTblFind(statusTbl, acc);
if ((status != NULL) && (status->selectAlign != NULL)
    && (status->selectAlign->version == version))
    {
    /* replace with acc without version */
    strcpy(oi->name, acc);
    if (select->type == GB_MRNA)
        fh = getOITabFile("mrnaOrientInfo", conn, &gMrnaOIUpd);
    else
        fh = getOITabFile("estOrientInfo", conn, &gEstOIUpd);
    fprintf(fh, "%u\t", hFindBin(oi->chromStart, oi->chromEnd));
    estOrientInfoTabOut(oi, fh);
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
/* Parse and process the next intron PSL entry */
{
char acc[GB_ACC_BUFSZ];
short version;
struct gbStatus* status;
FILE *fh;

/* qName has accession with version */
version = gbSplitAccVer(psl->qName, acc);
if (version < 0)
    errAbort("PSL entry qName \"%s\" is not a genback accession with version: %s:%d",
             psl->qName, pslLf->fileName, pslLf->lineIx);

status = gbStatusTblFind(statusTbl, acc);
if ((status != NULL) && (status->selectAlign != NULL)
    && (status->selectAlign->version == version))
    {
    /* replace with acc without version */
    strcpy(psl->qName, acc);
    if (gOptions->flags & DBLOAD_PER_CHROM_ALIGN)
        fh = getChromPslTabFile("intronEst", psl->tName, &gIntronEstPsls, conn);
    else
        fh = getPslTabFile("intronEst", conn, &gIntronEstUpd);
    writePsl(fh, psl);
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

static void updateRefFlatEntry(struct sqlConnection *conn,
                               struct gbSelect* select, 
                               struct gbStatus* status)
/* Update the gene names in the refFlat for an entry */
{
struct refSeqTblInfo *tblInfo;
char* geneName = (status->geneName == NULL) ? "" : status->geneName;
char *buf = needMem(2*strlen(geneName)+1);
geneName = sqlEscapeString2(buf, geneName);


if (status->entry->orgCat == GB_NATIVE)
    tblInfo = gRefGeneInfo;
else
    tblInfo = gXenoRefGeneInfo;

getOtherTabFile(tblInfo->flatTbl, conn, tblInfo->flatCreateSql,
                &tblInfo->flatUpd);
sqlUpdaterModRow(tblInfo->flatUpd, status->numAligns,
                 "geneName='%s' WHERE name='%s'", geneName, status->acc);
freeMem(buf);
}

static void updateRefFlat(struct sqlConnection *conn, struct gbSelect* select,
                          struct gbStatusTbl* statusTbl)
/* Update the gene names in the refFlat if the marked as metaChanged.
 * The metaChanged list is used, as a sequence change will cause refFlat
 * be be deleted and recreated */
{
struct gbStatus* status;
for (status = statusTbl->metaChgList; status != NULL; status = status->next)
    {
    if (status->selectProc->update == select->update)
        updateRefFlatEntry(conn, select, status);
    }
}

void gbAlignDataProcess(struct sqlConnection *conn, struct gbSelect* select,
                        struct gbStatusTbl* statusTbl)
/* Parse a psl file looking for accessions to add to the database.  If the n
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

/* refFlat table has is a join of metaData (geneName) with alignment, so we
 * may need to update individual entries */
if (select->release->srcDb == GB_REFSEQ)
    updateRefFlat(conn, select, statusTbl);
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
struct sqlUpdater *nextUpd;

while ((nextUpd = slPopHead(&gAllUpdaters)) != NULL)
    {
    sqlUpdaterCommit(nextUpd, conn);
    sqlUpdaterFree(&nextUpd);
    }
    
hashFree(&gPerChromMRnaPsls);
hashFree(&gPerChromEstPsls);
hashFree(&gIntronEstPsls);
gNativeMrnaUpd = NULL;
gNativeEstUpd = NULL;
gXenoMrnaUpd = NULL;
gXenoEstUpd = NULL;
gMrnaOIUpd = NULL;
gEstOIUpd = NULL;
gIntronEstUpd = NULL;
gRefSeqAliUpd = NULL;
gXenoRefSeqAliUpd = NULL;
if (gRefGeneInfo != NULL)
    gRefGeneInfo->upd = NULL;
if (gXenoRefGeneInfo != NULL)
    gXenoRefGeneInfo->upd = NULL;
}

static void deleteGenBankChromAligns(struct sqlConnection *conn,
                                     struct sqlDeleter* deleter,
                                     unsigned type, char *typeStr)
/* delete outdated genbank alignments from per-chrom tables. */
{
struct slName* chrom = gChroms;
char table[64];
while (chrom != NULL)
    {
    safef(table, sizeof(table), "%s_%s", chrom->name, typeStr);
    sqlDeleterDel(deleter, conn, table, "qName");
    if (type == GB_EST)
        {
        safef(table, sizeof(table), "%s_intronEst", chrom->name);
        sqlDeleterDel(deleter, conn, table, "qName");
        }
    chrom = chrom->next;
    }
}

static void deleteGenBankAligns(struct sqlConnection *conn,
                                struct sqlDeleter* deleter, unsigned type)
/* delete outdated genbank alignments from the database. */
{
char table[64];
char* typeStr = ((type == GB_MRNA) ? "mrna" : "est");
char *xenoTable = ((type == GB_MRNA) ? "xenoMrna" : "xenoEst");

safef(table, sizeof(table), "all_%s", typeStr);
sqlDeleterDel(deleter, conn, table, "qName");

if (gOptions->flags & DBLOAD_PER_CHROM_ALIGN)
    deleteGenBankChromAligns(conn, deleter, type, typeStr);
else 
    {
    if (type == GB_EST)
        sqlDeleterDel(deleter, conn, "intronEst", "qName");
    }

sqlDeleterDel(deleter, conn, xenoTable, "qName");

safef(table, sizeof(table), "%sOrientInfo", typeStr);
sqlDeleterDel(deleter, conn, table, "name");
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
                                 struct sqlDeleter* deleter)
/* delete alignment data from tables */
{
if (gChroms == NULL)
    gChroms = hAllChromNames();
if (srcDb == GB_REFSEQ)
    deleteRefSeqAligns(conn, deleter);
else
    deleteGenBankAligns(conn, deleter, type);
}

void gbAlignDataDeleteOutdated(struct sqlConnection *conn,
                               struct gbSelect* select, 
                               struct gbStatusTbl* statusTbl,
                               char *tmpDirPath)
/* delete outdated alignment data */
{
struct sqlDeleter* deleter = sqlDeleterNew(tmpDirPath, (gbVerbose >= 4));
struct gbStatus* status;
strcpy(gTmpDir, tmpDirPath);

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
                            deleter);

sqlDeleterFree(&deleter);
} 

static void removeRefSeq(struct sqlConnection *conn, struct gbSelect* select,
                         struct sqlDeleter* deleter)
/* delete all refseq alignments */
{
if (select->orgCats & GB_NATIVE)
    {
    sqlDropTable(conn, "refSeqAli");
    sqlDropTable(conn, "refGene"); 
    sqlDropTable(conn, "refFlat");
    sqlDeleterDel(deleter, conn, "mrnaOrientInfo", "name");
    }
if (select->orgCats & GB_XENO)
    {
    sqlDropTable(conn, "xenoRefSeqAli");
    sqlDropTable(conn, "xenoRefGene");
    sqlDropTable(conn, "xenoRefFlat");
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
    if (gChroms == NULL)
        gChroms = hAllChromNames();
    sqlDropTable(conn, "all_mrna");
    sqlDeleterDel(deleter, conn, "mrnaOrientInfo", "name");
    for (chrom = gChroms; chrom != NULL; chrom = chrom->next)
        {
        safef(table, sizeof(table), "%s_mrna", chrom->name);
        sqlDropTable(conn, table);
        }
    }
if (select->orgCats & GB_XENO)
    {
    sqlDropTable(conn, "xenoMrna");
    }
    
}

void gbAlignRemove(struct sqlConnection *conn, struct gbSelect* select,
                   struct sqlDeleter* deleter)
/* Delete all alignments for the selected categories.  Used when reloading
 * alignments.*/
{
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
/* includes table that exist only when perChrom tables not created */
static char* singleTableNames[] = {
    "all_mrna", "all_est", "intronEst", "xenoMrna", "xenoEst",
    "mrnaOrientInfo", "estOrientInfo", "refSeqAli", "refGene", "refFlat",
    "xenoRefSeqAli", "xenoRefGene", "xenoRefFlat",
    NULL
};

/* patterns for per-chrom tables */
static char *perChromTableLikes[] = {
    "%_mrna", "%_est", "%_intronEst",
    NULL
};
struct slName* tables = NULL;
int i;

for (i = 0; singleTableNames[i] != NULL; i++)
    gbAddTableIfExists(conn, singleTableNames[i], &tables);

for (i = 0; perChromTableLikes[i] != NULL; i++)
    {
    struct slName* tbls = gbSqlListTablesLike(conn, perChromTableLikes[i]);
    struct slName* tbl;
    for (tbl = tbls; tbl != NULL; tbl = tbl->next)
        if (!(sameString(tbl->name, "all_mrna") || sameString(tbl->name, "all_est")))
            gbAddTableIfExists(conn, tbl->name, &tables);
    slFreeList(&tbls);
    }
return tables;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

