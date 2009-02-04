#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "sqlNum.h"
#include "localmem.h"
#include "cirTree.h"
#include "bbiFile.h"

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


bits64 bbiWriteSummaryAndIndex(struct bbiSummary *summaryList, 
	int blockSize, int itemsPerSlot, FILE *f)
/* Write out summary and index to summary, returning start position of
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

