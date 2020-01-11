/* used for cleaning up self alignments - deletes all overlapping self alignment blocks */
#include "common.h"
#include "linefile.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
"pslDropOverlap - deletes all overlapping (trivial/diagonal) self-alignment blocks. \n"
"usage:\n"
"    pslDropOverlap in.psl out.psl\n"
"This discards information in mismatch, repMatch and nCount, lumping all into match.\n"
"(all matching bases are counted in the match column).\n");
}

void deleteElement(unsigned *list, int i, int total)
/* delete element i from array of size total, move elements down by one to fill gap */
{
int j;

for (j = i ; j < total ; j++)
    list[j] = list[j+1];
list[total-1] = 0;
}

void pslDropOverlap(char *inName, char *outName)
/* Delete all aligned blocks where target and query are at the same position. */
{
struct lineFile *lf = pslFileOpen(inName);
FILE *f = mustOpen(outName, "w");
struct psl *psl;
int skipMatch = 0;
int totMatch = 0;
int totSkip = 0;
int totLines = 0;
int totBlocks = 0;

while ((psl = pslNext(lf)) != NULL)
    {
    totLines++;
    totBlocks += psl->blockCount;
    psl->match += psl->misMatch + psl->repMatch + psl->nCount;
    psl->misMatch = psl->repMatch = psl->nCount = 0;
    totMatch += psl->match;
    if (sameString(psl->qName, psl->tName))
        {
        boolean changed = FALSE;
        int i;
        int newBlockCount = psl->blockCount;
        for (i = psl->blockCount-1 ; i >= 0 ; i--)
            {
            int ts = psl->tStarts[i];
            int te = psl->tStarts[i]+psl->blockSizes[i];
            int qs = psl->qStarts[i];
            int qe = psl->qStarts[i]+psl->blockSizes[i];
            if (psl->strand[0] == '-')
                reverseIntRange(&qs, &qe, psl->qSize);
            if (psl->strand[1] == '-')
                reverseIntRange(&ts, &te, psl->tSize);
            if (rangeIntersection(ts, te, qs, qe) > 0)
                {
                if ((ts != qs || te != qe) &&
                    ((psl->strand[0] == '+' && psl->strand[1] == '\0') ||
                     (psl->strand[0] == psl->strand[1])))
                    verbose(2, "line %d: Blocks[%d] overlap but are not identical: "
                            "t:%d,%d;  q:%d,%d, offset:%d, size:%d",
                         lf->lineIx, i, ts, te, qs, qe, qs - ts, psl->blockSizes[i]);
                newBlockCount--;
                // In unscored PSL, psl->match could be 0; prevent underflow.
                if (psl->match >= psl->blockSizes[i])
                    psl->match -= psl->blockSizes[i];
                else if (psl->match > 0)
                    lineFileAbort(lf, "psl->match too small (%d) for block %d size %d",
                                  psl->match, i, psl->blockSizes[i]);
                totSkip++;
                skipMatch += psl->blockSizes[i];
                verbose(2, "skip block size %d #%d blk %d "
                        "%d\t%d\t%d\t%d\t%s\t%s\t%d\t%d\t%d\t%s\t%d\t%d\t%d\n",
                        psl->blockSizes[i], i, psl->blockCount,
                        psl->match, psl->misMatch, psl->repMatch, psl->nCount,
                        psl->strand,
                        psl->qName, psl->qSize, psl->qStart, psl->qEnd,
                        psl->tName, psl->tSize, psl->tStart, psl->tEnd);
                deleteElement(psl->tStarts, i , psl->blockCount);
                deleteElement(psl->qStarts, i , psl->blockCount);
                deleteElement(psl->blockSizes, i , psl->blockCount);
                changed = TRUE;
                }
            }
        if (changed)
            {
            psl->blockCount = newBlockCount;
            pslRecalcBounds(psl);
            pslComputeInsertCounts(psl);
            }
        }
    if (psl->blockCount > 0)
        pslTabOut(psl, f);
    pslFree(&psl);
    }
verbose(1, "Total skipped %d blocks out of %d blocks in %d alignments, match %d out of %d, in %s\n",
	totSkip, totBlocks, totLines, skipMatch, totMatch,  outName );
fclose(f);
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
pslDropOverlap(argv[1], argv[2]);
return 0;
}
