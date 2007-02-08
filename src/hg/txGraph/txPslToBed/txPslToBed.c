/* txPsltoBed - Convert a psl to a bed file by projecting it onto it's target 
 * sequence. Optionally merge adjacent blocks and trim to splice sites. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "psl.h"
#include "dnaseq.h"
#include "nibTwo.h"
#include "bed.h"
#include "rangeTree.h"

/* Variables set from command line. */
int mergeMax = 5;	
boolean fixIntrons = TRUE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txPsltoBed - Convert a psl to a bed file by projecting it onto its target\n"
  "sequence. Optionally merge adjacent blocks and trim to splice sites.\n"
  "usage:\n"
  "   txPsltoBed input.psl dnaPath output.bed\n"
  "where dnaPath is either a two bit file or a dir of nib files containing the DNA\n"
  "referenced on the target side of input.psl.\n"
  "options:\n"
  "   -mergeMax=N - merge small blocks separated by no more than N on either\n"
  "                 target or query. Default value is %d.\n"
  "   -noFixIntrons - slide large gaps in target to seek for splice sites.\n"
  , mergeMax
  );
}

static struct optionSpec options[] = {
   {"mergeMax", OPTION_INT},
   {"noFixIntrons", OPTION_BOOLEAN},
   {NULL, 0},
};

struct bed *pslToBed(struct psl *psl, int mergeMax)
/* Create a bed similar to PSL, but merging nearby blocks. */
{
/* Calculate blockCount. */
int blockCount = 1;
int tStart = psl->tStarts[0];
int qStart = psl->qStarts[0];
int blockSize = psl->blockSizes[0];
int i;
for (i=1; i<psl->blockCount; ++i)
    {
    int tEnd = tStart + blockSize;
    int qEnd = qStart + blockSize;
    tStart = psl->tStarts[i];
    qStart = psl->qStarts[i];
    blockSize = psl->blockSizes[i];
    if (tStart - tEnd > mergeMax || qStart - qEnd > mergeMax)
	++blockCount;
    }

/* Call library routine that does conversion without merging. */
struct bed *bed = bedFromPsl(psl);
if (blockCount != psl->blockCount)
    {
    /* If need be merge some blocks. */
    blockCount = 1;
    int tStart = psl->tStarts[0];
    int qStart = psl->qStarts[0];
    int blockSize = psl->blockSizes[0];
    for (i=1; i<psl->blockCount; ++i)
        {
	int tEnd = tStart + blockSize;
	int qEnd = qStart + blockSize;
	tStart = psl->tStarts[i];
	qStart = psl->qStarts[i];
	blockSize = psl->blockSizes[i];
	if (tStart - tEnd > mergeMax || qStart - qEnd > mergeMax)
	    {
	    bed->blockSizes[blockCount] = blockSize;
	    bed->chromStarts[blockCount] = tStart - bed->chromStart;
	    ++blockCount;
	    }
	else
	    {
	    bed->blockSizes[blockCount-1] += tStart - tEnd + blockSize;
	    }
	}
    bed->blockCount = blockCount;
    }
return bed;
}

void fixPslIntrons(struct psl *psl, struct dnaSeq *chrom)
/* Go through gaps in psl.  For ones the right size to be introns,
 * see if ends are introns.  If not see if adding or subtracting
 * a base could turn them into intron ends. */
{
int start = psl->tStarts[0];
int size = psl->blockSizes[0];
int i;
char *dna = chrom->dna;
int orientation = (psl->strand[0] == '-' ? -1 : 1);
for (i=1; i<psl->blockCount; ++i)
    {
    int end = start+size;
    size = psl->blockSizes[i];
    start = psl->tStarts[i];
    char *intronStart = dna+end;
    char *intronEnd = dna+start;
    if (intronOrientation(intronStart, intronEnd) != orientation)
        {
	if (orientation > 0)
	    {
	    if (memcmp(intronStart, "gt", 2) != 0)
		{
		if (memcmp(intronStart-1, "gt", 2) == 0)
		    {
		    psl->blockSizes[i-1] -= 1;
		    }
		else if (memcmp(intronStart+1, "gt", 2) == 0)
		    {
		    psl->blockSizes[i-1] += 1;
		    }
		}
	    if (memcmp(intronEnd-2, "ag", 2) != 0)
	        {
		if (memcmp(intronEnd-2-1, "ag", 2) != 0)
		    {
		    psl->blockSizes[i] -= 1;
		    psl->tStarts[i] += 1;
		    }
		else if (memcmp(intronEnd-2+1, "ag", 2) != 0)
		    {
		    psl->blockSizes[i] += 1;
		    psl->tStarts[i] -= 1;
		    }
		}
	    }
	else
	    {
	    if (memcmp(intronStart, "ct", 2) != 0)
		{
		if (memcmp(intronStart-1, "ct", 2) == 0)
		    {
		    psl->blockSizes[i-1] -= 1;
		    }
		else if (memcmp(intronStart+1, "ct", 2) == 0)
		    {
		    psl->blockSizes[i-1] += 1;
		    }
		}
	    if (memcmp(intronEnd-2, "ac", 2) != 0)
	        {
		if (memcmp(intronEnd-2-1, "ac", 2) != 0)
		    {
		    psl->blockSizes[i] -= 1;
		    psl->tStarts[i] += 1;
		    }
		else if (memcmp(intronEnd-2+1, "ac", 2) != 0)
		    {
		    psl->blockSizes[i] += 1;
		    psl->tStarts[i] -= 1;
		    }
		}
	    }
	}
    }
}

