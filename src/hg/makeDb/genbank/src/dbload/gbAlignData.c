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
#include "gbAligned.h"
#include "gbGenome.h"
#include "gbStatusTbl.h"
#include "gbFileOps.h"
#include "gbVerb.h"
#include "sqlUpdater.h"
#include "estOrientInfo.h"
#include "gbBuildState.h"
#include "sqlDeleter.h"

static char const rcsid[] = "$Id: gbAlignData.c,v 1.1 2003/06/03 01:27:42 markd Exp $";

/* table names */
static char *REF_SEQ_ALI = "refSeqAli";
static char *REF_GENE_TBL = "refGene";
static char *REF_FLAT_TBL = "refFlat";

/* strings to create refSeq tables are put here */
static char *gRefGeneTableDef;
static char *gRefFlatTableDef = 
"CREATE TABLE refFlat ( \n"
"   geneName varchar(255) not null,	# name of gene \n"
"   name varchar(255) not null,	# mrna accession of gene \n"
"   chrom varchar(255) not null,	# Chromosome name \n"
"   strand char(1) not null,	# + or - for strand \n"
"   txStart int unsigned not null,	# Transcription start position \n"
"   txEnd int unsigned not null,	# Transcription end position \n"
"   cdsStart int unsigned not null,	# Coding region start \n"
"   cdsEnd int unsigned not null,	# Coding region end \n"
"   exonCount int unsigned not null,	# Number of exons \n"
"   exonStarts longblob not null,	# Exon start positions \n"
"   exonEnds longblob not null,	# Exon end positions \n"
          "   #Indices \n"
"   INDEX(geneName(10)), \n"
"   INDEX(name(10)), \n"
"   INDEX(chrom(12),txStart), \n"
"   INDEX(chrom(12),txEnd) \n"
")";


/* global conf */
static char gTmpDir[PATH_LEN];
static boolean gBuildPerChrom = FALSE;   /* build per-chrom tables */
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
static struct sqlUpdater* gRefGeneUpd = NULL;
static struct sqlUpdater* gRefFlatUpd = NULL;

void gbAlignDataInit(char *tmpDirPath, boolean noPerChrom)
/* initialize for outputing PSL files, called once per genbank type */
{
strcpy(gTmpDir, tmpDirPath);
gBuildPerChrom = !noPerChrom;
if (gChroms == NULL)
    gChroms = hAllChromNames();

/* get sql to create tables for first go */
if (gRefGeneTableDef == NULL)
    {
    char editDef[1024], *tmpDef, *part2, *p;
    gRefGeneTableDef = genePredGetCreateSql(REF_GENE_TBL, 0);
    
    /* edit generated SQL to add geneName column and index */
    tmpDef = genePredGetCreateSql(REF_FLAT_TBL, 0);
    part2 = strchr(tmpDef, '(');
    *(part2++) = '\0';
    p = strrchr(part2, ')');
    *p = '\0';
    safef(editDef, sizeof(editDef),
          "%s(geneName varchar(255) not null, %s, INDEX(geneName(10)))",
          tmpDef, part2);
    gRefFlatTableDef = cloneString(editDef);
    free(tmpDef);
    }
}
static FILE* getPslTabFile(char* table, struct sqlConnection* conn,
                           struct sqlUpdater **tabFileVar)
/* Get the tab file for a combined PSL table, opening if necessary. */
{
if (*tabFileVar == NULL)
    {
    *tabFileVar = sqlUpdaterNew(table, gTmpDir, (verbose >= 2), &gAllUpdaters);
    if (!sqlTableExists(conn, table))
        {
        /* create with tName index and bin */
        char *sqlCmd = pslGetCreateSql(table, (PSL_TNAMEIX|PSL_WITH_BIN));
        sqlRemakeTable(conn, table, sqlCmd);
        freez(&sqlCmd);
        }
    }
return sqlUpdaterGetFh(*tabFileVar, 1);
}

static FILE* getChromPslTabFile(char* rootTable, char* chrom,
                                struct hash ** chromHashPtr,
                                struct sqlConnection* conn)
/* get the tab file for a per-chrom psl, opening if necessary */
{
struct hashEl* hel;
if (*chromHashPtr == NULL)
    *chromHashPtr = hashNew(8);
hel = hashLookup(*chromHashPtr, chrom);
if (hel == NULL)
    {
    char table[64];
    struct sqlUpdater* tabFile;
    safef(table, sizeof(table), "%s_%s", chrom, rootTable);
    tabFile = sqlUpdaterNew(table, gTmpDir, (verbose >= 2), &gAllUpdaters);
    hel = hashAdd(*chromHashPtr, chrom, tabFile);
    if (!sqlTableExists(conn, table))
        {
        /* create with bin */
        char *sqlCmd = pslGetCreateSql(table, PSL_WITH_BIN);
        sqlRemakeTable(conn, table, sqlCmd);
        freez(&sqlCmd);
        }
    }
return sqlUpdaterGetFh((struct sqlUpdater*)hel->val, 1);
}

static FILE* getOITabFile(char* table, struct sqlConnection* conn,
                          struct sqlUpdater **tabFileVar)
