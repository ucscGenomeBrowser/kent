/* bedGraphToBigWig - Convert a bedGraph program to bigWig.. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "cirTree.h"
#include "bPlusTree.h"
#include "bbiFile.h"
#include "bwgInternal.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: bedGraphToBigWig.c,v 1.5 2009/06/27 00:41:57 kent Exp $";

#define maxZoomLevels 10

int blockSize = 256;
int itemsPerSlot = 1024;
boolean clipDontDie = FALSE;


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedGraphToBigWig - Convert a bedGraph program to bigWig.\n"
  "usage:\n"
  "   bedGraphToBigWig in.bedGraph chrom.sizes out.bw\n"
  "where in.bedGraph is a four column file in the format:\n"
  "      <chrom> <start> <end> <value>\n"
  "and chrom.sizes is two column: <chromosome name> <size in bases>\n"
  "and out.bw is the output indexed big wig file.\n"
  "options:\n"
  "   -blockSize=N - Number of items to bundle in r-tree.  Default %d\n"
  "   -itemsPerSlot=N - Number of data points bundled at lowest level. Default %d\n"
  "   -clip - If set just issue warning messages rather than dying if wig\n"
  "                  file contains items off end of chromosome."
  , blockSize, itemsPerSlot
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {"itemsPerSlot", OPTION_INT},
   {"clip", OPTION_BOOLEAN},
   {NULL, 0},
};


struct bwgBedGraphItem
/* An bedGraph-type item in a bwgSection. */
    {
    struct bwgBedGraphItem *next;	/* Next in list. */
    bits32 start,end;		/* Range of chromosome covered. */
    float val;			/* Value. */
    };

struct chromUsage
/* Information on how many items per chromosome etc. */
    {
    struct chromUsage *next;
    char *name;	/* chromosome name. */
    bits32 itemCount;	/* Number of items for this chromosome. */
    bits32 id;	/* Unique ID for chromosome. */
    bits32 size;	/* Size of chromosome. */
    };



struct chromUsage *readPass1(struct lineFile *lf, struct hash *chromSizesHash, int *retMinDiff)
/* Go through chromGraph file and collect chromosomes and statistics. */
{
char *row[2];
struct hash *uniqHash = hashNew(0);
struct chromUsage *usage = NULL, *usageList = NULL;
int lastStart = -1;
bits32 id = 0;
int minDiff = BIGNUM;
for (;;)
    {
    int rowSize = lineFileChopNext(lf, row, ArraySize(row));
    if (rowSize == 0)
        break;
    lineFileExpectWords(lf, 2, rowSize);
    char *chrom = row[0];
    int start = lineFileNeedNum(lf, row, 1);
    if (usage == NULL || differentString(usage->name, chrom))
        {
	if (hashLookup(uniqHash, chrom))
	    {
	    errAbort("%s is not sorted.  Please use \"sort -k1 -k2n\" or bedSort and try again.",
	    	lf->fileName);
	    }
	hashAdd(uniqHash, chrom, NULL);
	AllocVar(usage);
	usage->name = cloneString(chrom);
	usage->id = id++;
	usage->size = hashIntVal(chromSizesHash, chrom);
	slAddHead(&usageList, usage);
	lastStart = -1;
	}
    usage->itemCount += 1;
    if (lastStart >= 0)
        {
	int diff = start - lastStart;
	if (diff < minDiff)
	    minDiff = diff;
	}
    lastStart = start;
    }
slReverse(&usageList);
*retMinDiff = minDiff;
return usageList;
}

void writeDummyHeader(FILE *f)
/* Write out all-zero header, just to reserve space for it. */
{
repeatCharOut(f, 0, 64);
}

void writeDummyZooms(FILE *f)
/* Write out zeroes to reserve space for ten zoom levels. */
{
repeatCharOut(f, 0, maxZoomLevels * 24);
}

