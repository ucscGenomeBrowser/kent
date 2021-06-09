/* pslSwap - reverse target and query in psls */

#include "common.h"
#include "linefile.h"
#include "dnautil.h"
#include "options.h"
#include "psl.h"

/* command line options */
static struct optionSpec optionSpecs[] = {
    {"noRc", OPTION_BOOLEAN},
    {NULL, 0}
};
boolean gNoRc = FALSE;

void usage()
/* usage message and abort */
{
errAbort("pslSwap - swap target and query in psls\n"
         "usage:\n"
         "    pslSwap [options] inPsl outPsl\n"
         "\n"
         "Options:\n"
         "  -noRc - don't reverse complement untranslated alignments to\n"
         "   keep target positive strand.  This will make the target strand\n"
         "   explict.\n");
}

void pslSwapFile(char *inPslFile, char *outPslFile)
/* reverse target and query in a psl file */
{
struct lineFile *inLf = pslFileOpen(inPslFile);
FILE *outFh = mustOpen(outPslFile, "w");
struct psl *psl;

while ((psl = pslNext(inLf)) != NULL)
    {
    pslSwap(psl, gNoRc);
    pslTabOut(psl, outFh);
    pslFree(&psl);
    }

carefulClose(&outFh);
lineFileClose(&inLf);
}

int main(int argc, char** argv)
/* entry */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
gNoRc = optionExists("noRc");
dnaUtilOpen();
pslSwapFile(argv[1], argv[2]);
return 0;
}
