/* pslScore - calculate web blat score from psl files. */
#include "common.h"
#include "linefile.h"
#include "psl.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslScore - calculate web blat score from psl files\n"
  "usage:\n"
  "   pslScore <file.psl> [moreFiles.psl]\n"
  "options:\n"
  "   none at this time\n\n"
  "columns in output:\n\n"
  "#tName\ttStart\ttEnd\tqName:qStart-qEnd\tscore\tpercentIdentity"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void pslScoreDisplay(char *inFiles[], int inFileCount, char *outFile)
/* pslScoreDisplay - calculate web blat score from psl files. */
{
int i;

for (i=0; i<inFileCount; ++i)
    {
    struct psl *psl;
    char *inPsl = inFiles[i];
    verbose(3, "# reading: %s\n", inPsl);
    struct lineFile *pslFile = pslFileOpen(inPsl);
    while ((psl = pslNext(pslFile)) != NULL)
        {
        int score = pslScore(psl);
        int milliBad = pslCalcMilliBad(psl, TRUE);
        float percentIdentity = 100.0 - pslCalcMilliBad(psl, TRUE) * 0.1;
        verbose(2, "# %s\t%s\t%d\t%.2f\t%d\n", psl->tName, psl->qName, score, percentIdentity, milliBad);
        printf("%s\t%d\t%d\t%s:%d-%d\t%d\t%.2f\n", psl->tName, psl->tStart, psl->tEnd, psl->qName, psl->qStart, psl->qEnd, score, percentIdentity);
        pslFree(&psl);
        }
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
pslScoreDisplay(argv+1, argc-1, argv[argc-1]);
return 0;
}