void writeChromInfo(struct chromUsage *usageList, int blockSize, FILE *f)
/* Write out information on chromosomes to file. */
{
int chromCount = slCount(usageList);
struct chromUsage *usage;

/* Allocate and fill in array from list. */
struct bbiChromInfo *chromInfoArray;
AllocArray(chromInfoArray, chromCount);
int i;
int maxChromNameSize = 0;
for (i=0, usage = usageList; i<chromCount; ++i, usage = usage->next)
    {
    char *chromName = usage->name;
    int len = strlen(chromName);
    if (len > maxChromNameSize)
        maxChromNameSize = len;
    chromInfoArray[i].name = chromName;
    chromInfoArray[i].id = usage->id;
    chromInfoArray[i].id = usage->size;
    }

/* Write chromosome bPlusTree */
int chromBlockSize = min(blockSize, chromCount);
bptFileBulkIndexToOpenFile(chromInfoArray, sizeof(chromInfoArray[0]), chromCount, chromBlockSize,
    bbiChromInfoKey, maxChromNameSize, bbiChromInfoVal, 
    sizeof(chromInfoArray[0].id) + sizeof(chromInfoArray[0].size), 
    f);

freeMem(chromInfoArray);
}

int countSectionsNeeded(struct chromUsage *usageList, int itemsPerSlot)
/* Count up number of sections needed for data. */
{
struct chromUsage *usage;
int count = 0;
for (usage = usageList; usage != NULL; usage = usage->next)
    {
    int countOne = (usage->itemCount + itemsPerSlot - 1)/itemsPerSlot;
    count += countOne;
    uglyf("%s %d, %d blocks of %d\n", usage->name, usage->itemCount, countOne, itemsPerSlot);
    }
return count;
}

struct boundsArray
/* What a section covers and where it is on disk. */
    {
    bits64 offset;		/* Offset within file. */
    struct cirTreeRange range;	/* What is covered. */
    };

static struct cirTreeRange boundsArrayFetchKey(const void *va, void *context)
/* Fetch boundsArray key for r-tree */
{
const struct boundsArray *a = ((struct boundsArray *)va);
return a->range;
}

static bits64 boundsArrayFetchOffset(const void *va, void *context)
/* Fetch boundsArray file offset for r-tree */
{
const struct boundsArray *a = ((struct boundsArray *)va);
return a->offset;
}

struct sectionItem
/* An item in a section of a bedGraph. */
    {
    bits32 start, end;			/* Position in chromosome, half open. */
    float val;				/* Single precision value. */
    };

void writeSections(struct chromUsage *usageList, struct lineFile *lf, 
	int itemsPerSlot, struct boundsArray *bounds, int sectionCount, FILE *f,
	int resTryCount, int resScales[], int resSizes[])
