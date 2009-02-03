/* bbiRead - Big Binary Indexed file.  Stuff that's common between bigWig and bigBed on the 
 * read side. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "localmem.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "bbiFile.h"

struct bbiZoomLevel *bbiBestZoom(struct bbiZoomLevel *levelList, int desiredReduction)
/* Return zoom level that is the closest one that is less than or equal to 
 * desiredReduction. */
{
if (desiredReduction < 0)
   errAbort("bad value %d for desiredReduction in bbiBestZoom", desiredReduction);
if (desiredReduction <= 1)
    return NULL;
int closestDiff = BIGNUM;
struct bbiZoomLevel *closestLevel = NULL;
struct bbiZoomLevel *level;

for (level = levelList; level != NULL; level = level->next)
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

struct bbiFile *bbiFileOpen(char *fileName, bits32 sig, char *typeName)
/* Open up big wig or big bed file. */
{
/* This code needs to agree with code in two other places currently - bigBedFileCreate,
 * and bigWigFileCreate.  I'm thinking of refactoring to share at least between
 * bigBedFileCreate and bigWigFileCreate.  It'd be great so it could be structured
 * so that it could send the input in one chromosome at a time, and send in the zoom
 * stuff only after all the chromosomes are done.  This'd potentially reduce the memory
 * footprint by a factor of 2 or 4.  Still, for now it works. -JK */
struct bbiFile *bbi;
AllocVar(bbi);
bbi->fileName = cloneString(fileName);
FILE *f = bbi->f = mustOpen(fileName, "rb");

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = bbi->isSwapped = FALSE;
mustReadOne(f, magic);
if (magic != sig)
    {
    magic = byteSwap32(magic);
    isSwapped = TRUE;
    if (magic != sig)
       errAbort("%s is not a %s file", fileName, typeName);
    }
bbi->typeSig = sig;

/* Read rest of defined bits of header, byte swapping as needed. */
bbi->version = readBits16(f, isSwapped);
bbi->zoomLevels = readBits16(f, isSwapped);
bbi->chromTreeOffset = readBits64(f, isSwapped);
bbi->unzoomedDataOffset = readBits64(f, isSwapped);
bbi->unzoomedIndexOffset = readBits64(f, isSwapped);

/* Skip over reserved area. */
fseek(f, 32, SEEK_CUR);

/* Read zoom headers. */
int i;
struct bbiZoomLevel *level, *levelList = NULL;
for (i=0; i<bbi->zoomLevels; ++i)
    {
    AllocVar(level);
    level->reductionLevel = readBits32(f, isSwapped);
    level->reserved = readBits32(f, isSwapped);
    level->dataOffset = readBits64(f, isSwapped);
    level->indexOffset = readBits64(f, isSwapped);
    slAddHead(&levelList, level);
    }
slReverse(&levelList);
bbi->levelList = levelList;

/* Attach B+ tree of chromosome names and ids. */
fseek(f, bbi->chromTreeOffset, SEEK_SET);
bbi->chromBpt =  bptFileAttach(fileName, f);

return bbi;
}

void bbiFileClose(struct bbiFile **pBwf)
/* Close down a big wig/big bed file. */
{
struct bbiFile *bwf = *pBwf;
if (bwf != NULL)
    {
    cirTreeFileDetach(&bwf->unzoomedCir);
    slFreeList(&bwf->levelList);
    bptFileDetach(&bwf->chromBpt);
    carefulClose(&bwf->f);
    freeMem(bwf->fileName);
    freez(pBwf);
    }
}


struct fileOffsetSize *bbiOverlappingBlocks(struct bbiFile *bbi, struct cirTreeFile *ctf,
	char *chrom, bits32 start, bits32 end)
/* Fetch list of file blocks that contain items overlapping chromosome range. */
{
struct bbiChromIdSize idSize;
if (!bptFileFind(bbi->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return NULL;
return cirTreeFindOverlappingBlocks(ctf, idSize.chromId, start, end);
}

static void chromNameCallback(void *context, void *key, int keySize, void *val, int valSize)
/* Callback that captures chromInfo from bPlusTree. */
{
struct bbiChromInfo *info;
struct bbiChromIdSize *idSize = val;
assert(valSize == sizeof(*idSize));
AllocVar(info);
info->name = cloneStringZ(key, keySize);
info->id = idSize->chromId;
info->size = idSize->chromSize;
struct bbiChromInfo **pList = context;
slAddHead(pList, info);
}

struct bbiChromInfo *bbiChromList(struct bbiFile *bbi)
/* Return list of chromosomes. */
{
struct bbiChromInfo *list = NULL;
bptFileTraverse(bbi->chromBpt, &list, chromNameCallback);
slReverse(&list);
return list;
}

bits32 bbiChromSize(struct bbiFile *bbi, char *chrom)
/* Return chromosome size, or 0 if no such chromosome in file. */
{
struct bbiChromIdSize idSize;
if (!bptFileFind(bbi->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return 0;
return idSize.chromSize;
}

void bbiChromInfoFree(struct bbiChromInfo **pInfo)
/* Free up one chromInfo */
{
struct bbiChromInfo *info = *pInfo;
if (info != NULL)
    {
    freeMem(info->name);
    freez(pInfo);
    }
}

void bbiChromInfoFreeList(struct bbiChromInfo **pList)
/* Free a list of dynamically allocated bbiChromInfo's */
{
struct bbiChromInfo *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    bbiChromInfoFree(&el);
    }
*pList = NULL;
}

void bbiAttachUnzoomedCir(struct bbiFile *bbi)
/* Make sure unzoomed cir is attached. */
{
if (bbi->unzoomedCir == NULL)
    {
    fseek(bbi->f, bbi->unzoomedIndexOffset, SEEK_SET);
    bbi->unzoomedCir = cirTreeFileAttach(bbi->fileName, bbi->f);
    }
}

enum bbiSummaryType bbiSummaryTypeFromString(char *string)
/* Return summary type given a descriptive string. */
{
if (sameWord(string, "mean") || sameWord(string, "average"))
    return bbiSumMean;
else if (sameWord(string, "max") || sameWord(string, "maximum"))
    return bbiSumMax;
else if (sameWord(string, "min") || sameWord(string, "minimum"))
    return bbiSumMin;
else if (sameWord(string, "coverage") || sameWord(string, "dataCoverage"))
    return bbiSumCoverage;
else
    {
    errAbort("Unknown bbiSummaryType %s", string);
    return bbiSumMean;	/* Keep compiler quiet. */
    }
}

char *bbiSummaryTypeToString(enum bbiSummaryType type)
/* Convert summary type from enum to string representation. */
{
switch (type)
    {
    case bbiSumMean:
        return "mean";
    case bbiSumMax:
        return "max";
    case bbiSumMin:
        return "min";
    case bbiSumCoverage:
        return "coverage";
    default:
	errAbort("Unknown bbiSummaryType %d", (int)type);
	return NULL;
    }
}

static void bbiSummaryOnDiskRead(struct bbiFile *bbi, struct bbiSummaryOnDisk *sum)
/* Read in summary from file. */
{
FILE *f = bbi->f;
boolean isSwapped = bbi->isSwapped;
sum->chromId = readBits32(f, isSwapped);
sum->start = readBits32(f, isSwapped);
sum->end = readBits32(f, isSwapped);
sum->validCount = readBits32(f, isSwapped);
mustReadOne(f, sum->minVal);
mustReadOne(f, sum->maxVal);
mustReadOne(f, sum->sumData);
mustReadOne(f, sum->sumSquares);
}

static struct bbiSummary *bbiSummaryFromOnDisk(struct bbiSummaryOnDisk *in)
/* Create a bbiSummary unlinked to anything from input in onDisk format. */
{
struct bbiSummary *out;
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

static struct bbiSummary *bbiSummariesInRegion(struct bbiZoomLevel *zoom, struct bbiFile *bbi, 
	int chromId, bits32 start, bits32 end)
/* Return list of all summaries in region at given zoom level of bbiFile. */
{
struct bbiSummary *sumList = NULL, *sum;
FILE *f = bbi->f;
fseek(f, zoom->indexOffset, SEEK_SET);
struct cirTreeFile *ctf = cirTreeFileAttach(bbi->fileName, bbi->f);
struct fileOffsetSize *blockList = cirTreeFindOverlappingBlocks(ctf, chromId, start, end);
if (blockList != NULL)
    {
    struct fileOffsetSize *block;
    for (block = blockList; block != NULL; block = block->next)
        {
	fseek(f, block->offset, SEEK_SET);
	struct bbiSummaryOnDisk diskSum;
	int itemSize = sizeof(diskSum);
	assert(block->size % itemSize == 0);
	int itemCount = block->size / itemSize;
	int i;
	for (i=0; i<itemCount; ++i)
	    {
	    bbiSummaryOnDiskRead(bbi, &diskSum);
	    if (diskSum.chromId == chromId)
		{
		int s = max(diskSum.start, start);
		int e = min(diskSum.end, end);
		if (s < e)
		    {
		    sum = bbiSummaryFromOnDisk(&diskSum);
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

static bits32 bbiSummarySlice(struct bbiFile *bbi, bits32 baseStart, bits32 baseEnd, 
	struct bbiSummary *sumList, enum bbiSummaryType summaryType, double *retVal)
/* Update retVal with the average value if there is any data in interval.  Return number
 * of valid data bases in interval. */
{
bits32 validCount = 0;

if (sumList != NULL)
    {
    double minVal = sumList->minVal;
    double maxVal = sumList->maxVal;
    double sumData = 0;

    struct bbiSummary *sum;
    for (sum = sumList; sum != NULL && sum->start < baseEnd; sum = sum->next)
	{
	int overlap = rangeIntersection(baseStart, baseEnd, sum->start, sum->end);
	if (overlap > 0)
	    {
	    double overlapFactor = (double)overlap / (sum->end - sum->start);
	    validCount += sum->validCount * overlapFactor;
	    switch (summaryType)
	        {
		case bbiSumMean:
		    sumData += sum->sumData * overlapFactor;
		    break;
		case bbiSumMax:
		    if (maxVal < sum->maxVal)
		        maxVal = sum->maxVal;
		    break;
		case bbiSumMin:
		    if (minVal > sum->minVal)
		        minVal = sum->minVal;
		    break;
		case bbiSumCoverage:
		    break;
		default:
		    internalErr();
		    break;
		}
	    }
	}
    if (validCount > 0)
	{
	double val = 0;
	switch (summaryType)
	    {
	    case bbiSumMean:
		val = sumData/validCount;
		break;
	    case bbiSumMax:
	        val = maxVal;
		break;
	    case bbiSumMin:
	        val = minVal;
		break;
	    case bbiSumCoverage:
	        val = (double)validCount/(baseEnd-baseStart);
		break;
	    default:
	        internalErr();
		val = 0.0;
		break;
	    }
	*retVal = val;
	}
    }
return validCount;
}

static int bbiChromId(struct bbiFile *bbi, char *chrom)
/* Return chromosome size */
{
struct bbiChromIdSize idSize;
if (!bptFileFind(bbi->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return -1;
return idSize.chromId;
}

boolean bbiSummaryArrayFromZoom(struct bbiZoomLevel *zoom, struct bbiFile *bbi, 
	char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues)
/* Look up region in index and get data at given zoom level.  Summarize this data
 * in the summaryValues array.  Only update summaryValues we actually do have data. */
{
boolean result = FALSE;
int chromId = bbiChromId(bbi, chrom);
if (chromId < 0)
    return FALSE;
struct bbiSummary *sum, *sumList = bbiSummariesInRegion(zoom, bbi, chromId, start, end);
if (sumList != NULL)
    {
    int i;
    bits32 baseStart = start, baseEnd;
    bits32 baseCount = end - start;
    sum = sumList;
    for (i=0; i<summarySize; ++i)
        {
	/* Calculate end of this part of summary */
	baseEnd = start + (bits64)baseCount*(i+1)/summarySize;

        /* Advance sum to skip over parts we are no longer interested in. */
	while (sum != NULL && sum->end <= baseStart)
	    sum = sum->next;

	if (bbiSummarySlice(bbi, baseStart, baseEnd, sum, summaryType, &summaryValues[i]))
	    result = TRUE;

	/* Next time round start where we left off. */
	baseStart = baseEnd;
	}
    slFreeList(&sumList);
    }
return result;
}

static bits32 bbiIntervalSlice(struct bbiFile *bbi, bits32 baseStart, bits32 baseEnd, 
	struct bbiInterval *intervalList, enum bbiSummaryType summaryType, double *retVal)
/* Update retVal with the average value if there is any data in interval.  Return number
 * of valid data bases in interval. */
{
bits32 validCount = 0;

if (intervalList != NULL)
    {
    struct bbiInterval *interval;
    double sumData = 0;
    double minVal = intervalList->val;
    double maxVal = intervalList->val;

    for (interval = intervalList; interval != NULL && interval->start < baseEnd; 
	    interval = interval->next)
	{
	int overlap = rangeIntersection(baseStart, baseEnd, interval->start, interval->end);
	if (overlap > 0)
	    {
	    int intervalSize = interval->end - interval->start;
	    double overlapFactor = (double)overlap / intervalSize;
	    double intervalWeight = intervalSize * overlapFactor;
	    validCount += intervalWeight;

	    switch (summaryType)
	        {
		case bbiSumMean:
		    sumData += interval->val * intervalWeight;
		    break;
		case bbiSumMax:
		    if (maxVal < interval->val)
		        maxVal = interval->val;
		    break;
		case bbiSumMin:
		    if (minVal > interval->val)
		        minVal = interval->val;
		    break;
		case bbiSumCoverage:
		    break;
		default:
		    internalErr();
		    break;
		}
	    }
	}
    if (validCount > 0)
	{
	double val = 0;
	switch (summaryType)
	    {
	    case bbiSumMean:
		val = sumData/validCount;
		break;
	    case bbiSumMax:
		val = maxVal;
		break;
	    case bbiSumMin:
		val = minVal;
		break;
	    case bbiSumCoverage:
		val = (double)validCount/(baseEnd-baseStart);
		break;
	    default:
		internalErr();
		val = 0.0;
		break;
	    }
	*retVal = val;
	}
    }
return validCount;
}


static boolean bbiSummaryArrayFromFull(struct bbiFile *bbi, 
	char *chrom, bits32 start, bits32 end, BbiFetchIntervals fetchIntervals,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues)
/* Summarize data, not using zoom. */
{
struct bbiInterval *intervalList = NULL, *interval;
struct lm *lm = lmInit(0);
intervalList = (*fetchIntervals)(bbi, chrom, start, end, lm);
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

	if (bbiIntervalSlice(bbi, baseStart, baseEnd, interval, summaryType, &summaryValues[i]))
	    result = TRUE;

	/* Next time round start where we left off. */
	baseStart = baseEnd;
	}
    }
lmCleanup(&lm);
return result;
}

boolean bbiSummaryArray(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	BbiFetchIntervals fetchIntervals,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bbiFile file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
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

/* Get the closest zoom level less than what we're looking for. */
struct bbiZoomLevel *zoom = bbiBestZoom(bbi->levelList, zoomLevel);
if (zoom != NULL)
    result = bbiSummaryArrayFromZoom(zoom, bbi, chrom, start, end, 
    	summaryType, summarySize, summaryValues);
else
    result = bbiSummaryArrayFromFull(bbi, chrom, start, end, fetchIntervals,
    	summaryType, summarySize, summaryValues);
return result;
}

