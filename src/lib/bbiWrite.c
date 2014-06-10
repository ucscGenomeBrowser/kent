/* bbiWrite.c - Routines to help write bigWig and bigBed files. See also bbiFile.h */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "sqlNum.h"
#include "zlibFace.h"
#include "cirTree.h"
#include "bPlusTree.h"
#include "bbiFile.h"
#include "obscure.h"

void bbiWriteDummyHeader(FILE *f)
/* Write out all-zero header, just to reserve space for it. */
{
repeatCharOut(f, 0, 64);
}

void bbiWriteDummyZooms(FILE *f)
/* Write out zeroes to reserve space for ten zoom levels. */
{
repeatCharOut(f, 0, bbiMaxZoomLevels * 24);
}

void bbiSummaryElementWrite(FILE *f, struct bbiSummaryElement *sum)
/* Write out summary element to file. */
{
writeOne(f, sum->validCount);
writeOne(f, sum->minVal);
writeOne(f, sum->maxVal);
writeOne(f, sum->sumData);
writeOne(f, sum->sumSquares);
}

static int bbiChromInfoCmp(const void *va, const void *vb)
/* Sort bbiChromInfo.  Unlike most of our sorts this is single rather
 * than double indirect. */
{
const struct bbiChromInfo *a = (const struct bbiChromInfo *)va;
const struct bbiChromInfo *b = (const struct bbiChromInfo *)vb;
return strcmp(a->name, b->name);
}


void bbiWriteChromInfo(struct bbiChromUsage *usageList, int blockSize, FILE *f)
/* Write out information on chromosomes to file. */
{
int chromCount = slCount(usageList);
struct bbiChromUsage *usage;

/* Allocate and fill in array from list. */
struct bbiChromInfo *chromInfoArray = NULL;
int maxChromNameSize = 0;
if (chromCount > 0)
    {
    AllocArray(chromInfoArray, chromCount);
    int i;
    for (i=0, usage = usageList; i<chromCount; ++i, usage = usage->next)
	{
	char *chromName = usage->name;
	int len = strlen(chromName);
	if (len > maxChromNameSize)
	    maxChromNameSize = len;
	chromInfoArray[i].name = chromName;
	chromInfoArray[i].id = usage->id;
	chromInfoArray[i].size = usage->size;
	}

    /* Sort so the b-Tree actually works. */
    qsort(chromInfoArray, chromCount, sizeof(chromInfoArray[0]), bbiChromInfoCmp);
    }

/* Write chromosome bPlusTree */
int chromBlockSize = min(blockSize, chromCount);
bptFileBulkIndexToOpenFile(chromInfoArray, sizeof(chromInfoArray[0]), chromCount, chromBlockSize,
    bbiChromInfoKey, maxChromNameSize, bbiChromInfoVal, 
    sizeof(chromInfoArray[0].id) + sizeof(chromInfoArray[0].size), 
    f);

freeMem(chromInfoArray);
}

void bbiWriteFloat(FILE *f, float val)
/* Write out floating point val to file.  Mostly to convert from double... */
{
writeOne(f, val);
}

struct hash *bbiChromSizesFromFile(char *fileName)
/* Read two column file into hash keyed by chrom. */
{
struct hash *hash = hashNew(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    hashAddInt(hash, row[0], sqlUnsigned(row[1]));

lineFileClose(&lf);
return hash;
}

void bbiChromInfoKey(const void *va, char *keyBuf)
/* Get key field out of bbiChromInfo. */
{
const struct bbiChromInfo *a = ((struct bbiChromInfo *)va);
strcpy(keyBuf, a->name);
}

void *bbiChromInfoVal(const void *va)
/* Get val field out of bbiChromInfo. */
{
const struct bbiChromInfo *a = ((struct bbiChromInfo *)va);
return (void*)(&a->id);
}

void bbiChromUsageFree(struct bbiChromUsage **pUsage)
/* free a single bbiChromUsage structure */
{
struct bbiChromUsage *usage = *pUsage;
if (usage != NULL)
    {
    freeMem(usage->name);
    freez(pUsage);
    }
}

void bbiChromUsageFreeList(struct bbiChromUsage **pList)
/* free a list of bbiChromUsage structures */
{
struct bbiChromUsage *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    bbiChromUsageFree(&el);
    }
*pList = NULL;
}

