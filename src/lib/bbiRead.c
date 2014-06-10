/* bbiRead - Big Binary Indexed file.  Stuff that's common between bigWig and bigBed on the 
 * read side. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "zlibFace.h"
#include "bPlusTree.h"
#include "hmmstats.h"
#include "cirTree.h"
#include "udc.h"
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

boolean bbiFileCheckSigs(char *fileName, bits32 sig, char *typeName)
/* check file signatures at beginning and end of file */
{
int fd = mustOpenFd(fileName, O_RDONLY);
bits32 magic;
boolean isSwapped = FALSE;

// look for signature at the beginning of the file
mustReadFd(fd, &magic, sizeof(magic));

if (magic != sig)
    {
    magic = byteSwap32(magic);
    isSwapped = TRUE;
    if (magic != sig)
        return FALSE;
    }

// look for signature at the end of the file
mustLseek(fd, -sizeof(magic), SEEK_END);
mustReadFd(fd, &magic, sizeof(magic));
mustCloseFd(&fd);

if (isSwapped)
    {
    magic = byteSwap32(magic);
    if (magic != sig)
        return FALSE;
    }
else
    {
    if (magic != sig)
        return FALSE;
    }

return TRUE;
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
struct udcFile *udc = bbi->udc = udcFileOpen(fileName, udcDefaultDir());

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = FALSE;
udcMustRead(udc, &magic, sizeof(magic));
if (magic != sig)
    {
    magic = byteSwap32(magic);
    isSwapped = TRUE;
    if (magic != sig)
       errAbort("%s is not a %s file", fileName, typeName);
    }
bbi->typeSig = sig;
bbi->isSwapped = isSwapped;

/* Read rest of defined bits of header, byte swapping as needed. */
bbi->version = udcReadBits16(udc, isSwapped);
bbi->zoomLevels = udcReadBits16(udc, isSwapped);
bbi->chromTreeOffset = udcReadBits64(udc, isSwapped);
bbi->unzoomedDataOffset = udcReadBits64(udc, isSwapped);
bbi->unzoomedIndexOffset = udcReadBits64(udc, isSwapped);
bbi->fieldCount = udcReadBits16(udc, isSwapped);
bbi->definedFieldCount = udcReadBits16(udc, isSwapped);
bbi->asOffset = udcReadBits64(udc, isSwapped);
bbi->totalSummaryOffset = udcReadBits64(udc, isSwapped);
bbi->uncompressBufSize = udcReadBits32(udc, isSwapped);
bbi->extensionOffset = udcReadBits64(udc, isSwapped);

/* Read zoom headers. */
int i;
struct bbiZoomLevel *level, *levelList = NULL;
for (i=0; i<bbi->zoomLevels; ++i)
    {
    AllocVar(level);
    level->reductionLevel = udcReadBits32(udc, isSwapped);
    level->reserved = udcReadBits32(udc, isSwapped);
    level->dataOffset = udcReadBits64(udc, isSwapped);
    level->indexOffset = udcReadBits64(udc, isSwapped);
    slAddHead(&levelList, level);
    }
slReverse(&levelList);
bbi->levelList = levelList;

/* Deal with header extension if any. */
if (bbi->extensionOffset != 0)
    {
    udcSeek(udc, bbi->extensionOffset);
    bbi->extensionSize = udcReadBits16(udc, isSwapped);
    bbi->extraIndexCount = udcReadBits16(udc, isSwapped);
    bbi->extraIndexListOffset = udcReadBits64(udc, isSwapped);
    }

/* Attach B+ tree of chromosome names and ids. */
udcSeek(udc, bbi->chromTreeOffset);
bbi->chromBpt =  bptFileAttach(fileName, udc);

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
    slFreeList(&bwf->levelList);
    bptFileDetach(&bwf->chromBpt);
    udcFileClose(&bwf->udc);
    freeMem(bwf->fileName);
    freez(pBwf);
    }
}

static void chromIdSizeHandleSwapped(boolean isSwapped, struct bbiChromIdSize *idSize)
/* Swap bytes in chromosome Id and Size as needed. */
{
if (isSwapped)
    {
    idSize->chromId = byteSwap32(idSize->chromId);
    idSize->chromSize = byteSwap32(idSize->chromSize);
    }
}


struct fileOffsetSize *bbiOverlappingBlocks(struct bbiFile *bbi, struct cirTreeFile *ctf,
	char *chrom, bits32 start, bits32 end, bits32 *retChromId)
