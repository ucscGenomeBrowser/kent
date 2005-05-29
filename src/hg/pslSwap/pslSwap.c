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

void usage(char *msg)
/* usage message and abort */
{
errAbort("%s:\n"
         "pslSwap [options] inPsl outPsl\n"
         "\n"
         "Swap target and query in psls\n"
         "\n"
         "Options:\n"
         "  -noRc - don't reverse complement untranslated alignments to\n"
         "   keep target positive strand.  This will make the target strand\n"
         "   explict.\n",
         msg);
}

struct psl *allocPsl(struct psl *inPsl)
/* allocated memory, including block arrays for a PSL */
{
struct psl *outPsl;
AllocVar(outPsl);
outPsl->blockSizes = needMem(inPsl->blockCount*sizeof(unsigned));
outPsl->qStarts = needMem(inPsl->blockCount*sizeof(unsigned));
outPsl->tStarts = needMem(inPsl->blockCount*sizeof(unsigned));
if (inPsl->qSequence != NULL)
    {
    outPsl->qSequence = needMem(inPsl->blockCount*sizeof(char*));
    outPsl->tSequence = needMem(inPsl->blockCount*sizeof(char*));
    }
return outPsl;
}

void swapBlocks(struct psl *inPsl, struct psl *outPsl)
/* swap the blocks in a psl without reverse complementing them */
{
int i;
for (i = 0; i < inPsl->blockCount; i++)
    {
    outPsl->blockSizes[i] = inPsl->blockSizes[i];
    outPsl->qStarts[i] = inPsl->tStarts[i];
    outPsl->tStarts[i] = inPsl->qStarts[i];
    if (outPsl->qSequence != NULL)
        {
        outPsl->qSequence[i] = cloneString(inPsl->tSequence[i]);
        outPsl->tSequence[i] = cloneString(inPsl->qSequence[i]);
        }
    }
}

void swapRCBlocks(struct psl *inPsl, struct psl *outPsl)
/* swap and reverse complement blocks in a psl */
{
int ii, oi;
for (ii = 0, oi = inPsl->blockCount-1; ii < inPsl->blockCount; ii++, oi--)
    {
    outPsl->blockSizes[oi] = inPsl->blockSizes[ii];
    outPsl->qStarts[oi] = inPsl->tSize - (inPsl->tStarts[ii] + inPsl->blockSizes[ii]);
    outPsl->tStarts[oi] = inPsl->qSize - (inPsl->qStarts[ii] + inPsl->blockSizes[ii]);
    if (outPsl->qSequence != NULL)
        {
        outPsl->qSequence[oi] = cloneString(inPsl->tSequence[ii]);
        reverseComplement(outPsl->qSequence[oi], strlen(outPsl->qSequence[oi]));
        outPsl->tSequence[oi] = cloneString(inPsl->qSequence[ii]);
        reverseComplement(outPsl->tSequence[oi], strlen(outPsl->tSequence[oi]));
        }
    }
}

struct psl *pslSwap(struct psl *inPsl)
/* reverse target and query in a psl */
{
struct psl *outPsl = allocPsl(inPsl);
outPsl->match = inPsl->match;
outPsl->misMatch = inPsl->misMatch;
outPsl->repMatch = inPsl->repMatch;
outPsl->nCount = inPsl->nCount;
outPsl->qNumInsert = inPsl->tNumInsert;
outPsl->qBaseInsert = inPsl->tBaseInsert;
outPsl->tNumInsert = inPsl->qNumInsert;
outPsl->tBaseInsert = inPsl->qBaseInsert;
outPsl->qName = cloneString(inPsl->tName);
outPsl->qSize = inPsl->tSize;
outPsl->qStart = inPsl->tStart;
outPsl->qEnd = inPsl->tEnd;
outPsl->tName = cloneString(inPsl->qName);
outPsl->tSize = inPsl->qSize;
outPsl->tStart = inPsl->qStart;
outPsl->tEnd = inPsl->qEnd;
outPsl->blockCount = inPsl->blockCount;

/* handle strand and block copy */
if (inPsl->strand[1] != '\0')
    {
    /* translated */
    outPsl->strand[0] = inPsl->strand[1];
    outPsl->strand[1] = inPsl->strand[0];
    swapBlocks(inPsl, outPsl);
    }
else if (gNoRc)
    {
    /* untranslated with no reverse complement */
    outPsl->strand[0] = '+';
    outPsl->strand[1] = inPsl->strand[0];
    swapBlocks(inPsl, outPsl);
    }
else
    {
    /* untranslated */
    outPsl->strand[0] = inPsl->strand[0];
    if (inPsl->strand[0] == '+')
        swapBlocks(inPsl, outPsl);
    else
        swapRCBlocks(inPsl, outPsl);
    }
return outPsl;
}

void pslSwapFile(char *inPslFile, char *outPslFile)
/* reverse target and query in a psl file */
{
struct lineFile *inLf = pslFileOpen(inPslFile);
FILE *outFh = mustOpen(outPslFile, "w");
struct psl *inPsl;

while ((inPsl = pslNext(inLf)) != NULL)
    {
    struct psl *outPsl = pslSwap(inPsl);
    pslTabOut(outPsl, outFh);
    pslFree(&outPsl);
    pslFree(&inPsl);
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
pslSwapFile(argv[1], argv[2]);
return 0;
}
