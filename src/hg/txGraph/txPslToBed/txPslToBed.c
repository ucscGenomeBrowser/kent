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
int minSize = 18;
int minIntronSize=16;
FILE *unusualFile = NULL;

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
  "   -minSize=N - suppress output of beds of less than this size (in exons)\n"
  "                Default value is %d.\n"
  "   -noFixIntrons - slide large gaps in target to seek for splice sites.\n"
  "   -minIntronSize - minimum size of an intron, default %d.\n"
  "   -unusual=fileName - put info about unusual splice sites etc. here\n"
  , mergeMax, minSize, minIntronSize
  );
}


static struct optionSpec options[] = {
   {"mergeMax", OPTION_INT},
   {"minSize", OPTION_INT},
   {"noFixIntrons", OPTION_BOOLEAN},
   {"minIntronSize", OPTION_INT},
   {"unusual", OPTION_STRING},
   {NULL, 0},
};

void unusualPrint(char *format, ...)
/* Print out info to unusual file if it exists. */
{
if (unusualFile != NULL)
    {
    va_list args;
    va_start(args, format);
    vfprintf(unusualFile, format, args);
    va_end(args);
    }
}

int totalIntronLooking = 0;
int totalFunny = 0;

void fixOrientation(struct psl *psl, struct dnaSeq *chrom)
/* If the transcript is spliced, use this to override the
 * input orientation.  Also write info about unusual splice
 * sites to unusual file. */
{
int inputOrientation = (psl->strand[0] == '-' ? -1 : 1);
int totalOrientation = 0;
int outputOrientation;
int i;
int lastBlock = psl->blockCount-1;
for (i=0; i<lastBlock; ++i)
    {
    int blockSize = psl->blockSizes[i];
    int tStart = psl->tStarts[i] + blockSize;
    int qStart = psl->qStarts[i] + blockSize;
    int tEnd = psl->tStarts[i+1];
    int qEnd = psl->qStarts[i+1];
    if (qStart == qEnd && tEnd-tStart >= minIntronSize)
        {
	++totalIntronLooking;
	char *intronStart = chrom->dna + tStart;
	char *intronEnd = chrom->dna + tEnd;
	int orientation = intronOrientationMinSize(intronStart, intronEnd, minIntronSize);
	totalOrientation += orientation;
	if (orientation == 0)
	    {
	    totalFunny += 1;
	    unusualPrint("site %c%c/%c%c %s %s %d %s:%d-%d\n", intronStart[0], intronStart[1],
	    	intronEnd[-2], intronEnd[-1], psl->strand, psl->qName,
		i+1, psl->tName, psl->tStart+1, psl->tEnd); 
	    }
	}
    }
if (totalOrientation > 0)
    outputOrientation = 1;
else if (totalOrientation < 0)
    outputOrientation = -1;
else
    outputOrientation = inputOrientation;
if (outputOrientation != inputOrientation)
    {
    unusualPrint("flip %s %d %d %d %s:%d-%d\n", psl->qName, inputOrientation,
    	outputOrientation, totalOrientation, psl->tName, psl->tStart+1, psl->tEnd);
    if (psl->strand[0] == '+')
        psl->strand[0] = '-';
    else
        psl->strand[0] = '+';
    }
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
    if (intronOrientationMinSize(intronStart, intronEnd, minIntronSize) != orientation)
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
		if (memcmp(intronEnd-2-1, "ag", 2) == 0)
		    {
		    psl->blockSizes[i] += 1;
		    psl->tStarts[i] -= 1;
		    }
		else if (memcmp(intronEnd-2+1, "ag", 2) == 0)
		    {
		    psl->blockSizes[i] -= 1;
		    psl->tStarts[i] += 1;
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
		if (memcmp(intronEnd-2-1, "ac", 2) == 0)
		    {
		    psl->blockSizes[i] += 1;
		    psl->tStarts[i] -= 1;
		    }
		else if (memcmp(intronEnd-2+1, "ac", 2) == 0)
		    {
		    psl->blockSizes[i] -= 1;
		    psl->tStarts[i] += 1;
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
int totalSize = 0;
struct range *range = NULL, *lastRange = NULL;
for (range = rangeList; range != NULL; range = range->next)
    {
    if (range->end > range->start)
	{
	++blockCount;
	totalSize += range->end - range->start;
	lastRange = range;
	}
    }

if (totalSize < minSize)
    return NULL;

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
    if (range->end > range->start)
	{
	sizes[i] = range->end - range->start;
	starts[i] = range->start - chromStart;
	++i;
	}
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
    else if (qGap > 1 || qGap < -1 || tStart - tEnd < minIntronSize ||
        (strand == '+' && (iStart[0] != 'g' || (iStart[1] != 'c' && iStart[1] != 't') || iEnd[0] != 'a' || iEnd[1] != 'g' )) ||
        (strand == '-' && (iStart[0] != 'c' || iStart[1] != 't' || (iEnd[0] != 'a' && iEnd[0] != 'g') || iEnd[1] != 'c' )))
	 {
	 /* Break case. Not a short break or a gt/ag or gc/ag intron on either strand */
	 slReverse(&list);
	 bed = rangeListToBed(list, psl->tName, psl->qName, psl->strand, 0);
	 if (bed)
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
if (bed)
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
    fixOrientation(psl, chrom);
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
unusualPrint("total intron-looking %d, total funny-looking %d\n", 
	totalIntronLooking, totalFunny);
carefulClose(&f);
dnaSeqFree(&chrom);
nibTwoCacheFree(&ntc);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
mergeMax = optionInt("mergeMax", mergeMax);
minSize = optionInt("minSize", minSize);
fixIntrons = !optionExists("noFixIntrons");
minIntronSize = optionInt("minIntronSize", minIntronSize);
if (optionExists("unusual"))
    unusualFile = mustOpen(optionVal("unusual", NULL), "w");
if (argc != 4)
    usage();
txPsltoBed(argv[1], argv[2], argv[3]);
carefulClose(&unusualFile);
return 0;
}
