
/* mkCcdsGeneMap - create mappings between ccds and other gene tables */

#include "common.h"
#include "options.h"
#include "genePred.h"
#include "genePredReader.h"
#include "jksql.h"
#include "ccdsInfo.h"
#include "ccdsGeneMap.h"
#include "linefile.h"
#include "verbose.h"
#include "chromBins.h"
#include "binRange.h"
#include "bits.h"
#include "ccdsCommon.h"

static char const rcsid[] = "$Id: mkCcdsGeneMap.c,v 1.2 2008/07/01 06:40:46 markd Exp $";

static struct optionSpec optionSpecs[] =
/* command line option specifications */
{
    {"db", OPTION_STRING},
    {"loadDb", OPTION_BOOLEAN},
    {"keep", OPTION_BOOLEAN},
    {"similarity", OPTION_FLOAT},
    {"noopNoCcdsTbl", OPTION_BOOLEAN},
    {NULL, 0}
};
static boolean keep = FALSE;    /* keep tab files after load */
static boolean loadDb = FALSE;  /* load database */
static boolean noopNoCcdsTbl = FALSE; /* skip if no CCDS table */
static float similarity = 0.95; /* fraction similarity */

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("Error: %s\n"
    "mkCcdsGeneMap - create with mappings between ccds and other gene tables\n"
    "\n"
    "Usage:\n"
    "   mkCcdsGeneMap [options] ccdsTblFile geneTblFile ccdsGeneMapOut\n"
    "\n"
    "Create a mapping between CCDS and other gene tables based on similarity\n"
    "of the genomic location of the CDS.  The ccdsTblFile, and geneTblFile\n"
    "arguments are genePreds and can be tables or files. The ccdsGeneMapOut\n"
    "specifes the table and tab file to create to load the database, it's base name\n"
    "is also the table name.  If a simple table name is supplied, a .tab extenions is\n"
    "added.  The table is first loaded with an _tmp extension and then renamed\n"
    "if the load succeeds.\n"
    "\n"
    "Options:\n"
    "  -db=db - database if tables are being read or loaded\n"
    "  -similarity=0.95 - require at least this level of similar\n"
    "  -loadDb - load table into db\n"
    "  -noopNoCcdsTbl - if specified, it's the program will exit without\n"
    "   error if there is no CCDS table. Requires -db.\n"
    "  -keep - keep tmp tab file used to load database\n"
    "  -verbose=n\n",
    msg);
}

struct genePredReader *openGenePred(struct sqlConnection *conn,
                                    char *gpTblFile)
