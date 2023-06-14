/* pslProtToRnaCoords - Convert protein alignments to RNA coordinates. */
#include "common.h"
#include "psl.h"
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

static void trimOverlapping(bool isProtNaPsl, struct psl *psl, int oBlk)
/* if this block overlaps the previous block, trim accordingly.  Overlaps
 * can be created with protein to DNA PSLs.  */
// check for overlap; use int so we can go negative
{
int prevQEnd = pslQEnd(psl, oBlk - 1);
int prevTEnd = pslTEnd(psl, oBlk - 1);

// trim, possibly setting to zero-length
int overAmt = max((prevQEnd - (int)psl->qStarts[oBlk]), (prevTEnd - (int)psl->tStarts[oBlk]));
if (overAmt < 0)
    overAmt = 0;
else if (overAmt > psl->blockSizes[oBlk])
    overAmt = psl->blockSizes[oBlk];

psl->qStarts[oBlk] += overAmt;
psl->tStarts[oBlk] += overAmt;
psl->blockSizes[oBlk] -= overAmt;
psl->match -= overAmt;
}


static int updateBlk(bool isProtNaPsl, struct psl *psl, int iBlk, int oBlk)
/* Update sizes in a block, adjusting start of block if it now
 * overlaps previous block due previous block size expansion.
 * return updated oBlk */
{
// update to NA coordinates
psl->blockSizes[oBlk] = 3 * psl->blockSizes[iBlk];
psl->qStarts[oBlk] = 3 * psl->qStarts[iBlk];
if (isProtNaPsl)
    psl->tStarts[oBlk] = psl->tStarts[iBlk];  // already in NA coords
else
    psl->tStarts[oBlk] = 3 * psl->tStarts[iBlk];
psl->match += psl->blockSizes[oBlk];

if (oBlk > 0)
    trimOverlapping(isProtNaPsl, psl, oBlk);

// only advance if something left in the block
if (psl->blockSizes[oBlk] > 0)
    oBlk++;
return oBlk;
}

static void pslConvertCoords(struct psl *psl)
/* convert psl coordinates */
{
boolean isProtNaPsl = pslIsProtein(psl);
// overall sizes and coordinates
psl->qSize *= 3;
psl->qStart *= 3;
psl->qEnd *= 3;
if (!isProtNaPsl)
    {
    psl->tSize *= 3;
    psl->tStart *= 3;
    psl->tEnd *= 3;
    }

/* set counts to be all match, since we don't know actual answer and
 * it makes it easier to trim overlapping blocks  */
psl->match = psl->misMatch = psl->repMatch = psl->nCount = 0;

// blocks
int oBlk = 0;
for (int iBlk = 0; iBlk < psl->blockCount; iBlk += 1)
    {
    oBlk = updateBlk(isProtNaPsl, psl, iBlk, oBlk);
    }
psl->blockCount = oBlk;

// need to update overall coordinates if last block was full remove due to
// overlap
psl->qStart = psl->qStarts[0];
psl->qEnd = psl->qStarts[oBlk - 1] + psl->blockSizes[oBlk - 1];
if (psl->strand[0] == '-')
    reverseIntRange(&psl->qStart, &psl->qEnd, psl->qSize);
psl->tStart = psl->tStarts[0];
psl->tEnd = psl->tStarts[oBlk - 1] + psl->blockSizes[oBlk - 1];
if (psl->strand[0] == '-')
    reverseIntRange(&psl->tStart, &psl->tEnd, psl->tSize);

// indels
pslComputeInsertCounts(psl);
}


static void pslProtToRnaCoords(char *inPsl, char *outPsl)
/* pslProtToRnaCoords - Convert protein alignments to RNA coordinates. */
{
struct lineFile *inFh = pslFileOpen(inPsl);
FILE *outFh = mustOpen(outPsl, "w");

struct psl *psl;
while ((psl = pslNext(inFh)) != NULL)
    {
    pslConvertCoords(psl);
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
