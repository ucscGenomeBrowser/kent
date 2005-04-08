/* mkCcdsKgMap - create mappings between ccds and known genes */

#include "common.h"
#include "options.h"
#include "genePred.h"
#include "genePredReader.h"
#include "jksql.h"
#include "ccdsInfo.h"
#include "ccdsKgMap.h"
#include "linefile.h"
#include "verbose.h"
#include "chromBins.h"
#include "binRange.h"
#include "bits.h"
#include "ccdsCommon.h"

static char const rcsid[] = "$Id: mkCcdsKgMap.c,v 1.1 2005/04/08 09:02:41 markd Exp $";

static struct optionSpec optionSpecs[] =
/* command line option specifications */
{
    {"db", OPTION_STRING},
    {"loadDb", OPTION_BOOLEAN},
    {"keep", OPTION_BOOLEAN},
    {"similarity", OPTION_FLOAT},
    {NULL, 0}
};
boolean keep = FALSE;    /* keep tab files after load */
boolean loadDb = FALSE;  /* load database */
float similarity = 0.95; /* fraction similarity */

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("Error: %s\n"
    "mkCcdsKgMap - create with mappings between ccds and known genes\n"
    "\n"
    "Usage:\n"
    "   mkCcdsKgMap [options] ccdsTblFile knownGeneTblFile ccdsKgMapOut\n"
    "\n"
    "Create a mapping between CCDS and Known Genes based on similarity\n"
    "of the genomic location of the CDS.  The ccdsTbl, and knownGeneTbl\n"
    "arguments are genePreds and can be tables or files. The ccdsKgMapOut\n"
    "the name of the tab file to create to load the database, it's base name\n"
    "is also the table name.  If a simple table name is supplied, a .tab extenions is\n"
    "added.  The table is first loaded with an _tmp extension and then renamed\n"
    "if the load succeeds.\n"
    "\n"
    "Options:\n"
    "  -db=db - database if tables are being read or loaded\n"
    "  -similarity=0.95 - require at least this level of similar\n"
    "  -loadDb - load table into db\n"
    "  -keep - keep tmp tab file used to load database\n"
    "  -verbose=n\n",
    msg);
}

struct genePredReader *openGenePred(struct sqlConnection *conn,
                                     char *gpTblFile)
/* create a genePredReader, determiniong if a file or table is used for
 * input. */
{
if (fileExists(gpTblFile))
    return genePredReaderFile(gpTblFile, NULL);
else
    {
    if (conn == NULL)
        errAbort("database not supplied and %s is not a file", gpTblFile);
    return genePredReaderQuery(conn, gpTblFile, NULL);
    }
}

struct chromBins* loadGenePreds(struct sqlConnection *conn, char *gpTblFile)
/* load genePreds from table or file into chromBins.  conn maybe null if a
 * file is supplied */
{
struct chromBins *bins = chromBinsNew(genePredFree);
struct genePredReader *gpr = openGenePred(conn, gpTblFile);
struct genePred *gp;
while ((gp = genePredReaderNext(gpr)) != NULL)
    {
    if (gp->cdsStart < gp->cdsEnd)
        chromBinsAdd(bins, gp->chrom, gp->cdsStart, gp->cdsEnd, gp);
    else
        genePredFree(&gp);  /* no CDS */
    }
genePredReaderFree(&gpr);
return bins;
}

Bits *mkCdsMap(struct genePred *gp, int start, int end, int *cdsSizePtr)
/* create a bit map of CDS over the range; also get size of CDS in bases */
{
Bits *cdsMap = bitAlloc(end-start);
int iExon, exonStart, exonEnd, cdsSize = 0;
for (iExon = 0; iExon < gp->exonCount; iExon++)
    {
    if (genePredCdsExon(gp, iExon, &exonStart, &exonEnd))
        {
        bitSetRange(cdsMap, exonStart-start, exonEnd-exonStart);
        cdsSize += exonEnd-exonStart;
        }
    }
*cdsSizePtr = cdsSize;
return cdsMap;
}

