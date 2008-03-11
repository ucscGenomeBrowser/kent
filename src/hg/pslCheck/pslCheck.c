/* pslCheck - validate PSL files or tables. */
#include "common.h"
#include "options.h"
#include "portable.h"
#include "psl.h"
#include "hash.h"
#include "jksql.h"
#include "sqlNum.h"
#include "chromInfo.h"
#include "verbose.h"

static char const rcsid[] = "$Id: pslCheck.c,v 1.12 2008/03/11 21:56:27 markd Exp $";

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"db", OPTION_STRING},
    {"prot", OPTION_BOOLEAN},
    {"quiet", OPTION_BOOLEAN},
    {"noCountCheck", OPTION_BOOLEAN},
    {"targetSizes", OPTION_STRING},
    {"querySizes", OPTION_STRING},
    {"pass", OPTION_STRING},
    {"fail", OPTION_STRING},
    {NULL, 0}
};
static char *db = NULL;
static boolean protCheck = FALSE;
static boolean quiet = FALSE;
static boolean noCountCheck = FALSE;
static char *passFile = NULL;
static char *failFile = NULL;
static struct hash *targetSizes = NULL;
static struct hash *querySizes = NULL;

/* global count of alignments checked and errors */
static int chkCount = 0;
static int failCount = 0;
static int errCount = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslCheck - validate PSL files\n"
  "usage:\n"
  "   pslCheck fileTbl(s)\n"
  "options:\n"
  "   -db=db - get targetSizes from this database, and if file doesn't exist,\n"
  "    look for a table in this database.\n"
  "   -prot - confirm psls are protein psls\n"
  "   -noCountCheck - don't validate that match/mismatch counts are match\n"
  "    the total size of the alignment blocks\n"
  "   -pass=pslFile - write PSLs without errors to this file\n"
  "   -fail=pslFile - write PSLs with errors to this file\n"
  "   -targetSizes=sizesFile - tab file with columns of target and size.\n"
  "    If specified, psl is check to have a valid target and target\n"
  "    coordinates.\n"
  "   -querySizes=sizesFile - file with query sizes.\n"
  "   -quiet - no write error message, just filter\n");
}

static struct hash *loadSizes(char *sizesFile)
/* load a sizes file */
{
struct hash *sizes = hashNew(20);
struct lineFile *lf = lineFileOpen(sizesFile, TRUE);
char *cols[2];

while (lineFileNextRowTab(lf, cols, ArraySize(cols)))
    hashAddInt(sizes, cols[0], sqlUnsigned(cols[1]));
lineFileClose(&lf);
return sizes;
}

static struct hash *loadChromInfoSizes(struct sqlConnection *conn)
/* chromInfo sizes */
{
struct hash *sizes = hashNew(20);
char **row;
struct sqlResult *sr = sqlGetResult(conn, "select * from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct chromInfo *ci = chromInfoLoad(row);
    hashAddInt(sizes, ci->chrom, ci->size);
    chromInfoFree(&ci);
    }
sqlFreeResult(&sr);
return sizes;
}

static void prPslDesc(struct psl *psl, char *pslDesc,FILE *errFh)
/* print a description of psl before the first error.  */
{
fprintf(errFh, "Error: invalid PSL: %s:%u-%u %s:%u-%u %s %s\n",
        psl->qName, psl->qStart, psl->qEnd,
        psl->tName, psl->tStart, psl->tEnd,
        psl->strand, pslDesc);
}

static int checkSize(struct psl *psl, char *pslDesc, char *sizeDesc,
                     int numErrs, struct hash *sizeTbl, char *name, int size,
                     FILE *errFh)
/* check a size, error count (0 or 1) */
{
int expectSz = hashIntValDefault(sizeTbl, name, -1);
if (expectSz < 0)
    {
    if (numErrs == 0)
        prPslDesc(psl, pslDesc, errFh);
    fprintf(errFh, "\t%s \"%s\" does not exist\n", sizeDesc, name);
    return 1;
    }
if (size != expectSz)
    {
    if (numErrs == 0)
        prPslDesc(psl, pslDesc, errFh);
    fprintf(errFh, "\t%s \"%s\" size (%d) != expected (%d)\n", sizeDesc, name,
            size, expectSz);
    return 1;
    }
return 0;    
}

