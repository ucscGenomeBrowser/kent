/* pslRemoveFrameShifts - remove frame shifts from psl. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslRemoveFrameShifts - remove frame shifts from psl\n"
  "usage:\n"
  "   pslRemoveFrameShifts file.psl out.psl\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void doIt(char *fileName, char *outFile)
/* pslRemoveFrameShifts - remove frame shifts from psl. */
{
FILE *outF = mustOpen(outFile, "w");
struct lineFile *lf = pslFileOpen(fileName);
struct psl  *psl;
while ((psl = pslNext(lf)) != NULL)
    {
    pslRemoveFrameShifts(psl);
    pslOutput(psl, outF, '\t', '\n');
    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
doIt(argv[1], argv[2]);
return 0;
}