int bbExIndexMakerMaxIndexField(struct bbExIndexMaker *eim)
/* Return the maximum field we have to index. */
{
int maxIx = 0;
int i;
for (i=0; i<eim->indexCount; ++i)
    {
    int ix = eim->indexFields[i];
    if (ix > maxIx)
        maxIx = ix;
    }
return maxIx;
}

void bbExIndexMakerUpdateMaxFieldSize(struct bbExIndexMaker *eim, char **row)
/* Fold in information about row into bbExIndexMaker into eim->maxFieldSize */
{
int i;
for (i=0; i<eim->indexCount; ++i)
    {
    int rowIx = eim->indexFields[i];
    int size = strlen(row[rowIx]);
    if (size > eim->maxFieldSize[i])
        eim->maxFieldSize[i] = size;
    }
}

struct bbiChromUsage *bbiChromUsageFromBedFile(struct lineFile *lf, struct hash *chromSizesHash, 
	struct bbExIndexMaker *eim, int *retMinDiff, double *retAveSize, bits64 *retBedCount)
/* Go through bed file and collect chromosomes and statistics.  If eim parameter is non-NULL
 * collect max field sizes there too. */
{
int maxRowSize = (eim == NULL ? 3 : bbExIndexMakerMaxIndexField(eim) + 1);
char *row[maxRowSize];
struct hash *uniqHash = hashNew(0);
struct bbiChromUsage *usage = NULL, *usageList = NULL;
int lastStart = -1;
bits32 id = 0;
bits64 totalBases = 0, bedCount = 0;
int minDiff = BIGNUM;

lineFileRemoveInitialCustomTrackLines(lf);

for (;;)
    {
    int rowSize = lineFileChopNext(lf, row, maxRowSize);
    if (rowSize == 0)
        break;
    lineFileExpectAtLeast(lf, maxRowSize, rowSize);
    char *chrom = row[0];
    int start = lineFileNeedNum(lf, row, 1);
    int end = lineFileNeedNum(lf, row, 2);
    if (eim != NULL)
	bbExIndexMakerUpdateMaxFieldSize(eim, row);
    if (start > end)
        {
	    errAbort("end (%d) before start (%d) line %d of %s",
	    	end, start, lf->lineIx, lf->fileName);
	}
    ++bedCount;
    totalBases += (end - start);
    if (usage == NULL || differentString(usage->name, chrom))
        {
	if (hashLookup(uniqHash, chrom))
	    {
	    errAbort("%s is not sorted at line %d.  Please use \"sort -k1,1 -k2,2n\" or bedSort and try again.",
	    	lf->fileName, lf->lineIx);
	    }
	hashAdd(uniqHash, chrom, NULL);
	struct hashEl *chromHashEl = hashLookup(chromSizesHash, chrom);
	if (chromHashEl == NULL)
	    errAbort("%s is not found in chromosome sizes file", chrom);
	int chromSize = ptToInt(chromHashEl->val);
	AllocVar(usage);
	usage->name = cloneString(chrom);
	usage->id = id++;
	usage->size = chromSize;
	slAddHead(&usageList, usage);
	lastStart = -1;
	}
    if (end > usage->size)
        errAbort("End coordinate %d bigger than %s size of %d line %d of %s", end, usage->name, usage->size, lf->lineIx, lf->fileName);
    usage->itemCount += 1;
    if (lastStart >= 0)
        {
	int diff = start - lastStart;
	if (diff < minDiff)
	    {
	    if (diff < 0)
		errAbort("%s is not sorted at line %d.  Please use \"sort -k1,1 -k2,2n\" or bedSort and try again.",
		    lf->fileName, lf->lineIx);
	    minDiff = diff;
	    }
	}
    lastStart = start;
    }
slReverse(&usageList);
double aveSize = 0;
if (bedCount > 0)
    aveSize = (double)totalBases/bedCount;
*retMinDiff = minDiff;
*retAveSize = aveSize;
*retBedCount = bedCount;
freeHash(&uniqHash);
return usageList;
}

int bbiCalcResScalesAndSizes(int aveSize, 
    int resScales[bbiMaxZoomLevels], int resSizes[bbiMaxZoomLevels])
/* Fill in resScales with amount to zoom at each level, and zero out resSizes based
 * on average span. Returns the number of zoom levels we actually will use. */
{
int resTryCount = bbiMaxZoomLevels, resTry;
int resIncrement = bbiResIncrement;
int minZoom = 10;
int res = aveSize;
if (res < minZoom)
    res = minZoom;
for (resTry = 0; resTry < resTryCount; ++resTry)
    {
    resSizes[resTry] = 0;
    resScales[resTry] = res;
    // if aveSize is large, then the initial value of res is large, and we
    // and we cannot do all 10 levels without overflowing res* integers and other related variables.
    if (res > 1000000000) 
	{
	resTryCount = resTry + 1;  
	verbose(2, "resTryCount reduced from 10 to %d\n", resTryCount);
	break;
	}
    res *= resIncrement;
    }
return resTryCount;
}

