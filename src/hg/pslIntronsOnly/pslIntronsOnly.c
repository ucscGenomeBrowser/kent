/* pslIntronsOnly - Filter psl files to only include those with introns. */
#include "common.h"
#include "linefile.h"
#include "psl.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslIntronsOnly - Filter psl files to only include those with introns\n"
  "usage:\n"
  "   pslIntronsOnly in.psl in.nib out.psl\n");
}

boolean hasIntron(struct psl *psl, struct dnaSeq *seq, int seqOffset)
/* Return TRUE if there's a probable intron. */
{
int blockCount = psl->blockCount, i;
unsigned *tStarts = psl->tStarts;
unsigned *blockSizes = psl->blockSizes;
unsigned *qStarts = psl->qStarts;
int blockSize, start, end;
DNA *dna = seq->dna;

for (i=1; i<blockCount; ++i)
    {
    blockSize = blockSizes[i-1];
    start = qStarts[i-1]+blockSize;
    end = qStarts[i];
    if (start == end)
        {
	start = tStarts[i-1]+blockSize-seqOffset;
	end = tStarts[i]-seqOffset;
	if (intronOrientation(dna+start, dna+end) != 0)
	    return TRUE;
	}
    }
return FALSE;
}

void pslIntronsOnly(char *inPslName, char *inNibName, char *outPslName)
/* pslIntronsOnly - Filter psl files to only include those with introns. */
{
struct dnaSeq *nibSeq = NULL;
int nibStart = 0, nibEnd = 0, nibSize = 0;
int nibSegTargetSize = 32*1024*1024;
int nibSegSize = 0, pslSegSize = 0;
struct lineFile *lf = NULL;
FILE *nibFile = NULL, *outFile = NULL;
struct psl *psl;
int count = 0, intronCount = 0;

lf = pslFileOpen(inPslName);
nibOpenVerify(inNibName, &nibFile, &nibSize);
outFile = mustOpen(outPslName, "w");
while ((psl = pslNext(lf)) != NULL)
    {
    ++count;
    pslSegSize = psl->tEnd - psl->tStart;
    if (rangeIntersection(psl->tStart, psl->tEnd, nibStart, nibEnd) < pslSegSize)
        {
	freeDnaSeq(&nibSeq);
	nibStart = psl->tStart;
	nibEnd = nibStart + nibSegTargetSize;
	if (psl->tEnd > nibEnd) nibEnd = psl->tEnd;
	if (nibEnd > nibSize) nibEnd = nibSize;
	uglyf("Processing %s:%d-%d\n", inNibName, nibStart, nibEnd);
	nibSeq = nibLdPart(inNibName, nibFile, nibSize, nibStart, nibEnd - nibStart);
	}
    if (hasIntron(psl, nibSeq, nibStart))
        {
	++intronCount;
	pslTabOut(psl, outFile);
	}
    pslFree(&psl);
    }
carefulClose(&outFile);
carefulClose(&nibFile);
lineFileClose(&lf);
printf("%d of %d in %s have introns\n", intronCount, count, inPslName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
dnaUtilOpen();
pslIntronsOnly(argv[1], argv[2], argv[3]);
return 0;
}
