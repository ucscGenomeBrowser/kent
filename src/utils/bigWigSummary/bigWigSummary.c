/* bigWigSummary - Extract summary information from a bigWig file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "localmem.h"		// Just for development - ugly
#include "bPlusTree.h"		// Just for development - ugly
#include "cirTree.h"		// Just for development - ugly
#include "bwgInternal.h"	// Just for development - ugly
#include "bigWig.h"

static char const rcsid[] = "$Id: bigWigSummary.c,v 1.3 2009/01/29 03:17:40 kent Exp $";

char *summaryType = "mean";


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigSummary - Extract summary information from a bigWig file.\n"
  "usage:\n"
  "   bigWigSummary file.bigWig chrom start end dataPoints\n"
  "Get summary data from bigWig for indicated region, broken into\n"
  "dataPoints equal parts.  (Use dataPoints=1 for simple summary.)\n"
  "options:\n"
  "   -type=X where X is one of:\n"
  "         mean - average value in region\n"
  "         min - minimum value in region\n"
  "         max - maximum value in region\n"
  "         coverage - %% of region that is covered\n"
  );
}

static struct optionSpec options[] = {
   {"summaryType", OPTION_STRING},
   {NULL, 0},
};

enum bigWigSummaryType bigWigSummaryTypeFromString(char *string)
/* Return summary type givin a descriptive string. */
{
if (sameWord(string, "mean") || sameWord(string, "average"))
    return bigWigSumMean;
else if (sameWord(string, "max") || sameWord(string, "maximum"))
    return bigWigSumMax;
else if (sameWord(string, "min") || sameWord(string, "minimum"))
    return bigWigSumMin;
else if (sameWord(string, "coverage") || sameWord(string, "dataCoverage"))
    return bigWigSumDataCoverage;
else
    {
    warn("Unknown summary type %s", string);
    usage();
    return bigWigSumMean;	/* Keep compiler quiet. */
    }
}

char *bigWigSummaryTypeToString(enum bigWigSummaryType type)
/* Convert summary type from enum to string representation. */
{
switch (type)
    {
    case bigWigSumMean:
        return "mean";
    case bigWigSumMax:
        return "max";
    case bigWigSumMin:
        return "min";
    case bigWigSumDataCoverage:
        return "coverage";
    default:
	errAbort("Unknown bigWigSummaryType %d", (int)type);
	return NULL;
    }
}

struct bwgZoomLevel *bwgBestZoom(struct bigWigFile *bwf, int desiredReduction)
/* Return zoom level that is the closest one that is less than or equal to 
 * desiredReduction. */
{
if (desiredReduction < 1)
   errAbort("bad value %d for desiredReduction in bwgBestZoom", desiredReduction);
int closestDiff = BIGNUM;
struct bwgZoomLevel *closestLevel = NULL;
struct bwgZoomLevel *level;

for (level = bwf->levelList; level != NULL; level = level->next)
    {
    int diff = desiredReduction - level->reductionLevel;
    if (diff >= 0 && diff < closestDiff)
        {
	closestDiff = diff;
	closestLevel = level;
	}
    }
return closestLevel;
}

void bwgSummaryOnDiskRead(struct bigWigFile *bwf, struct bwgSummaryOnDisk *sum)
/* Read in summary from file. */
{
FILE *f = bwf->f;
boolean isSwapped = bwf->isSwapped;
sum->chromId = readBits32(f, isSwapped);
sum->start = readBits32(f, isSwapped);
sum->end = readBits32(f, isSwapped);
sum->validCount = readBits32(f, isSwapped);
mustReadOne(f, sum->minVal);
mustReadOne(f, sum->maxVal);
mustReadOne(f, sum->sumData);
mustReadOne(f, sum->sumSquares);
}

struct bwgSummary *bwgSummaryFromOnDisk(struct bwgSummaryOnDisk *in)
/* Create a bwgSummary unlinked to anything from input in onDisk format. */
{
struct bwgSummary *out;
AllocVar(out);
out->chromId = in->chromId;
out->start = in->start;
out->end = in->end;
out->validCount = in->validCount;
out->minVal = in->minVal;
out->maxVal = in->maxVal;
out->sumData = in->sumData;
out->sumSquares = in->sumSquares;
return out;
}

struct bwgSummary *bwgSummaryRead(struct bigWigFile *bwf)
/* Read in a bwgSummaryOnDisk and convert it to a freshly minted bwgSummary. */
{
struct bwgSummaryOnDisk oneSum;
bwgSummaryOnDiskRead(bwf, &oneSum);
return bwgSummaryFromOnDisk(&oneSum);
}