int bbiWriteZoomLevels(
    struct lineFile *lf,    /* Input file. */
    FILE *f,		    /* Output. */
    int blockSize,	    /* Size of index block */
    int itemsPerSlot,	    /* Number of data points bundled at lowest level. */
    bbiWriteReducedOnceReturnReducedTwice writeReducedOnceReturnReducedTwice,   /* callback */
    int fieldCount,	    /* Number of fields in bed (4 for bedGraph) */
    boolean doCompress,	    /* Do we compress.  Answer really should be yes! */
    bits64 dataSize,	    /* Size of data on disk (after compression if any). */
    struct bbiChromUsage *usageList, /* Result from bbiChromUsageFromBedFile */
    int resTryCount, int resScales[], int resSizes[],   /* How much to zoom at each level */
    bits32 zoomAmounts[bbiMaxZoomLevels],      /* Fills in amount zoomed at each level. */
    bits64 zoomDataOffsets[bbiMaxZoomLevels],  /* Fills in where data starts for each zoom level. */
    bits64 zoomIndexOffsets[bbiMaxZoomLevels], /* Fills in where index starts for each level. */
    struct bbiSummaryElement *totalSum)
/* Write out all the zoom levels and return the number of levels written.  Writes 
 * actual zoom amount and the offsets of the zoomed data and index in the last three
 * parameters.  Sorry for all the parameters - it was this or duplicate a big chunk of
 * code between bedToBigBed and bedGraphToBigWig. */
{
/* Write out first zoomed section while storing in memory next zoom level. */
assert(resTryCount > 0);
int maxReducedSize = dataSize/2;
int initialReduction = 0, initialReducedCount = 0;

/* Figure out initialReduction for zoom - one that is maxReducedSize or less. */
int resTry;
for (resTry = 0; resTry < resTryCount; ++resTry)
    {
    bits64 reducedSize = resSizes[resTry] * sizeof(struct bbiSummaryOnDisk);
    if (doCompress)
	reducedSize /= 2;	// Estimate!
    if (reducedSize <= maxReducedSize)
	{
	initialReduction = resScales[resTry];
	initialReducedCount = resSizes[resTry];
	break;
	}
    }
verbose(2, "initialReduction %d, initialReducedCount = %d\n", 
    initialReduction, initialReducedCount);

/* Force there to always be at least one zoom.  It may waste a little space on small
 * files, but it makes files more uniform, and avoids special case code for calculating
 * overall file summary. */
if (initialReduction == 0)
    {
    initialReduction = resScales[0];
    initialReducedCount = resSizes[0];
    }

/* Call routine to make the initial zoom level and also a bit of work towards further levels. */
struct lm *lm = lmInit(0);
int zoomIncrement = bbiResIncrement;
lineFileRewind(lf);
struct bbiSummary *rezoomedList = writeReducedOnceReturnReducedTwice(usageList, fieldCount,
	lf, initialReduction, initialReducedCount,
	zoomIncrement, blockSize, itemsPerSlot, doCompress, lm, 
	f, &zoomDataOffsets[0], &zoomIndexOffsets[0], totalSum);
verboseTime(2, "writeReducedOnceReturnReducedTwice");
zoomAmounts[0] = initialReduction;
int zoomLevels = 1;

/* Loop around to do any additional levels of zoom. */
int zoomCount = initialReducedCount;
int reduction = initialReduction * zoomIncrement;
while (zoomLevels < bbiMaxZoomLevels)
    {
    int rezoomCount = slCount(rezoomedList);
    if (rezoomCount >= zoomCount)
	break;
    zoomCount = rezoomCount;
    zoomDataOffsets[zoomLevels] = ftell(f);
    zoomIndexOffsets[zoomLevels] = bbiWriteSummaryAndIndex(rezoomedList, 
	blockSize, itemsPerSlot, doCompress, f);
    zoomAmounts[zoomLevels] = reduction;
    ++zoomLevels;
    reduction *= zoomIncrement;
    rezoomedList = bbiSummarySimpleReduce(rezoomedList, reduction, lm);
    }
lmCleanup(&lm);
verboseTime(2, "further reductions");
return zoomLevels;
}