float calcCdsSimilarity(struct genePred *gp1, struct genePred *gp2) 
/* calculate the fraction similarity of two CDSs */
{
int minCdsStart = min(gp1->cdsStart, gp2->cdsStart);
int maxCdsEnd = max(gp1->cdsEnd, gp2->cdsEnd);
int cds1Size, cds2Size, interCnt;
Bits *cdsMap1 = mkCdsMap(gp1, minCdsStart, maxCdsEnd, &cds1Size);
Bits *cdsMap2 = mkCdsMap(gp2, minCdsStart, maxCdsEnd, &cds2Size);
float over12, over21;

bitAnd(cdsMap1, cdsMap2, maxCdsEnd-minCdsStart);
interCnt = bitCountRange(cdsMap1, 0, maxCdsEnd-minCdsStart);
bitFree(&cdsMap1);
bitFree(&cdsMap2);

/* use the smallest of the fraction over in each direction */
over12 = ((float)interCnt)/cds1Size;
over21 = ((float)interCnt)/cds2Size;
if (over12 < over21)
    return over12;
else
    return over21;
}

void processPair(struct genePred* ccdsGp, struct genePred *kgGp,
                 FILE *tabFh)
/* process an overlapping known gene/ccds gene pair, determining if they
 * are sufficiently similar to save. */
{
float sim = calcCdsSimilarity(ccdsGp, kgGp);
if (sim >= similarity)
    {
    struct ccdsKgMap rec;
    ZeroVar(&rec);
    rec.ccdsId = ccdsGp->name;
    rec.kgId = kgGp->name;
    rec.cdsSimilarity = sim;
    ccdsKgMapTabOut(&rec, tabFh);
    }
}

void processCcds(struct chromBins* kgGenes, struct genePred *ccdsGp,
                 FILE *tabFh)
/* process CCDS, pairing it with one or more known genes */
{
struct binElement *overKgList = chromBinsFind(kgGenes, ccdsGp->chrom,
                                              ccdsGp->cdsStart, ccdsGp->cdsEnd);
struct binElement *overKg;

for (overKg = overKgList; overKg != NULL; overKg = overKg->next)
    {
    struct genePred *kgGp = overKg->val;
    if (kgGp->strand[0] == ccdsGp->strand[0])
        processPair(ccdsGp, kgGp, tabFh);
    }
}

void loadTable(struct sqlConnection *conn, char *ccdsKgMapTbl,
               char *ccdsKgMapFile)
/* load table into database */
{
char ccdsKgMapTblTmp[PATH_LEN], *ccdsKgMapSql;

/* create table with _tmp extension, then rename after loaded */
safef(ccdsKgMapTblTmp, sizeof(ccdsKgMapTblTmp), "%s_tmp", ccdsKgMapTbl);
ccdsKgMapSql = ccdsKgMapGetCreateSql(ccdsKgMapTblTmp);

sqlRemakeTable(conn, ccdsKgMapTblTmp, ccdsKgMapSql);
sqlLoadTabFile(conn, ccdsKgMapFile, ccdsKgMapTblTmp, SQL_TAB_FILE_ON_SERVER);
ccdsRenameTable(conn, ccdsKgMapTblTmp, ccdsKgMapTbl);

if (!keep)
    unlink(ccdsKgMapFile);
freeMem(ccdsKgMapSql);
}

void mkCcdsKgMap(char *db, char *ccdsTblFile, char *knownGeneTblFile,
                 char *ccdsKgMapTblFile)
/* create mappings between ccds and known genes */
{
struct sqlConnection *conn = NULL;
struct chromBins* kgGenes;
struct genePredReader *ccdsGpr;
struct genePred *ccdsGp;
FILE *tabFh;
char ccdsKgMapTbl[PATH_LEN], ccdsKgMapFile[PATH_LEN];

ccdsGetTblFileNames(ccdsKgMapTblFile, ccdsKgMapTbl, ccdsKgMapFile);
if (db != NULL)
    conn = sqlConnect(db);

/* build table file */
kgGenes = loadGenePreds(conn, knownGeneTblFile);
ccdsGpr = openGenePred(conn, ccdsTblFile);
tabFh = mustOpen(ccdsKgMapFile, "w");

while ((ccdsGp = genePredReaderNext(ccdsGpr)) != NULL)
    processCcds(kgGenes, ccdsGp, tabFh);
carefulClose(&tabFh);
genePredReaderFree(&ccdsGpr);

if (loadDb)
    loadTable(conn, ccdsKgMapTbl, ccdsKgMapFile);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *db;
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage("wrong number of args");
db = optionVal("db", NULL);
keep = optionExists("keep");
loadDb = optionExists("loadDb");
similarity = optionFloat("similarity", similarity);
if (loadDb && (db == NULL))
    errAbort("-loadDb requires -db");
mkCcdsKgMap(db, argv[1], argv[2], argv[3]);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