struct bwgSummary *bwgSummariesInRegion(struct bwgZoomLevel *zoom, struct bigWigFile *bwf, 
	int chromId, bits32 start, bits32 end)
/* Return list of all summaries in region at given zoom level of bigWig. */
{
struct bwgSummary *sumList = NULL, *sum;
FILE *f = bwf->f;
fseek(f, zoom->indexOffset, SEEK_SET);
struct cirTreeFile *ctf = cirTreeFileAttach(bwf->fileName, bwf->f);
struct fileOffsetSize *blockList = cirTreeFindOverlappingBlocks(ctf, chromId, start, end);
uglyf("%d blocks in blockList\n", slCount(blockList));
if (blockList != NULL)
    {
    struct fileOffsetSize *block;
    for (block = blockList; block != NULL; block = block->next)
        {
	fseek(f, block->offset, SEEK_SET);
	struct bwgSummaryOnDisk diskSum;
	int itemSize = sizeof(diskSum);
	assert(block->size % itemSize == 0);
	int itemCount = block->size / itemSize;
	int i;
	for (i=0; i<itemCount; ++i)
	    {
	    bwgSummaryOnDiskRead(bwf, &diskSum);
	    if (diskSum.chromId == chromId)
		{
		int s = max(diskSum.start, start);
		int e = min(diskSum.end, end);
		if (s < e)
		    {
		    sum = bwgSummaryFromOnDisk(&diskSum);
		    slAddHead(&sumList, sum);
		    }
		}
	    }
	}
    }
slFreeList(&blockList);
cirTreeFileDetach(&ctf);
slReverse(&sumList);
return sumList;
}

bits32 bwgSummarySlice(struct bigWigFile *bwf, bits32 baseStart, bits32 baseEnd, 
	struct bwgSummary *sumList, double *retVal)
/* Update retVal with the average value if there is any data in interval.  Return number
 * of valid data bases in interval. */
{
struct bwgSummary *sum;
bits32 validCount = 0;
double sumData = 0;

for (sum = sumList; sum != NULL && sum->start < baseEnd; sum = sum->next)
    {
    int overlap = rangeIntersection(baseStart, baseEnd, sum->start, sum->end);
    if (overlap > 0)
        {
	double overlapFactor = (double)overlap / (sum->end - sum->start);
	sumData += sum->sumData * overlapFactor;
	validCount += sum->validCount * overlapFactor;
	}
    }
if (validCount > 0)
    *retVal = sumData/validCount;
return validCount;
}

bits32 bwgIntervalSlice(struct bigWigFile *bwf, bits32 baseStart, bits32 baseEnd, 
	struct bigWigInterval *intervalList, double *retVal)
/* Update retVal with the average value if there is any data in interval.  Return number
 * of valid data bases in interval. */
{
struct bwgSummary *sum;
bits32 validCount = 0;
double sumData = 0;
struct bigWigInterval *interval;
for (interval = intervalList; interval != NULL && interval->start < baseEnd; interval = interval->next)
    {
    int overlap = rangeIntersection(baseStart, baseEnd, interval->start, interval->end);
    if (overlap > 0)
        {
	int intervalSize = interval->end - interval->start;
	double overlapFactor = (double)overlap / (intervalSize);
	sumData += interval->val * intervalSize * overlapFactor;
	validCount += intervalSize * overlapFactor;
	}
    }
if (validCount > 0)
    *retVal = sumData/validCount;
return validCount;
}

