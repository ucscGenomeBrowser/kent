/* pslProtToRnaCoords - Convert protein alignments to RNA coordinates. */
#include "common.h"
#include "psl.h"
#include "pslTransMap.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslProtToRnaCoords - Convert protein alignments to RNA coordinates\n"
  "usage:\n"
  "   pslProtToRnaCoords inPsl outPsl\n"
  "\n"
  "Convert either a protein/protein or protein/NA PSL to NA/NA PSL.  This\n"
  "multiplies coordinates and statistics by three.  As this can occasionally\n"
  "results in blocks overlapping, overlap is trimmed as needed.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static void pslProtToRnaCoords(char *inPsl, char *outPsl)
/* pslProtToRnaCoords - Convert protein alignments to RNA coordinates. */
{
struct lineFile *inFh = pslFileOpen(inPsl);
FILE *outFh = mustOpen(outPsl, "w");

struct psl *psl;
while ((psl = pslNext(inFh)) != NULL)
    {
    pslProtToNAConvert(psl);
    pslTabOut(psl, outFh);
    pslFree(&psl);
    }

carefulClose(&outFh);
lineFileClose(&inFh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
pslProtToRnaCoords(argv[1], argv[2]);
return 0;
}