/* Read through lf, chunking it into sections that get written to f.  Save info
 * about sections in bounds. */
{
struct chromUsage *usage = usageList;
int itemIx = 0, sectionIx = 0;
bits32 reserved32 = 0;
bits16 reserved16 = 0;
UBYTE reserved8 = 0;
struct sectionItem items[itemsPerSlot];
struct sectionItem *lastB = NULL;
bits32 resEnds[resTryCount];
int resTry;
for (resTry = 0; resTry < resTryCount; ++resTry)
    resEnds[resTry] = 0;
for (;;)
    {
    /* Get next line of input if any. */
    char *row[5];
    int rowSize = lineFileChopNext(lf, row, ArraySize(row));

    /* Figure out whether need to output section. */
    boolean sameChrom = FALSE;
    if (rowSize > 0)
	sameChrom = sameString(row[0], usage->name);
    if (itemIx >= itemsPerSlot || rowSize == 0 || !sameChrom)
        {
	/* Figure out section position. */
	bits32 chromId = usage->id;
	bits32 sectionStart = items[0].start;
	bits32 sectionEnd = items[itemIx-1].start;

	/* Save section info for indexing. */
	assert(sectionIx < sectionCount);
	struct boundsArray *section = &bounds[sectionIx++];
	section->offset = ftell(f);
	section->range.chromIx = chromId;
	section->range.start = sectionStart;
	section->range.end = sectionEnd;

	/* Output section header to file. */
	UBYTE type = bwgTypeBedGraph;
	bits16 itemCount = itemIx;
	writeOne(f, chromId);			// chromId
	writeOne(f, sectionStart);		// start
	writeOne(f, sectionEnd);	// end
	writeOne(f, reserved32);		// itemStep
	writeOne(f, reserved32);		// itemSpan
	writeOne(f, type);			// type
	writeOne(f, reserved8);			// reserved
	writeOne(f, itemCount);			// itemCount

	/* Output each item in section to file. */
	int i;
	for (i=0; i<itemIx; ++i)
	    {
	    struct sectionItem *item = &items[i];
	    writeOne(f, item->start);
	    writeOne(f, item->end);
	    writeOne(f, item->val);
	    }

	/* If at end of input we are done. */
	if (rowSize == 0)
	    break;

	/* Set up for next section. */
	itemIx = 0;

	if (!sameChrom)
	    {
	    usage = usage->next;
	    assert(usage != NULL);
	    assert(sameString(row[0], usage->name));
	    lastB = NULL;
	    for (resTry = 0; resTry < resTryCount; ++resTry)
		resEnds[resTry] = 0;
	    }
	}

    /* Parse out input. */
    lineFileExpectWords(lf, 4, rowSize);
    bits32 start = lineFileNeedNum(lf, row, 1);
    bits32 end = lineFileNeedNum(lf, row, 2);
    float val = lineFileNeedDouble(lf, row, 3);

    /* Verify that inputs meets our assumption - that it is a sorted bedGraph file. */
    if (start > end)
        errAbort("Start (%u) after end (%u) line %d of %s", start, end, lf->lineIx, lf->fileName);
    if (lastB != NULL)
        {
	if (lastB->start > start)
	    errAbort("BedGraph not sorted on start line %d of %s", lf->lineIx, lf->fileName);
	if (lastB->end > start)
	    errAbort("Overlapping regions in bedGraph line %d of %s", lf->lineIx, lf->fileName);
	}


    /* Do zoom counting. */
    for (resTry = 0; resTry < resTryCount; ++resTry)
        {
	bits32 resEnd = resEnds[resTry];
	if (start >= resEnd)
	    {
	    resSizes[resTry] += 1;
	    resEnds[resTry] = resEnd = start + resScales[resTry];
	    }
	while (end > resEnd)
	    {
	    resSizes[resTry] += 1;
	    resEnds[resTry] = resEnd = resEnd + resScales[resTry];
	    }
	}

    /* Save values in output array. */
    struct sectionItem *b = &items[itemIx];
    b->start = start;
    b->end = end;
    b->val = val;
    lastB = b;
    itemIx += 1;
    }
assert(sectionIx == sectionCount);
}

void outputOneSummaryFurtherReduce(struct bbiSummary *sum, struct bbiSummary **pTwiceReducedList, 
	int doubleReductionSize, struct boundsArray **pBoundsPt, struct boundsArray *boundsEnd, 
	bits32 chromSize, struct lm *lm, FILE *f)