struct bed *rangeListToBed(struct range *rangeList, char *chrom, char *name, char *strand, int score)
/* Convert a list of ranges to a bed12. */
{
if (rangeList == NULL)
    return NULL;

/* Figure out number of blocks and overall bounds. */
int blockCount = 0;
struct range *range = NULL, *lastRange = NULL;
for (range = rangeList; range != NULL; range = range->next)
    {
    ++blockCount;
    lastRange = range;
    }
int chromStart = rangeList->start;
int chromEnd = lastRange->end;

/* Allocate bed and fill in most of it. */
struct bed *bed;
AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = chromStart;
bed->chromEnd = chromEnd;
bed->name = cloneString(name);
bed->score = score;
bed->strand[0] = strand[0];
bed->blockCount = blockCount;

/* Fill in sizes and starts arrays */
int *sizes = AllocArray(bed->blockSizes, blockCount);
int *starts = AllocArray(bed->chromStarts, blockCount);
int i = 0;
for (range = rangeList; range != NULL; range = range->next)
    {
    sizes[i] = range->end - range->start;
    starts[i] = range->start - chromStart;
    ++i;
    }

return bed;
}

struct bed *pslToBedList(struct psl *psl, struct dnaSeq *chrom, int mergeMax)
/* Convert a psl to a list of beds, breaking up at gaps that are bigger than mergeMax,
 * and not introns. */
{
struct bed *bedList = NULL, *bed;
struct lm *lm = lmInit(0);
struct range *list = NULL, *el;
char strand = psl->strand[0];

/* Create first range from first block, and put it on list. */
int tStart = psl->tStarts[0];
int qStart = psl->qStarts[0];
int blockSize = psl->blockSizes[0];
lmAllocVar(lm, list);
list->start = tStart;
list->end = tStart + blockSize;

int i;
for (i=1; i<psl->blockCount; ++i)
    {
    /* Loop through.  At each position have three choices:
     *   1) Normal - add new range to existing list
     *   2) Merge - merge in block with current range
     *   3) Break - output range list so far and start new one with 
     *      this block. */
    int tEnd = tStart + blockSize;
    int qEnd = qStart + blockSize;
    tStart = psl->tStarts[i];
    qStart = psl->qStarts[i];
    int qGap = qStart - qEnd;
    char *iStart = chrom->dna + tEnd;
    char *iEnd = chrom->dna + tStart - 2;
    blockSize = psl->blockSizes[i];
    if (tStart - tEnd <= mergeMax && qStart - qEnd <= mergeMax)	
         {
	 /* merge case */
	 list->end = tStart + blockSize;
	 }
    else if (qGap > 1 || qGap < -1 || tStart - tEnd < 16 ||
        (strand == '+' && (iStart[0] != 'g' || (iStart[1] != 'c' && iStart[1] != 't') || iEnd[0] != 'a' || iEnd[1] != 'g' )) ||
        (strand == '-' && (iStart[0] != 'c' || iStart[1] != 't' || (iEnd[0] != 'a' && iEnd[0] != 'g') || iEnd[1] != 'c' )))
	 {
	 /* Break case. Not a short break or a gt/ag or gc/ag intron on either strand */
	 slReverse(&list);
	 bed = rangeListToBed(list, psl->tName, psl->qName, psl->strand, 0);
	 slAddHead(&bedList, bed);
	 lmAllocVar(lm, list);
	 list->start = tStart;
	 list->end = tStart + blockSize;
	 }
    else
         {
	 /* Normal case. */
	 lmAllocVar(lm, el);
	 el->start = tStart;
	 el->end = tStart + blockSize;
	 slAddHead(&list, el);
	 }
    }
slReverse(&list);
bed  = rangeListToBed(list, psl->tName, psl->qName, psl->strand, 0);
slAddHead(&bedList, bed);

lmCleanup(&lm);
slReverse(&bedList);
return bedList;
}



void txPsltoBed(char *inPsl, char *dnaPath, char *outBed)
/* txPsltoBed - Convert a psl to a bed file by projecting it onto it's target 
 * sequence. Optionally merge adjacent blocks and trim to splice sites. */
{
FILE *f = mustOpen(outBed, "w");
struct psl *psl, *pslList  = pslLoadAll(inPsl);
char *chromName = "";
struct nibTwoCache *ntc = nibTwoCacheNew(dnaPath);
struct dnaSeq *chrom = NULL;


verbose(2, "Loaded %d psls\n", slCount(pslList));

slSort(&pslList, pslCmpTarget);
for (psl = pslList; psl != NULL; psl = psl->next)
    {
    verbose(3, "Processing %s\n", psl->qName);
    if (!sameString(chromName, psl->tName))
	{
	dnaSeqFree(&chrom);
	chrom = nibTwoCacheSeq(ntc, psl->tName);
	toLowerN(chrom->dna, chrom->size);
	chromName = psl->tName;
	verbose(2, "Loaded %d bases in %s\n", chrom->size, chromName);
	}
    if (psl->tSize != chrom->size)
	errAbort("DNA and PSL out of sync. %s thinks %s is %d bases, but %s thinks it's %d.",
		 inPsl, chromName, psl->tSize, dnaPath, chrom->size);
    if (fixIntrons)
	fixPslIntrons(psl, chrom);
    struct bed *bedList = pslToBedList(psl, chrom, mergeMax);
    struct bed *bed;
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	bedTabOutN(bed, 12, f);
	}
    bedFreeList(&bedList);
    }
carefulClose(&f);
dnaSeqFree(&chrom);
nibTwoCacheFree(&ntc);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
mergeMax = optionInt("mergeMax", mergeMax);
fixIntrons = !optionExists("noFixIntrons");
if (argc != 4)
    usage();
txPsltoBed(argv[1], argv[2], argv[3]);
return 0;
}