int bbiCountSectionsNeeded(struct bbiChromUsage *usageList, int itemsPerSlot)
/* Count up number of sections needed for data. */
{
struct bbiChromUsage *usage;
int count = 0;
for (usage = usageList; usage != NULL; usage = usage->next)
    {
    int countOne = (usage->itemCount + itemsPerSlot - 1)/itemsPerSlot;
    count += countOne;
    verbose(2, "%s %d, %d blocks of %d\n", usage->name, usage->itemCount, countOne, itemsPerSlot);
    }
return count;
}


void bbiAddToSummary(bits32 chromId, bits32 chromSize, bits32 start, bits32 end, 
	bits32 validCount, double minVal, double maxVal, double sumData, double sumSquares,  
	int reduction, struct bbiSummary **pOutList)
/* Add data range to summary - putting it onto top of list if possible, otherwise
 * expanding list. */
{
struct bbiSummary *sum = *pOutList;
if (end > chromSize)	// Avoid pathological clipping situation on bad input
    end = chromSize;
while (start < end)
    {
    /* See if need to allocate a new summary. */
    if (sum == NULL || sum->chromId != chromId || sum->end <= start)
        {
	struct bbiSummary *newSum;
	AllocVar(newSum);
	newSum->chromId = chromId;
	if (sum == NULL || sum->chromId != chromId || sum->end + reduction <= start)
	    newSum->start = start;
	else
	    newSum->start = sum->end;
	newSum->end = newSum->start + reduction;
	if (newSum->end > chromSize)
	    newSum->end = chromSize;
	newSum->minVal = minVal;
	newSum->maxVal = maxVal;
	sum = newSum;
	slAddHead(pOutList, sum);
	}

    /* Figure out amount of overlap between current summary and item */
    int overlap = rangeIntersection(start, end, sum->start, sum->end);
    if (overlap <= 0) 
	{
        warn("%u %u doesn't intersect %u %u, chromId %u chromSize %u", start, end, sum->start, sum->end, chromId, chromSize);
	internalErr();
	}
    int itemSize = end - start;
    double overlapFactor = (double)overlap/itemSize;

    /* Fold overlapping bits into output. */
    sum->validCount += overlapFactor * validCount;
    if (sum->minVal > minVal)
        sum->minVal = minVal;
    if (sum->maxVal < maxVal)
        sum->maxVal = maxVal;
    sum->sumData += overlapFactor * sumData;
    sum->sumSquares += overlapFactor * sumSquares;

    /* Advance over overlapping bits. */
    start += overlap;
    }
}

void bbiAddRangeToSummary(bits32 chromId, bits32 chromSize, bits32 start, bits32 end, 
	double val, int reduction, struct bbiSummary **pOutList)
/* Add chromosome range to summary - putting it onto top of list if possible, otherwise
 * expanding list. */
{
int size = end-start;
double sum = size*val;
double sumSquares = sum*val;
bbiAddToSummary(chromId, chromSize, start, end, size, val, val, sum, sumSquares, reduction, pOutList);
}

struct bbiSummary *bbiReduceSummaryList(struct bbiSummary *inList, 
	struct bbiChromInfo *chromInfoArray, int reduction)
/* Reduce summary list to another summary list. */
{
struct bbiSummary *outList = NULL;
struct bbiSummary *sum;
for (sum = inList; sum != NULL; sum = sum->next)
    bbiAddToSummary(sum->chromId, chromInfoArray[sum->chromId].size, sum->start, sum->end, sum->validCount, sum->minVal,
    	sum->maxVal, sum->sumData, sum->sumSquares, reduction, &outList);
slReverse(&outList);
return outList;
}

bits64 bbiTotalSummarySize(struct bbiSummary *list)
/* Return size on disk of all summaries. */
{
struct bbiSummary *el;
bits64 total = 0;
for (el = list; el != NULL; el = el->next)
    total += sizeof(struct bbiSummaryOnDisk);
return total;
}


static bits64 bbiSummaryFetchOffset(const void *va, void *context)
/* Fetch bbiSummary file offset for r-tree */
{
const struct bbiSummary *a = *((struct bbiSummary **)va);
return a->fileOffset;
}

static struct cirTreeRange bbiSummaryFetchKey(const void *va, void *context)
/* Fetch bbiSummary key for r-tree */
{
struct cirTreeRange res;
const struct bbiSummary *a = *((struct bbiSummary **)va);
res.chromIx = a->chromId;
res.start = a->start;
res.end = a->end;
return res;
}