static int checkCounts(struct psl *psl, char *pslDesc, int numErrs, FILE *errFh)
/* check the match/mismatch counts */
{
unsigned matchCnts = (psl->match+psl->misMatch+psl->repMatch+psl->nCount);
unsigned alnSize = 0;
int iBlk;

for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    alnSize += psl->blockSizes[iBlk];

if (alnSize != matchCnts)
    {
    if (numErrs == 0)
        prPslDesc(psl, pslDesc, errFh);
    fprintf(errFh, "alignment size (%d) doesn't match counts (%d)\n",
            alnSize, matchCnts);
    return 1;
    }
else
    return 0;
}

static void checkPsl(struct lineFile *lf, char *tbl, struct psl *psl,
                     FILE *errFh, FILE *passFh, FILE *failFh)
/* check a psl */
{
char pslDesc[PATH_LEN+64];
int numErrs = 0;
if (lf != NULL)
    safef(pslDesc, sizeof(pslDesc), "%s:%u", lf->fileName, lf->lineIx);
else
    safef(pslDesc, sizeof(pslDesc), "%s", tbl);
numErrs += pslCheck(pslDesc, errFh, psl);
if (!noCountCheck)
    numErrs += checkCounts(psl, pslDesc, numErrs, errFh);
if (protCheck && !pslIsProtein(psl))
    {
    if (numErrs == 0)
        prPslDesc(psl, pslDesc, errFh);
    fprintf(errFh, "\tnot a protein psl\n");
    numErrs++;
    }
if (targetSizes != NULL)
    numErrs += checkSize(psl, pslDesc, "target", numErrs, targetSizes, psl->tName, psl->tSize, errFh);
if (querySizes != NULL)
    numErrs += checkSize(psl, pslDesc, "query", numErrs, querySizes, psl->qName, psl->qSize, errFh);
if ((passFh != NULL) && (numErrs == 0))
    pslTabOut(psl, passFh);
if ((failFh != NULL) && (numErrs > 0))
    pslTabOut(psl, failFh);
errCount += numErrs;
chkCount++;
if (numErrs > 0)
    failCount++;
}

static void checkPslFile(char *fileName, FILE *errFh,
                         FILE *passFh, FILE *failFh)
/* Check one psl file */
{
struct lineFile *lf = pslFileOpen(fileName);
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    checkPsl(lf, NULL, psl, errFh, passFh, failFh);
    pslFree(&psl);
    }
lineFileClose(&lf);
}

static void checkPslTbl(struct sqlConnection *conn, char *tbl, FILE *errFh,
                         FILE *passFh, FILE *failFh)
/* Check one psl table */
{
char query[1024], **row;
safef(query, sizeof(query), "select * from %s", tbl);
struct sqlResult *sr = sqlGetResult(conn, query);
int rowOff = (sqlFieldColumn(sr, "bin") >= 0) ? 1 : 0;

while ((row = sqlNextRow(sr)) != NULL)
    {
    struct psl *psl = pslLoad(row+rowOff);
    checkPsl(NULL, tbl, psl, errFh, passFh, failFh);
    pslFree(&psl);
    }
sqlFreeResult(&sr);
}

void checkFilesTbls(struct sqlConnection *conn,
                    int fileTblCount, char *fileTblNames[])
/* checkPsl file or tables. */
{
int i;
FILE *errFh = quiet ? mustOpen("/dev/null", "w") : stderr;
FILE *passFh = passFile ? mustOpen(passFile, "w") : NULL;
FILE *failFh = failFile ? mustOpen(failFile, "w") : NULL;

for (i = 0; i< fileTblCount; i++)
    {
    if (fileExists(fileTblNames[i]))
        checkPslFile(fileTblNames[i], errFh, passFh, failFh);
    else
        checkPslTbl(conn, fileTblNames[i], errFh, passFh, failFh);
    }
carefulClose(&passFh);
carefulClose(&failFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
db = optionVal("db", NULL);
protCheck = optionExists("prot");
noCountCheck = optionExists("noCountCheck");
quiet = optionExists("quiet");
passFile = optionVal("pass", NULL);
failFile = optionVal("fail", NULL);
struct sqlConnection *conn = NULL;
if (db != NULL)
    conn = sqlConnect(db);

if (optionExists("targetSizes"))
    targetSizes = loadSizes(optionVal("targetSizes", NULL));
else if (db != NULL)
    targetSizes = loadChromInfoSizes(conn);
if (optionExists("querySizes"))
    querySizes = loadSizes(optionVal("querySizes", NULL));
checkFilesTbls(conn, argc-1, argv+1);
sqlDisconnect(&conn);
verbose(1, "checked: %d failed: %d errors: %d\n", chkCount, failCount, errCount);
return ((errCount == 0) ? 0 : 1);
}