/* Write out sum to file, keeping track of minimal info on it in *pBoundsPt, and also adding
 * it to second level summary. */
{
uglyf("outputOneSummaryFurtherReduce(%u:%d-%d valid %d, min %f, max %f, ave %f\n",
	sum->chromId, sum->start, sum->end, sum->validCount, sum->minVal, sum->maxVal, sum->sumData/sum->validCount);
/* Get place to store file offset etc and make sure we have not gone off end. */
struct boundsArray *bounds = *pBoundsPt;
assert(bounds < boundsEnd);
*pBoundsPt += 1;

/* Fill in bounds info. */
bounds->offset = ftell(f);
bounds->range.chromIx = sum->chromId;
bounds->range.start = sum->start;
bounds->range.end = sum->end;

/* Write out summary info. */
writeOne(f, sum->chromId);
writeOne(f, sum->start);
writeOne(f, sum->end);
writeOne(f, sum->validCount);
bbiWriteFloat(f, sum->minVal);
bbiWriteFloat(f, sum->maxVal);
bbiWriteFloat(f, sum->sumData);
bbiWriteFloat(f, sum->sumSquares);

/* Fold summary info into pTwiceReducedList. */
struct bbiSummary *twiceReduced = *pTwiceReducedList;
if (twiceReduced == NULL || twiceReduced->chromId != sum->chromId || twiceReduced->end < sum->end)
    {
    lmAllocVar(lm, twiceReduced);
    twiceReduced->chromId = sum->chromId;
    twiceReduced->start = sum->start;
    twiceReduced->end = twiceReduced->start + doubleReductionSize;
    if (twiceReduced->end > chromSize) twiceReduced->end = chromSize;
    slAddHead(pTwiceReducedList, twiceReduced);
    }
else
    {
    twiceReduced->validCount += sum->validCount;
    if (sum->minVal < twiceReduced->minVal) twiceReduced->minVal = sum->minVal;
    if (sum->maxVal < twiceReduced->maxVal) twiceReduced->maxVal = sum->maxVal;
    twiceReduced->sumData += sum->sumData;
    twiceReduced->sumSquares += sum->sumSquares;
    }
}

struct bbiSummary *writeReducedOnceReturnReducedTwice(struct chromUsage *usageList, 
	struct lineFile *lf, int initialReduction, int initialReductionCount, 
	int resIncrement, int blockSize, int itemsPerSlot, 
	struct lm *lm, FILE *f, bits64 *retDataStart, bits64 *retIndexStart)
/* Write out data reduced by factor of initialReduction.  Also calculate and keep in memory
 * next reduction level.  This is more work than some ways, but it keeps us from having to
 * keep the first reduction entirely in memory. */
{
struct bbiSummary *twiceReducedList = NULL;
bits32 doubleReductionSize = initialReduction * resIncrement;
struct chromUsage *usage = usageList;
struct bbiSummary oneSummary, *sum = NULL;
int outCount = 0;
struct boundsArray *boundsArray, *boundsPt, *boundsEnd;
boundsPt = AllocArray(boundsArray, initialReductionCount);
boundsEnd = boundsPt + initialReductionCount;

*retDataStart = ftell(f);
for (;;)
    {
    /* Get next line of input if any. */
    char *row[5];
    int rowSize = lineFileChopNext(lf, row, ArraySize(row));

    /* Output last section and break if at end of file. */
    if (rowSize == 0 && sum != NULL)
	{
	outputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize, 
		&boundsPt, boundsEnd, usage->size, lm, f);
	break;
	}

    /* Parse out row. */
    char *chrom = row[0];
    bits32 start = sqlUnsigned(row[1]);
    bits32 end = sqlUnsigned(row[2]);
    float val = sqlFloat(row[3]);

    /* If new chromosome output existing block. */
    if (differentString(chrom, usage->name))
        {
	usage = usage->next;
	outputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize,
		&boundsPt, boundsEnd, usage->size, lm, f);
	sum = NULL;
	}

    /* If start past existing block then output it. */
    else if (sum != NULL && sum->end <= start)
	{
	outputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize, 
		&boundsPt, boundsEnd, usage->size, lm, f);
	sum = NULL;
	}

    /* If don't have a summary we're working on now, make one. */
    if (sum == NULL)
        {
	oneSummary.chromId = usage->id;
	oneSummary.start = start;
	oneSummary.end = start + initialReduction;
	if (oneSummary.end > usage->size) oneSummary.end = usage->size;
	oneSummary.minVal = oneSummary.maxVal = val;
	oneSummary.sumData = oneSummary.sumSquares = 0.0;
	oneSummary.validCount = 0;
	sum = &oneSummary;
	}
    
    /* Deal with case where might have to split an item between multiple summaries.  This
     * loop handles all but the final affected summary in that case. */
    while (end > sum->end)
        {
	uglyf("Splitting\n");
	/* Fold in bits that overlap with existing summary and output. */
	bits32 overlap = rangeIntersection(start, end, sum->start, sum->end);
	sum->validCount += overlap;
	if (sum->minVal > val) sum->minVal = val;
	if (sum->maxVal < val) sum->maxVal = val;
	sum->sumData += val * overlap;
	sum->sumSquares += val * overlap;
	outputOneSummaryFurtherReduce(sum, &twiceReducedList, doubleReductionSize, 
		&boundsPt, boundsEnd, usage->size, lm, f);

	/* Move summary to next part. */
	sum->start = start = sum->end;
	sum->end = start + initialReduction;
	if (sum->end > usage->size) sum->end = usage->size;
	sum->minVal = sum->maxVal = val;
	sum->sumData = sum->sumSquares = 0.0;
	sum->validCount = 0;
	}

    /* Add to summary. */
    bits32 size = end - start;
    sum->validCount += size;
    if (sum->minVal > val) sum->minVal = val;
    if (sum->maxVal < val) sum->maxVal = val;
    sum->sumData += val * size;
    sum->sumSquares += val * size;
    }