/* Get the tab file for an estOrientInfo table, opening if necessary. */
{
if (*tabFileVar == NULL)
    {
    *tabFileVar = sqlUpdaterNew(table, gTmpDir, (verbose >= 2), &gAllUpdaters);
    if (!sqlTableExists(conn, table))
        {
        char *createSql = estOrientInfoGetCreateSql(table);
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
    *tabFileVar = sqlUpdaterNew(table, gTmpDir, (verbose >= 2), &gAllUpdaters);
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
if ((status->entry->orgCat == GB_NATIVE) && gBuildPerChrom)
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

static void writeRefSeqPsl(struct sqlConnection *conn, struct gbSelect* select,
                           struct gbStatus* status, int version,
                           struct psl* psl)
/* write a refseq PSL */
{
/* only native refseq have been selected */
struct genePred* gene;
FILE *fh = getPslTabFile(REF_SEQ_ALI, conn, &gRefSeqAliUpd);
writePsl(fh, psl);

/* genePred and geneFlat tables, merge insert <= 5 bases */
gene = genePredFromPsl(psl, status->cdsStart, status->cdsEnd, 5);
fh= getOtherTabFile(REF_GENE_TBL, conn, gRefGeneTableDef, &gRefGeneUpd);
genePredTabOut(gene, fh);

fh = getOtherTabFile(REF_FLAT_TBL, conn, gRefFlatTableDef, &gRefFlatUpd);
fprintf(fh, "%s\t", ((status->geneName == NULL) ? "" : status->geneName));
genePredTabOut(gene, fh);

genePredFree(&gene);

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
    if (gBuildPerChrom)
        fh = getChromPslTabFile("intronEst", psl->tName, &gIntronEstPsls, conn);
    else
        fh = getPslTabFile("intronEst", conn, &gIntronEstUpd);
    writePsl(fh, psl);
    }
}

static void processIntrolPslFile(struct sqlConnection *conn,
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
char* geneName = (status->geneName == NULL) ? "" : status->geneName;
geneName = sqlEscapeString(geneName);

/* creates updater if it doesn't exist */
getOtherTabFile("refFlat", conn, gRefFlatTableDef, &gRefFlatUpd);
sqlUpdaterModRow(gRefFlatUpd, status->numAligns,
                   "geneName='%s' WHERE name='%s'", geneName, status->acc);
free(geneName);
}

static void updateRefFlat(struct sqlConnection *conn, struct gbSelect* select,
                          struct gbStatusTbl* statusTbl)
/* Update the gene names in the refFlat if the marked as metaChanged.
 * The metaChanged list is used, as a sequence change will cause refFlat
 * be be deleted and recreated */
{
struct gbStatus* status;
for (status = statusTbl->metaChgList; status != NULL; status = status->next)
    updateRefFlatEntry(conn, select, status);
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

/* get the associated orientInfo file */
strcpy(oiPath, pslPath);
assert(endsWith(pslPath, ".psl.gz"));
strcpy(oiPath + strlen(oiPath) - 7, ".oi.gz");
processOIFile(conn, select, statusTbl, oiPath);

/* for native ESTs, we might have an intronPsl file */
if ((select->type == GB_EST) && (select->orgCats == GB_NATIVE))
    {
    char intronPslPath[PATH_LEN];
    gbAlignedGetPath(select, "intronPsl.gz", NULL, intronPslPath);
    if (fileExists(intronPslPath))
        processIntrolPslFile(conn, select, statusTbl, intronPslPath);
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
gRefSeqAliUpd = NULL;
gRefGeneUpd = NULL;
gRefFlatUpd = NULL;
}

static void deleteGenBankAligns(struct sqlConnection *conn,
                                struct sqlDeleter* deleter, unsigned type)
/* delete outdated genbank alignments from the database. */
{
char table[64];
char* typeStr = ((type == GB_MRNA) ? "mrna" : "est");
char *xenoTable = ((type == GB_MRNA) ? "xenoMrna" : "xenoEst");
struct slName* chrom = gChroms;

safef(table, sizeof(table), "all_%s", typeStr);
sqlDeleterDel(deleter, conn, table, "qName");

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
struct sqlDeleter* deleter = sqlDeleterNew(tmpDirPath, (verbose >= 2));
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

void gbAlignDataDrop(struct sqlConnection *conn)
/* Drop alignment tables from database. */
{
char table[64];
struct slName* chrom;
if (gChroms == NULL)
    gChroms = hAllChromNames();

/* genebank tables */
sqlDropTable(conn, "all_mrna");
sqlDropTable(conn, "all_est");
sqlDropTable(conn, "xenoMrna");
sqlDropTable(conn, "xenoEst");

for (chrom = gChroms; chrom != NULL; chrom = chrom->next)
    {
    safef(table, sizeof(table), "%s_mrna", chrom->name);
    sqlDropTable(conn, table);
    safef(table, sizeof(table), "%s_est", chrom->name);
    sqlDropTable(conn, table);
    safef(table, sizeof(table), "%s_intronEst", chrom->name);
    sqlDropTable(conn, table);
    }
sqlDropTable(conn, "mrnaOrientInfo");
sqlDropTable(conn, "estOrientInfo");

sqlDropTable(conn, "refSeqAli");
sqlDropTable(conn, "refGene");
sqlDropTable(conn, "refFlat");
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