/* Fetch list of file blocks that contain items overlapping chromosome range. */
{
struct bbiChromIdSize idSize;
if (!bptFileFind(bbi->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return NULL;
chromIdSizeHandleSwapped(bbi->isSwapped, &idSize);
if (retChromId != NULL)
    *retChromId = idSize.chromId;
return cirTreeFindOverlappingBlocks(ctf, idSize.chromId, start, end);
}

struct chromNameCallbackContext
/* Some stuff that the bPlusTree traverser needs for context. */
    {
    struct bbiChromInfo *list;		/* The list we are building. */
    boolean isSwapped;			/* Need to byte-swap things? */
    };

static void chromNameCallback(void *context, void *key, int keySize, void *val, int valSize)
/* Callback that captures chromInfo from bPlusTree. */
{
struct chromNameCallbackContext *c = context;
struct bbiChromInfo *info;
struct bbiChromIdSize *idSize = val;
assert(valSize == sizeof(*idSize));
chromIdSizeHandleSwapped(c->isSwapped, idSize);
AllocVar(info);
info->name = cloneStringZ(key, keySize);
info->id = idSize->chromId;
info->size = idSize->chromSize;
slAddHead(&c->list, info);
}

struct bbiChromInfo *bbiChromList(struct bbiFile *bbi)
/* Return list of chromosomes. */
{
struct chromNameCallbackContext context;
context.list = NULL;
context.isSwapped = bbi->isSwapped;
bptFileTraverse(bbi->chromBpt, &context, chromNameCallback);
slReverse(&context.list);
return context.list;
}

bits32 bbiChromSize(struct bbiFile *bbi, char *chrom)
/* Return chromosome size, or 0 if no such chromosome in file. */
{
struct bbiChromIdSize idSize;
if (!bptFileFind(bbi->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return 0;
chromIdSizeHandleSwapped(bbi->isSwapped, &idSize);
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
    udcSeek(bbi->udc, bbi->unzoomedIndexOffset);
    bbi->unzoomedCir = cirTreeFileAttach(bbi->fileName, bbi->udc);
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
else if (sameWord(string, "std"))
    return bbiSumStandardDeviation;
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
    case bbiSumStandardDeviation:
        return "std";
    default:
	errAbort("Unknown bbiSummaryType %d", (int)type);
	return NULL;
    }
}

#ifdef UNUSED
static void bbiSummaryOnDiskRead(struct bbiFile *bbi, struct bbiSummaryOnDisk *sum)
/* Read in summary from file. */
{
struct udcFile *udc = bbi->udc;
boolean isSwapped = bbi->isSwapped;
sum->chromId = udcReadBits32(udc, isSwapped);
sum->start = udcReadBits32(udc, isSwapped);
sum->end = udcReadBits32(udc, isSwapped);
sum->validCount = udcReadBits32(udc, isSwapped);
// looks like a bug to me, these should call udcReadFloat() 
udcMustReadOne(udc, sum->minVal);
udcMustReadOne(udc, sum->maxVal);
udcMustReadOne(udc, sum->sumData);
udcMustReadOne(udc, sum->sumSquares);
}
#endif /* UNUSED */

static void bbiSummaryHandleSwapped(struct bbiFile *bbi, struct bbiSummaryOnDisk *in)
/* Swap integer fields in summary as needed. */
{
if (bbi->isSwapped)
    {
    in->chromId = byteSwap32(in->chromId);
    in->start = byteSwap32(in->start);
    in->end = byteSwap32(in->end);
    in->validCount = byteSwap32(in->validCount);
    in->minVal = byteSwapFloat(in->minVal);
    in->maxVal = byteSwapFloat(in->maxVal);
    in->sumData = byteSwapFloat(in->sumData);
    in->sumSquares = byteSwapFloat(in->sumSquares);
    }
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

struct bbiSummary *bbiSummariesInRegion(struct bbiZoomLevel *zoom, struct bbiFile *bbi, 
	int chromId, bits32 start, bits32 end)
/* Return list of all summaries in region at given zoom level of bbiFile. */
{
struct bbiSummary *sumList = NULL, *sum;
struct udcFile *udc = bbi->udc;
udcSeek(udc, zoom->indexOffset);
struct cirTreeFile *ctf = cirTreeFileAttach(bbi->fileName, bbi->udc);
struct fileOffsetSize *blockList = cirTreeFindOverlappingBlocks(ctf, chromId, start, end);
struct fileOffsetSize *block, *beforeGap, *afterGap;

/* Set up for uncompression optionally. */
char *uncompressBuf = NULL;
if (bbi->uncompressBufSize > 0)
    uncompressBuf = needLargeMem(bbi->uncompressBufSize);


/* This loop is a little complicated because we merge the read requests for efficiency, but we 
 * have to then go back through the data one unmerged block at a time. */
for (block = blockList; block != NULL; )
    {
    /* Find contigious blocks and read them into mergedBuf. */
    fileOffsetSizeFindGap(block, &beforeGap, &afterGap);
    bits64 mergedOffset = block->offset;
    bits64 mergedSize = beforeGap->offset + beforeGap->size - mergedOffset;
    udcSeek(udc, mergedOffset);
    char *mergedBuf = needLargeMem(mergedSize);
    udcMustRead(udc, mergedBuf, mergedSize);
    char *blockBuf = mergedBuf;

    /* Loop through individual blocks within merged section. */
    for (;block != afterGap; block = block->next)
        {
	/* Uncompress if necessary. */
	char *blockPt, *blockEnd;
	if (uncompressBuf)
	    {
	    blockPt = uncompressBuf;
	    int uncSize = zUncompress(blockBuf, block->size, uncompressBuf, bbi->uncompressBufSize);
	    blockEnd = blockPt + uncSize;
	    }
	else
	    {
	    blockPt = blockBuf;
	    blockEnd = blockPt + block->size;
	    }

	/* Figure out bounds and number of items in block. */
	int blockSize = blockEnd - blockPt;
	struct bbiSummaryOnDisk *dSum;
	int itemSize = sizeof(*dSum);
	assert(blockSize % itemSize == 0);
	int itemCount = blockSize / itemSize;

	/* Read in items and convert to memory list format. */
	int i;
	for (i=0; i<itemCount; ++i)
	    {
	    dSum = (void *)blockPt;
	    blockPt += sizeof(*dSum);
	    bbiSummaryHandleSwapped(bbi, dSum);
	    if (dSum->chromId == chromId)
		{
		int s = max(dSum->start, start);
		int e = min(dSum->end, end);
		if (s < e)
		    {
		    sum = bbiSummaryFromOnDisk(dSum);
		    slAddHead(&sumList, sum);
		    }
		}
	    }
	assert(blockPt == blockEnd);
	blockBuf += block->size;
        }
    freeMem(mergedBuf);
    }
freeMem(uncompressBuf);
slFreeList(&blockList);
cirTreeFileDetach(&ctf);
slReverse(&sumList);
return sumList;
}

static int normalizeCount(struct bbiSummaryElement *el, double countFactor, 
    double minVal, double maxVal, double sumData, double sumSquares)
/* normalize statistics to be based on an integer number of valid bases 
 * Integer value is the smallest integer not less than countFactor */
{
bits32 validCount = ceil(countFactor);
double normFactor = (double)validCount/countFactor;

el->validCount = validCount;
el->minVal = minVal;
el->maxVal = maxVal;
el->sumData = sumData * normFactor;
el->sumSquares = sumSquares * normFactor;

return validCount;
}

static bits32 bbiSummarySlice(struct bbiFile *bbi, bits32 baseStart, bits32 baseEnd, 
	struct bbiSummary *sumList, struct bbiSummaryElement *el)
/* Update retVal with the average value if there is any data in interval.  Return number
 * of valid data bases in interval. */
{
bits32 validCount = 0;

if (sumList != NULL)
    {
    double minVal = sumList->minVal;
    double maxVal = sumList->maxVal;
    double sumData = 0, sumSquares = 0;
    double countFactor = 0.0;

    struct bbiSummary *sum;
    for (sum = sumList; sum != NULL && sum->start < baseEnd; sum = sum->next)
	{
	int overlap = rangeIntersection(baseStart, baseEnd, sum->start, sum->end);
	if (overlap > 0)
	    {
	    double overlapFactor = (double)overlap / (sum->end - sum->start);
	    countFactor += sum->validCount * overlapFactor;
	    sumData += sum->sumData * overlapFactor;
	    sumSquares += sum->sumSquares * overlapFactor;
	    if (maxVal < sum->maxVal)
		maxVal = sum->maxVal;
	    if (minVal > sum->minVal)
		minVal = sum->minVal;
	    }
	}

    if (countFactor > 0)
	validCount = normalizeCount(el, countFactor, minVal, maxVal, sumData, sumSquares);
    }
return validCount;
}

static int bbiChromId(struct bbiFile *bbi, char *chrom)
/* Return chromosome Id */
{
struct bbiChromIdSize idSize;
if (!bptFileFind(bbi->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return -1;
chromIdSizeHandleSwapped(bbi->isSwapped, &idSize);
return idSize.chromId;
}

static boolean bbiSummaryArrayFromZoom(struct bbiZoomLevel *zoom, struct bbiFile *bbi, 
	char *chrom, bits32 start, bits32 end,
	int summarySize, struct bbiSummaryElement *summary)
/* Look up region in index and get data at given zoom level.  Summarize this data
 * in the summary array. */
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

	if (bbiSummarySlice(bbi, baseStart, baseEnd, sum, &summary[i]))
	    result = TRUE;

	/* Next time round start where we left off. */
	baseStart = baseEnd;
	}
    slFreeList(&sumList);
    }
return result;
}

static bits32 bbiIntervalSlice(struct bbiFile *bbi, bits32 baseStart, bits32 baseEnd, 
	struct bbiInterval *intervalList, struct bbiSummaryElement *el)
/* Update retVal with the average value if there is any data in interval.  Return number
 * of valid data bases in interval. */
{
bits32 validCount = 0;

if (intervalList != NULL)
    {
    double countFactor = 0;
    struct bbiInterval *interval;
    double sumData = 0, sumSquares = 0;
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
	    countFactor += intervalWeight;
	    sumData += interval->val * intervalWeight;
	    sumSquares += interval->val * interval->val * intervalWeight;
	    if (maxVal < interval->val)
		maxVal = interval->val;
	    if (minVal > interval->val)
		minVal = interval->val;
	    }
	}

    validCount = normalizeCount(el, countFactor, minVal, maxVal, sumData, sumSquares);
    }
return validCount;
}


static boolean bbiSummaryArrayFromFull(struct bbiFile *bbi, 
	char *chrom, bits32 start, bits32 end, BbiFetchIntervals fetchIntervals,
	int summarySize, struct bbiSummaryElement *summary)
/* Summarize data, not using zoom. */
{
struct bbiInterval *intervalList = NULL, *interval;
struct lm *lm = lmInit(0);
intervalList = (*fetchIntervals)(bbi, chrom, start, end, lm);
boolean result = FALSE;
if (intervalList != NULL)
    {
    int i;
    bits32 baseStart = start, baseEnd;
    bits32 baseCount = end - start;
    interval = intervalList;
    for (i=0; i<summarySize; ++i)
        {
	/* Calculate end of this part of summary */
	baseEnd = start + (bits64)baseCount*(i+1)/summarySize;
	int end1 = baseEnd;
	if (end1 == baseStart)
	    end1 = baseStart+1;

        /* Advance interval to skip over parts we are no longer interested in. */
	while (interval != NULL && interval->end <= baseStart)
	    interval = interval->next;

	if (bbiIntervalSlice(bbi, baseStart, end1, interval, &summary[i]))
	    result = TRUE;

	/* Next time round start where we left off. */
	baseStart = baseEnd;
	}
    }
lmCleanup(&lm);
return result;
}

boolean bbiSummaryArrayExtended(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	BbiFetchIntervals fetchIntervals,
	int summarySize, struct bbiSummaryElement *summary)
/* Fill in summary with  data from indicated chromosome range in bigWig file. 
 * Returns FALSE if no data at that position. */
{
boolean result = FALSE;

/* Protect from bad input. */
if (start >= end)
    return result;
bzero(summary, summarySize * sizeof(summary[0]));

/* Figure out what size of data we want.  We actually want to get 2 data points per summary
 * value if possible to minimize the effect of a data point being split between summary pixels. */
bits32 baseSize = end - start; 
int fullReduction = (baseSize/summarySize);
int zoomLevel = fullReduction/2;
if (zoomLevel < 0)
    zoomLevel = 0;

/* Get the closest zoom level less than what we're looking for. */
struct bbiZoomLevel *zoom = bbiBestZoom(bbi->levelList, zoomLevel);
if (zoom != NULL)
    result = bbiSummaryArrayFromZoom(zoom, bbi, chrom, start, end, summarySize, summary);
else
    result = bbiSummaryArrayFromFull(bbi, chrom, start, end, fetchIntervals, summarySize, summary);
return result;
}

boolean bbiSummaryArray(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	BbiFetchIntervals fetchIntervals,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */
{
struct bbiSummaryElement *elements;
AllocArray(elements, summarySize);
boolean ret = bbiSummaryArrayExtended(bbi, chrom, start, end, 
	fetchIntervals, summarySize, elements);
if (ret)
    {
    int i;
    double covFactor = (double)summarySize/(end - start);
    for (i=0; i<summarySize; ++i)
        {
	struct bbiSummaryElement *el = &elements[i];
	if (el->validCount > 0)
	    {
	    double val;
	    switch (summaryType)
		{
		case bbiSumMean:
		    val = el->sumData/el->validCount;
		    break;
		case bbiSumMax:
		    val = el->maxVal;
		    break;
		case bbiSumMin:
		    val = el->minVal;
		    break;
		case bbiSumCoverage:
		    val = covFactor*el->validCount;
		    break;
		case bbiSumStandardDeviation:
		    val = calcStdFromSums(el->sumData, el->sumSquares, el->validCount);
		    break;
		default:
		    internalErr();
		    val = 0.0;
		    break;
		}
	    summaryValues[i] = val;
	    }
	}
    }
freeMem(elements);
return ret;
}

struct bbiSummaryElement bbiTotalSummary(struct bbiFile *bbi)
/* Return summary of entire file! */
{
struct udcFile *udc = bbi->udc;
boolean isSwapped = bbi->isSwapped;
struct bbiSummaryElement res;
ZeroVar(&res);

if (bbi->totalSummaryOffset != 0)
    {
    udcSeek(udc, bbi->totalSummaryOffset);
    res.validCount = udcReadBits64(udc, isSwapped);
    res.minVal = udcReadDouble(udc, isSwapped);
    res.maxVal = udcReadDouble(udc, isSwapped);
    res.sumData = udcReadDouble(udc, isSwapped);
    res.sumSquares = udcReadDouble(udc, isSwapped);
    }
else if (bbi->version == 1)
    /* Require version 1 so as not to have to deal with compression.  Should not happen
     * to have NULL totalSummaryOffset for non-empty version 2+ file anyway. */
    {
    /* Find most extreme zoom. */
    struct bbiZoomLevel *bestZoom = NULL, *zoom;
    bits32 bestReduction = 0;
    for (zoom = bbi->levelList; zoom != NULL; zoom = zoom->next)
	{
	if (zoom->reductionLevel > bestReduction)
	    {
	    bestReduction = zoom->reductionLevel;
	    bestZoom = zoom;
	    }
	}

    if (bestZoom != NULL)
	{
	udcSeek(udc, bestZoom->dataOffset);
	bits32 zoomSectionCount = udcReadBits32(udc, isSwapped);
	bits32 i;
	for (i=0; i<zoomSectionCount; ++i)
	    {
	    /* Read, but ignore, position. */
	    bits32 chromId, chromStart, chromEnd;
	    chromId = udcReadBits32(udc, isSwapped);
	    chromStart = udcReadBits32(udc, isSwapped);
	    chromEnd = udcReadBits32(udc, isSwapped);

	    /* First time through set values, rest of time add to them. */
	    if (i == 0)
		{
		res.validCount = udcReadBits32(udc, isSwapped);
		res.minVal = udcReadFloat(udc, isSwapped);
		res.maxVal = udcReadFloat(udc, isSwapped);
		res.sumData = udcReadFloat(udc, isSwapped);
		res.sumSquares = udcReadFloat(udc, isSwapped);
		}
	    else
		{
		res.validCount += udcReadBits32(udc, isSwapped);
		float minVal = udcReadFloat(udc, isSwapped);
		if (minVal < res.minVal) res.minVal = minVal;
		float maxVal = udcReadFloat(udc, isSwapped);
		if (maxVal > res.maxVal) res.maxVal = maxVal;
		res.sumData += udcReadFloat(udc, isSwapped);
		res.sumSquares += udcReadFloat(udc, isSwapped);
		}
	    }
	}
    }
return res;
}

time_t bbiUpdateTime(struct bbiFile *bbi)
/* return bbi->udc->updateTime */
{
struct udcFile *udc = bbi->udc;
return udcUpdateTime(udc);
}

char *bbiCachedChromLookup(struct bbiFile *bbi, int chromId, int lastChromId,
    char *chromBuf, int chromBufSize)
/* Return chromosome name corresponding to chromId.  Because this is a bit expensive,
 * if you are doing this repeatedly pass in the chromId used in the previous call to
 * this in lastChromId,  which will save it from doing the lookup again on the same
 * chromosome.  Pass in -1 to lastChromId if this is the first time or if you can't be
 * bothered.  The chromBufSize should be at greater or equal to bbi->keySize+1.  */
{
if (chromId != lastChromId)
    bptStringKeyAtPos(bbi->chromBpt, chromId, chromBuf, chromBufSize);
return chromBuf;
}