/* Write out 1st zoom index. */
int indexOffset = *retIndexStart = ftell(f);
assert(boundsPt == boundsEnd);
cirTreeFileBulkIndexToOpenFile(boundsArray, sizeof(boundsArray[0]), initialReductionCount,
    blockSize, itemsPerSlot, NULL, boundsArrayFetchKey, boundsArrayFetchOffset, 
    indexOffset, f);

freez(&boundsArray);
slReverse(&twiceReducedList);
return twiceReducedList;
}

struct bbiSummary *simpleReduce(struct bbiSummary *list, int reduction, struct lm *lm)
/* Do a simple reduction - where among other things the reduction level is an integral
 * multiple of the previous reduction level, and the list is sorted. Allocate result out of lm. */
{
struct bbiSummary *newList = NULL, *sum, *newSum = NULL;
for (sum = list; sum != NULL; sum = sum->next)
    {
    if (newSum == NULL || newSum->chromId != sum->chromId || sum->end > newSum->start + reduction)
        {
	lmAllocVar(lm, newSum);
	*newSum = *sum;
	slAddHead(&newList, newSum);
	}
    else
        {
	newSum->end = sum->end;
	newSum->validCount += sum->validCount;
	if (newSum->minVal > sum->minVal) newSum->minVal = sum->minVal;
	if (newSum->maxVal < sum->maxVal) newSum->maxVal = sum->maxVal;
	newSum->sumData += sum->sumData;
	newSum->sumSquares += sum->sumSquares;
	}
    }
slReverse(&newList);
return newList;
}