static bits64 bbiWriteSummaryAndIndexComp(struct bbiSummary *summaryList, 
	int blockSize, int itemsPerSlot, FILE *f)
/* Write out summary and index to summary uncompressed, returning start position of
 * summary index. */
{
bits32 i, count = slCount(summaryList);
struct bbiSummary **summaryArray;
AllocArray(summaryArray, count);
writeOne(f, count);
struct bbiSummary *summary = summaryList;

/* Figure out max size of uncompressed and compressed blocks. */
bits32 itemSize = sizeof(summary->chromId) + sizeof(summary->start) + sizeof(summary->end) + sizeof(summary->validCount) + 4*sizeof(float);
int uncBufSize = itemSize * itemsPerSlot;
char uncBuf[uncBufSize];
int compBufSize = zCompBufSize(uncBufSize);
char compBuf[compBufSize];

/* Loop through compressing and writing one slot at a time. */
bits32 itemsLeft = count;
int sumIx = 0;
while (itemsLeft > 0)
    {
    bits32 itemsInSlot = itemsLeft;
    if (itemsInSlot > itemsPerSlot)
         itemsInSlot = itemsPerSlot;
    char *writePt = uncBuf;

    bits64 filePos = ftell(f);
    for (i=0; i<itemsInSlot; ++i)
        {
	summaryArray[sumIx++] = summary;
	memWriteOne(&writePt, summary->chromId);
	memWriteOne(&writePt, summary->start);
	memWriteOne(&writePt, summary->end);
	memWriteOne(&writePt, summary->validCount);
	memWriteFloat(&writePt, summary->minVal);
	memWriteFloat(&writePt, summary->maxVal);
	memWriteFloat(&writePt, summary->sumData);
	memWriteFloat(&writePt, summary->sumSquares);
	summary->fileOffset = filePos;
	summary = summary->next;
	if (summary == NULL)
	    break;
	}

    bits32 uncSize = writePt - uncBuf;
    int compSize = zCompress(uncBuf, uncSize, compBuf, compBufSize);
    mustWrite(f, compBuf, compSize);

    itemsLeft -= itemsInSlot;
    }
bits64 indexOffset = ftell(f);
cirTreeFileBulkIndexToOpenFile(summaryArray, sizeof(summaryArray[0]), count,
    blockSize, itemsPerSlot, NULL, bbiSummaryFetchKey, bbiSummaryFetchOffset, 
    indexOffset, f);
freez(&summaryArray);
return indexOffset;
}

static bits64 bbiWriteSummaryAndIndexUnc(struct bbiSummary *summaryList, 
	int blockSize, int itemsPerSlot, FILE *f)
/* Write out summary and index to summary compressed, returning start position of
 * summary index. */
{
bits32 i, count = slCount(summaryList);
struct bbiSummary **summaryArray;
AllocArray(summaryArray, count);
writeOne(f, count);
struct bbiSummary *summary;
for (summary = summaryList, i=0; summary != NULL; summary = summary->next, ++i)
    {
    summaryArray[i] = summary;
    summary->fileOffset = ftell(f);
    writeOne(f, summary->chromId);
    writeOne(f, summary->start);
    writeOne(f, summary->end);
    writeOne(f, summary->validCount);
    bbiWriteFloat(f, summary->minVal);
    bbiWriteFloat(f, summary->maxVal);
    bbiWriteFloat(f, summary->sumData);
    bbiWriteFloat(f, summary->sumSquares);
    }
bits64 indexOffset = ftell(f);
cirTreeFileBulkIndexToOpenFile(summaryArray, sizeof(summaryArray[0]), count,
    blockSize, itemsPerSlot, NULL, bbiSummaryFetchKey, bbiSummaryFetchOffset, 
    indexOffset, f);
freez(&summaryArray);
return indexOffset;
}

bits64 bbiWriteSummaryAndIndex(struct bbiSummary *summaryList, 
	int blockSize, int itemsPerSlot, boolean doCompress, FILE *f)
/* Write out summary and index to summary, returning start position of
 * summary index. */
{
if (doCompress)
    return bbiWriteSummaryAndIndexComp(summaryList, blockSize, itemsPerSlot, f);
else
    return bbiWriteSummaryAndIndexUnc(summaryList, blockSize, itemsPerSlot, f);
}

