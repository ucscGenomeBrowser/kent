/* pslRc - reverse-complement psl */

#include "common.h"
#include "linefile.h"
#include "dnautil.h"
#include "options.h"
#include "psl.h"

/* command line options */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};
boolean gNoRc = FALSE;

void usage(char *msg)
/* usage message and abort */
{
errAbort("%s:\n"
         "pslRc [options] inPsl outPsl\n"
         "\n"
         "reverse-complement psl\n"
         "\n"
         "Options:\n",
         msg);
}

void pslRcFile(char *inPslFile, char *outPslFile)
/* reverse target and query in a psl file */
{
struct lineFile *inLf = pslFileOpen(inPslFile);
FILE *outFh = mustOpen(outPslFile, "w");
struct psl *psl;

while ((psl = pslNext(inLf)) != NULL)
    {
    pslRc(psl);
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
    usage("wrong # args");
gNoRc = optionExists("noRc");
dnaUtilOpen();
pslRcFile(argv[1], argv[2]);
return 0;
}