void bedGraphToBigWig(char *inName, char *chromSizes, char *outName)
/* bedGraphToBigWig - Convert a bedGraph program to bigWig.. */
{
struct lineFile *lf = lineFileOpen(inName, TRUE);
struct hash *chromSizesHash = bbiChromSizesFromFile(chromSizes);
verbose(2, "%d chroms in %s\n", chromSizesHash->elCount, chromSizes);
int minDiff;
bits64 totalDiff, diffCount;
struct chromUsage *usage, *usageList = readPass1(lf, chromSizesHash, &minDiff);
verbose(2, "%d chroms in %s\n", slCount(usageList), inName);
for (usage = usageList; usage != NULL; usage = usage->next)
    uglyf("%s\t%d\n", usage->name, usage->itemCount);

/* Write out dummy header, zoom offsets. */
FILE *f = mustOpen(outName, "wb");
writeDummyHeader(f);
writeDummyZooms(f);

/* Write out chromosome/size database. */
bits64 chromTreeOffset = ftell(f);
writeChromInfo(usageList, blockSize, f);

/* Set up to keep track of reduction levels. */
int resTryCount = 10, resTry;
int resIncrement = 4;
int resScales[resTryCount], resSizes[resTryCount];
int res = minDiff * 2;
if (res > 0)
    {
    for (resTry = 0; resTry < resTryCount; ++resTry)
	{
	resSizes[resTry] = 0;
	resScales[resTry] = res;
	res *= resIncrement;
	}
    }
else
    resTryCount = 0;

/* Write out primary full resolution data in sections, collect stats to use for reductions. */
bits64 dataOffset = ftell(f);
bits32 sectionCount = countSectionsNeeded(usageList, itemsPerSlot);
struct boundsArray *boundsArray;
AllocArray(boundsArray, sectionCount);
lineFileRewind(lf);
writeSections(usageList, lf, itemsPerSlot, boundsArray, sectionCount, f,
	resTryCount, resScales, resSizes);

/* Write out primary data index. */
bits64 indexOffset = ftell(f);
cirTreeFileBulkIndexToOpenFile(boundsArray, sizeof(boundsArray[0]), sectionCount,
    blockSize, 1, NULL, boundsArrayFetchKey, boundsArrayFetchOffset, 
    indexOffset, f);

/* Declare arrays and vars that track the zoom levels we actually output. */
bits32 zoomAmounts[maxZoomLevels];
bits64 zoomDataOffsets[maxZoomLevels];
bits64 zoomIndexOffsets[maxZoomLevels];
int zoomLevels = 0;

/* Write out first zoomed section while storing in memory next zoom level. */
if (minDiff > 0)
    {
    bits64 dataSize = indexOffset - dataOffset;
    int maxReducedSize = dataSize/2;
    int initialReduction = 0, initialReducedCount = 0;
    uglyf("minDiff %d\n", minDiff);
    lineFileRewind(lf);
    for (resTry = 0; resTry < resTryCount; ++resTry)
	{
	bits64 reducedSize = resSizes[resTry] * sizeof(struct bbiSummaryOnDisk);
	uglyf("%d scale %d, size %d\n", resTry, resScales[resTry], resSizes[resTry]);
	if (reducedSize <= maxReducedSize)
	    {
	    initialReduction = resScales[resTry];
	    initialReducedCount = resSizes[resTry];
	    break;
	    }
	}
    uglyf("initialReduction %d\n", initialReduction);

    if (initialReduction > 0)
        {
	struct lm *lm = lmInit(0);
	bits64 zoomStartData, zoomStartIndex;
	struct bbiSummary *rezoomedList = writeReducedOnceReturnReducedTwice(usageList, 
		lf, initialReduction, initialReducedCount,
		resIncrement, blockSize, itemsPerSlot, lm, 
		f, &zoomDataOffsets[0], &zoomIndexOffsets[0]);
	zoomLevels = 1;

	int zoomCount = initialReducedCount;
	int reduction = initialReduction * resIncrement;
	while (zoomLevels < maxZoomLevels)
	    {
	    int rezoomCount = slCount(rezoomedList);
	    uglyf("%d in rezoomedList\n", rezoomCount);
	    if (rezoomCount >= zoomCount)
	        break;
	    ++zoomLevels;
	    zoomCount = rezoomCount;
	    zoomDataOffsets[zoomLevels] = ftell(f);
	    zoomIndexOffsets[zoomLevels] = bbiWriteSummaryAndIndex(rezoomedList, 
	    	blockSize, itemsPerSlot, f);
	    rezoomedList = simpleReduce(rezoomedList, reduction, lm);
	    }
	lmCleanup(&lm);
	}

    }
lineFileClose(&lf);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
itemsPerSlot = optionInt("itemsPerSlot", itemsPerSlot);
clipDontDie = optionExists("clip");
if (argc != 4)
    usage();
bedGraphToBigWig(argv[1], argv[2], argv[3]);
return 0;
}