int bwgChromId(struct bigWigFile *bwf, char *chrom)
/* Return chromosome size */
{
struct bwgChromIdSize idSize;
if (!bptFileFind(bwf->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return -1;
return idSize.chromId;
}

boolean bwgSummaryArrayFromZoom(struct bwgZoomLevel *zoom, struct bigWigFile *bwf, 
	char *chrom, bits32 start, bits32 end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues)
/* Look up region in index and get data at given zoom level.  Summarize this data
 * in the summaryValues array.  Only update summaryValues we actually do have data. */
{
boolean result = FALSE;
int chromId = bwgChromId(bwf, chrom);
if (chromId < 0)
    return FALSE;
struct bwgSummary *sum, *sumList = bwgSummariesInRegion(zoom, bwf, chromId, start, end);
if (sumList != NULL)
    {
    int i;
    bits32 baseStart = start, baseEnd;
    bits32 baseCount = end - start;
    sum = sumList;
    for (i=0; i<summarySize; ++i)
        {
	/* Calculate end of this part of summary */
	baseEnd = start + baseCount*(i+1)/summarySize;

        /* Advance sum to skip over parts we are no longer interested in. */
	while (sum != NULL && sum->end <= baseStart)
	    sum = sum->next;

	if (bwgSummarySlice(bwf, baseStart, baseEnd, sum, &summaryValues[i]))
	    result = TRUE;

	/* Next time round start where we left off. */
	baseStart = baseEnd;
	}
    }
return result;
}

boolean bwgSummaryArrayFromFull(struct bigWigFile *bwf, char *chrom, bits32 start, bits32 end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues)
{
struct bigWigInterval *intervalList, *interval;
struct lm *lm = lmInit(0);
intervalList = bigWigIntervalQuery(bwf, chrom, start, end, lm);
boolean result = FALSE;
if (intervalList != NULL);
    {
    int i;
    bits32 baseStart = start, baseEnd;
    bits32 baseCount = end - start;
    interval = intervalList;
    for (i=0; i<summarySize; ++i)
        {
	/* Calculate end of this part of summary */
	baseEnd = start + baseCount*(i+1)/summarySize;

        /* Advance interval to skip over parts we are no longer interested in. */
	while (interval != NULL && interval->end <= baseStart)
	    interval = interval->next;

	if (bwgIntervalSlice(bwf, baseStart, baseEnd, interval, &summaryValues[i]))
	    result = TRUE;

	/* Next time round start where we left off. */
	baseStart = baseEnd;
	}
    }

lmCleanup(&lm);
return result;
}

boolean bigWigSummaryArray(char *fileName, char *chrom, bits32 start, bits32 end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan(0) depending on the application.)  Returns FALSE if no data
 * at that position. */
{
boolean result = FALSE;

/* Protect from bad input. */
if (start >= end)
    return result;

/* Figure out what size of data we want.  We actually want to get 4 data points per summary
 * value if possible to minimize the effect of a data point being split between summary pixels. */
bits32 baseSize = end - start; 
int fullReduction = (baseSize/summarySize);
int zoomLevel = fullReduction/4;
if (zoomLevel < 0)
    zoomLevel = 0;

uglyf("bigWigSummaryArray %s %s:%u-%u %s summarySize=%d\n", fileName, chrom, start, end, bigWigSummaryTypeToString(summaryType), summarySize);
uglyf("baseSize %u, summarySize %d, fullReduction %d, zoomLevel %d\n", baseSize, summarySize, fullReduction, zoomLevel);

/* Open up bigWig file and look up chromId. */
struct bigWigFile *bwf = bigWigFileOpen(fileName);

/* Get the closest zoom level less than what we're looking for. */
struct bwgZoomLevel *zoom = bwgBestZoom(bwf, zoomLevel);
if (zoom != NULL)
    {
    uglyf("bwgBestZoom = %u\n", zoom->reductionLevel);
    result = bwgSummaryArrayFromZoom(zoom, bwf, chrom, start, end, summaryType, summarySize, summaryValues);
    }
else
    {
    uglyf("No zoom available, using full resolution.\n");
    result = bwgSummaryArrayFromFull(bwf, chrom, start, end, summaryType, summarySize, summaryValues);
    }
bigWigFileClose(&bwf);
return result;
}

void bigWigSummary(char *bigWigFile, char *chrom, int start, int end, int dataPoints)
/* bigWigSummary - Extract summary information from a bigWig file.. */
{
/* Make up values array initialized to not-a-number. */
double nan0 = nan(NULL);
double summaryValues[dataPoints];
int i;
for (i=0; i<dataPoints; ++i)
    summaryValues[i] = nan0;

if (bigWigSummaryArray(bigWigFile, chrom, start, end, bigWigSummaryTypeFromString(summaryType), 
      dataPoints, summaryValues))
    {
    for (i=0; i<dataPoints; ++i)
	{
	double val = summaryValues[i];
	if (i != 0)
	    printf("\t");
	if (isnan(val))
	    printf("n/a");
	else
	    printf("%g", val);
	}
    printf("\n");
    }
else
    {
    errAbort("no data in region %s:%d-%d in %s\n", chrom, start, end, bigWigFile);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
summaryType = optionVal("summaryType", summaryType);
bigWigSummary(argv[1], argv[2], sqlUnsigned(argv[3]), sqlUnsigned(argv[4]), sqlUnsigned(argv[5]));
return 0;
}
