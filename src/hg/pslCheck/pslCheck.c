/* pslCheck - validate PSL files. */
#include "common.h"
#include "options.h"
#include "portable.h"
#include "psl.h"
#include "hash.h"
#include "sqlNum.h"

static char const rcsid[] = "$Id: pslCheck.c,v 1.8 2006/01/28 04:21:26 markd Exp $";

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"prot", OPTION_BOOLEAN},
    {"quiet", OPTION_BOOLEAN},
    {"targetSizes", OPTION_STRING},
    {"querySizes", OPTION_STRING},
    {"pass", OPTION_STRING},
    {"fail", OPTION_STRING},
    {NULL, 0}
};
int protCheck = FALSE;
boolean quiet = FALSE;
char *passFile = NULL;
char *failFile = NULL;
struct hash *targetSizes = NULL;
struct hash *querySizes = NULL;

/* global count of errors */
int errCount = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslCheck - validate PSL files\n"
  "usage:\n"
  "   pslCheck file(s)\n"
  "options:\n"
  "   -prot  confirm psls are protein psls\n"
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

static void checkPsl(struct lineFile *lf, struct psl *psl, FILE *errFh,
                     FILE *passFh, FILE *failFh)
/* check a psl */
{
char pslDesc[PATH_LEN+64];
int numErrs = 0;
safef(pslDesc, sizeof(pslDesc), "%s:%u", lf->fileName, lf->lineIx);
numErrs += pslCheck(pslDesc, errFh, psl);
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
}

static void checkPslFile(char *fileName, FILE *errFh,
                         FILE *passFh, FILE *failFh)
/* Check one .psl file */
{
struct lineFile *lf = pslFileOpen(fileName);
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    checkPsl(lf, psl, errFh, passFh, failFh);
    pslFree(&psl);
    }
lineFileClose(&lf);
}

void checkPsls(int fileCount, char *fileNames[])
/* checkPsls - check files. */
{
int i;
FILE *errFh = quiet ? mustOpen("/dev/null", "w") : stderr;
FILE *passFh = passFile ? mustOpen(passFile, "w") : NULL;
FILE *failFh = failFile ? mustOpen(failFile, "w") : NULL;

for (i=0; i<fileCount; ++i)
    checkPslFile(fileNames[i], errFh, passFh, failFh);
carefulClose(&passFh);
carefulClose(&failFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
protCheck = optionExists("prot");
quiet = optionExists("quiet");
passFile = optionVal("pass", NULL);
failFile = optionVal("fail", NULL);

if (optionExists("targetSizes"))
    targetSizes = loadSizes(optionVal("targetSizes", NULL));
if (optionExists("querySizes"))
    querySizes = loadSizes(optionVal("querySizes", NULL));
checkPsls(argc-1, argv+1);
return ((errCount == 0) ? 0 : 1);
}
