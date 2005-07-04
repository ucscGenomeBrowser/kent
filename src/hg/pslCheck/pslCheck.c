/* pslCheck - validate PSL files. */
#include "common.h"
#include "options.h"
#include "portable.h"
#include "psl.h"

static char const rcsid[] = "$Id: pslCheck.c,v 1.5 2005/07/04 19:42:30 markd Exp $";

/* command line options and values */
static struct optionSpec optionSpecs[] =
{
    {"prot", OPTION_BOOLEAN},
    {NULL, 0}
};
int gProt = FALSE;

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
  "\n");
}

void checkPslFile(char *fileName)
/* Check one .psl file */
{
char pslDesc[PATH_LEN+64];
struct lineFile *lf = pslFileOpen(fileName);
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    safef(pslDesc, sizeof(pslDesc), "%s:%u", fileName, lf->lineIx);
    errCount += pslCheck(pslDesc, stderr, psl);
    if (gProt && !pslIsProtein(psl))
	{
	errCount++;
	fprintf(stderr, "%s not a protein psl\n",psl->qName);
	}
    pslFree(&psl);
    }
lineFileClose(&lf);
}

void checkPsls(int fileCount, char *fileNames[])
/* checkPsls - check files. */
{
int i;

for (i=0; i<fileCount; ++i)
    checkPslFile(fileNames[i]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
gProt = optionExists("prot");
checkPsls(argc-1, argv+1);
return ((errCount == 0) ? 0 : 1);
}