/* create a genePredReader, determining if a file or table is used for
 * input. */
{
char gpTbl[PATH_LEN], gpFile[PATH_LEN];

ccdsGetTblFileNames(gpTblFile, gpTbl, gpFile);
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
char gpTbl[PATH_LEN], gpFile[PATH_LEN];
struct chromBins *bins = chromBinsNew(genePredFree);
struct genePredReader *gpr = openGenePred(conn, gpTblFile);
struct genePred *gp;

ccdsGetTblFileNames(gpTblFile, gpTbl, gpFile);

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

void processPair(struct genePred* ccdsGp, struct genePred *geneGp,
                 FILE *tabFh)
/* process an overlapping gene/ccds gene pair, determining if they
 * are sufficiently similar to save. */
{
float sim = calcCdsSimilarity(ccdsGp, geneGp);
if (sim >= similarity)
    {
    struct ccdsGeneMap rec;
    ZeroVar(&rec);
    rec.ccdsId = ccdsGp->name;
    rec.geneId = geneGp->name;
    rec.chrom = geneGp->chrom;
    rec.chromStart = geneGp->txStart; 
    rec.chromEnd = geneGp->txEnd;
    rec.cdsSimilarity = sim;
    ccdsGeneMapTabOut(&rec, tabFh);
    }
}

void processCcds(struct chromBins* genes, struct genePred *ccdsGp,
                 FILE *tabFh)
/* process CCDS, pairing it with one or more genes */
{
struct binElement *overGeneList = chromBinsFind(genes, ccdsGp->chrom,
                                                ccdsGp->cdsStart, ccdsGp->cdsEnd);
struct binElement *overGene;

for (overGene = overGeneList; overGene != NULL; overGene = overGene->next)
    {
    struct genePred *geneGp = overGene->val;
    if (geneGp->strand[0] == ccdsGp->strand[0])
        processPair(ccdsGp, geneGp, tabFh);
    }
}

void loadTable(struct sqlConnection *conn, char *ccdsGeneMapTbl,
               char *ccdsGeneMapFile)
/* load table into database */
{
char ccdsGeneMapTblTmp[PATH_LEN], *ccdsGeneMapSql;

/* create table with _tmp extension, then rename after loaded */
safef(ccdsGeneMapTblTmp, sizeof(ccdsGeneMapTblTmp), "%s_tmp", ccdsGeneMapTbl);
ccdsGeneMapSql = ccdsGeneMapGetCreateSql(ccdsGeneMapTblTmp);

sqlRemakeTable(conn, ccdsGeneMapTblTmp, ccdsGeneMapSql);
sqlLoadTabFile(conn, ccdsGeneMapFile, ccdsGeneMapTblTmp, SQL_TAB_FILE_ON_SERVER);
ccdsRenameTable(conn, ccdsGeneMapTblTmp, ccdsGeneMapTbl);

if (!keep)
    unlink(ccdsGeneMapFile);
freeMem(ccdsGeneMapSql);
}

void noopNoCcdsTblCheck(struct sqlConnection *conn, char *ccdsTblFile)
/* check for ccds table and exit if it doesn't exist */
{
char ccdsTbl[PATH_LEN];
ccdsGetTblFileNames(ccdsTblFile, ccdsTbl, NULL);
if (!sqlTableExists(conn, ccdsTbl))
    {
    fprintf(stderr, "Note: no %s.%s table, ignoring load request\n",
            sqlGetDatabase(conn), ccdsTbl);
    exit(0);
    }
} 

void mkCcdsGeneMap(char *db, char *ccdsTblFile, char *geneTblFile,
                   char *ccdsGeneMapTblFile)
/* create mappings between ccds and known genes */
{
struct sqlConnection *conn = NULL;
struct chromBins* genes;
struct genePredReader *ccdsGpr;
struct genePred *ccdsGp;
FILE *tabFh;
char ccdsGeneMapTbl[PATH_LEN], ccdsGeneMapFile[PATH_LEN];

ccdsGetTblFileNames(ccdsGeneMapTblFile, ccdsGeneMapTbl, ccdsGeneMapFile);

if (db != NULL)
    conn = sqlConnect(db);
if (noopNoCcdsTbl)
    noopNoCcdsTblCheck(conn, ccdsTblFile);

/* build table file */
genes = loadGenePreds(conn, geneTblFile);
ccdsGpr = openGenePred(conn, ccdsTblFile);
tabFh = mustOpen(ccdsGeneMapFile, "w");

while ((ccdsGp = genePredReaderNext(ccdsGpr)) != NULL)
    processCcds(genes, ccdsGp, tabFh);
carefulClose(&tabFh);
genePredReaderFree(&ccdsGpr);

if (loadDb)
    loadTable(conn, ccdsGeneMapTbl, ccdsGeneMapFile);

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
noopNoCcdsTbl = optionExists("noopNoCcdsTbl");
similarity = optionFloat("similarity", similarity);
if (loadDb && (db == NULL))
    errAbort("-loadDb requires -db");
if (noopNoCcdsTbl && (db == NULL))
    errAbort("-noopNoCcdsTbl requires -db");
mkCcdsGeneMap(db, argv[1], argv[2], argv[3]);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

