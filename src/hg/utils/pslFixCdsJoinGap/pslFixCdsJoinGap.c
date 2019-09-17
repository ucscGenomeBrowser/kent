/* pslFixCdsJoinGap - If a PSL record has a one-sided t gap corresponding to a CDS join,
 * make it two-sided. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "regexHelper.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslFixCdsJoinGap - If a PSL record has a one-sided t gap corresponding to a CDS join, make it two-sided\n"
  "usage:\n"
  "   pslFixCdsJoinGap pslIn cdsIn pslOut\n"
  "pslIn generally comes from genePredToFakePsl.\n"
  "cdsIn contains Genbank CDS strings such as \"123..456\" or \"join(123..301,303..456)\"\n"
  "pslOut will contain the same rows as pslIn except for tweaked gaps for joins.\n"
  "The ideal solution would be to remove the gap and properly interpret join CDS in the rest\n"
  "of our software, but this will at least prevent totally incorrect genePred-derived PSL for\n"
  "transcripts with +1 ribosomal frameshift for now.\n"
//  "options:\n"
//  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

static struct hash *getRiboSkipOffsets(char *cdsInFile)
/* Read in a file with two columns: accession and Genbank CDS string.  If the CDS string
 * is a join statement with two ranges that skip a base, then store that base offset for acc. */
{
struct lineFile *lf = lineFileOpen(cdsInFile, TRUE);
struct hash *skipHash = hashNew(0);
int riboSkipCount = 0;
regmatch_t substrs[3];
char *words[2];
char *line;
while (lineFileNextReal(lf, &line))
    {
    int wordCount = chopTabs(line, words);
    lineFileExpectWords(lf, 2, wordCount);
    char *acc = words[0];
    char *cds = words[1];
    if (regexMatchSubstr(cds, "^join\\([0-9]+\\.\\.([0-9]+),([0-9]+)\\.\\.[0-9]+\\)$",
                         substrs, ArraySize(substrs)))
        {
        int endFirst = regexSubstringInt(cds, substrs[1]);
        int startSecond = regexSubstringInt(cds, substrs[2]);
        if (startSecond == endFirst + 2)
            {
            riboSkipCount++;
            hashAddInt(skipHash, acc, endFirst);
            }
        }
    }
verbose(2, "Found %d CDS join records that skip a base.\n", riboSkipCount);
lineFileClose(&lf);
return skipHash;
}

static void fixRiboSkip(struct psl *psl, struct hash *skipHash)
/* If skipHash has a skipped-base offset for psl->qName, and psl->t coords skip the corresponding
 * target base, then tweak psl q coords to skip the base as well. */
{
int skippedBase = hashIntValDefault(skipHash, psl->qName, -1);
if (skippedBase >= 0)
    {
    verbose(2, "Found skippedBase=%d for %s.\n", skippedBase, psl->qName);
    boolean isRc = (pslQStrand(psl) == '-');
    // Do this on q + coords.
    if (isRc)
        pslRc(psl);
    int fixIx = -1, ix;
    for (ix = 0;  ix < psl->blockCount - 1;  ix++)
        {
        // Detect 1-based t gap, 0-base q gap at skippedBase:
        int blkQEnd = psl->qStarts[ix] + psl->blockSizes[ix];
        int nextBlkQStart = psl->qStarts[ix+1];
        if (blkQEnd == skippedBase && nextBlkQStart == skippedBase)
            {
            // 0-base gap on q; is there a 1-base gap on t?
            int blkTEnd = psl->tStarts[ix] + psl->blockSizes[ix];
            int nextBlkTStart = psl->tStarts[ix+1];
            if (nextBlkTStart == blkTEnd + 1)
                {
                fixIx = ix+1;
                verbose(2, "Start adjusting at ix %d\n", fixIx);
                }
            }
        if (blkQEnd >= skippedBase)
            break;
        }
    if (fixIx >= 0)
        {
        // Increment all q coords after fixIx
        for (ix = fixIx;  ix < psl->blockCount;  ix++)
            psl->qStarts[ix]++;
        psl->qEnd++;
        if (psl->qSize < psl->qEnd)
            psl->qSize++;
        psl->qNumInsert++;
        psl->qBaseInsert++;
        }
    if (isRc)
        pslRc(psl);
    }
}

void pslFixCdsJoinGap(char *pslInFile, char *cdsInFile, char *pslOutFile)
/* pslFixCdsJoinGap - If a PSL record has a one-sided t gap corresponding to a CDS join,
 * make it two-sided. */
{
struct lineFile *pslIn = pslFileOpen(pslInFile);
FILE *pslOut = mustOpen(pslOutFile, "w");
struct hash *skipHash = getRiboSkipOffsets(cdsInFile);
struct psl *psl;
while ((psl = pslNext(pslIn)) != NULL)
    {
    fixRiboSkip(psl, skipHash);
    pslTabOut(psl, pslOut);
    pslFree(&psl);
    }
lineFileClose(&pslIn);
carefulClose(&pslOut);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
pslFixCdsJoinGap(argv[1], argv[2], argv[3]);
return 0;
}
