/* pslCheck - validate PSL files. */
#include "common.h"
#include "options.h"
#include "portable.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslCheck.c,v 1.6 2005/07/31 00:22:49 markd Exp $";

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"prot", OPTION_BOOLEAN},
    {"quiet", OPTION_BOOLEAN},
    {"pass", OPTION_STRING},
    {"fail", OPTION_STRING},
    {NULL, 0}
};
int protCheck = FALSE;
boolean quiet = FALSE;
char *passFile = NULL;
char *failFile = NULL;

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
  "   -quiet - no write error message, just filter\n");
}

void checkPslFile(char *fileName, FILE *errFh,
                  FILE *passFh, FILE *failFh)
/* Check one .psl file */
{
char pslDesc[PATH_LEN+64];
struct lineFile *lf = pslFileOpen(fileName);
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    int prevErrCnt = errCount;
    safef(pslDesc, sizeof(pslDesc), "%s:%u", fileName, lf->lineIx);
    errCount += pslCheck(pslDesc, errFh, psl);
    if (protCheck && !pslIsProtein(psl))
	{
	errCount++;
	fprintf(stderr, "%s not a protein psl\n",psl->qName);
	}
    if ((passFh != NULL) && (errCount == prevErrCnt))
        pslTabOut( psl, passFh);
    if ((failFh != NULL) && (errCount > prevErrCnt))
        pslTabOut(psl, failFh);
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
checkPsls(argc-1, argv+1);
return ((errCount == 0) ? 0 : 1);
}