struct cirTreeRange bbiBoundsArrayFetchKey(const void *va, void *context)
/* Fetch bbiBoundsArray key for r-tree */
{
const struct bbiBoundsArray *a = ((struct bbiBoundsArray *)va);
return a->range;
}

bits64 bbiBoundsArrayFetchOffset(const void *va, void *context)
/* Fetch bbiBoundsArray file offset for r-tree */
{
const struct bbiBoundsArray *a = ((struct bbiBoundsArray *)va);
return a->offset;
}

struct bbiSumOutStream *bbiSumOutStreamOpen(int allocCount, FILE *f, boolean doCompress)
/* Allocate new bbiSumOutStream. */
{
struct bbiSumOutStream *stream;
AllocVar(stream);
AllocArray(stream->array, allocCount);
stream->allocCount = allocCount;
stream->f = f;
stream->doCompress = doCompress;
return stream;
}

void bbiSumOutStreamFlush(struct bbiSumOutStream *stream)
/* Flush out any pending input. */
{
if (stream->elCount != 0)
    {
    int uncSize = stream->elCount * sizeof(stream->array[0]);
    if (stream->doCompress)
        {
	int compBufSize = zCompBufSize(uncSize);
	char compBuf[compBufSize];
	int compSize = zCompress(stream->array, uncSize, compBuf, compBufSize);
	mustWrite(stream->f, compBuf, compSize);
	}
    else
        {
	mustWrite(stream->f, stream->array, uncSize);
	}
    stream->elCount = 0;
    }
}

void bbiSumOutStreamClose(struct bbiSumOutStream **pStream)
/* Free up bbiSumOutStream */
{
struct bbiSumOutStream *stream = *pStream;
if (stream != NULL)
    {
    bbiSumOutStreamFlush(stream);
    freeMem(stream->array);
    freez(pStream);
    }
}

void bbiSumOutStreamWrite(struct bbiSumOutStream *stream, struct bbiSummary *sum)
/* Write out next one to stream. */
{
int elCount = stream->elCount;
struct bbiSummaryOnDisk *a = &stream->array[elCount];
a->chromId = sum->chromId;
a->start = sum->start;
a->end = sum->end;
a->validCount = sum->validCount;
a->minVal = sum->minVal;
a->maxVal = sum->maxVal;
a->sumData = sum->sumData;
a->sumSquares = sum->sumSquares;
elCount += 1;
stream->elCount = elCount;
if (elCount >= stream->allocCount)
    bbiSumOutStreamFlush(stream);    
}

void bbiOutputOneSummaryFurtherReduce(struct bbiSummary *sum, 
	struct bbiSummary **pTwiceReducedList, 
	int doubleReductionSize, struct bbiBoundsArray **pBoundsPt, 
	struct bbiBoundsArray *boundsEnd, struct lm *lm, 
	struct bbiSumOutStream *stream)
/* Write out sum to file, keeping track of minimal info on it in *pBoundsPt, and also adding
 * it to second level summary. */
{
/* Get place to store file offset etc and make sure we have not gone off end. */
struct bbiBoundsArray *bounds = *pBoundsPt;
assert(bounds < boundsEnd);
*pBoundsPt += 1;

/* Fill in bounds info. */
bounds->offset = ftell(stream->f);
bounds->range.chromIx = sum->chromId;
bounds->range.start = sum->start;
bounds->range.end = sum->end;

/* Write out summary info. */
bbiSumOutStreamWrite(stream, sum);

/* Fold summary info into pTwiceReducedList. */
struct bbiSummary *twiceReduced = *pTwiceReducedList;
if (twiceReduced == NULL || twiceReduced->chromId != sum->chromId 
	|| twiceReduced->start + doubleReductionSize < sum->end)
    {
    lmAllocVar(lm, twiceReduced);
    *twiceReduced = *sum;
    slAddHead(pTwiceReducedList, twiceReduced);
    }
else
    {
    twiceReduced->end = sum->end;
    twiceReduced->validCount += sum->validCount;
    if (sum->minVal < twiceReduced->minVal) twiceReduced->minVal = sum->minVal;
    if (sum->maxVal > twiceReduced->maxVal) twiceReduced->maxVal = sum->maxVal;
    twiceReduced->sumData += sum->sumData;
    twiceReduced->sumSquares += sum->sumSquares;
    }
}

struct bbiSummary *bbiSummarySimpleReduce(struct bbiSummary *list, int reduction, struct lm *lm)
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
	assert(newSum->end < sum->end);	// check sorted input assumption
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

