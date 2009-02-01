/* bwQuery - implements the query side of bigWig.  See bwgInternal.h for definition of file
 * format. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "options.h"
#include "sig.h"
#include "sqlNum.h"
#include "obscure.h"
#include "dystring.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "rangeTree.h"
#include "bwgInternal.h"
#include "bigWig.h"
#include "bigBed.h"

static char const rcsid[] = "$Id: bwgQuery.c,v 1.10 2009/02/01 01:26:44 kent Exp $";

void bptDumpCallback(void *context, void *key, int keySize, void *val, int valSize)
{
char *keyString = cloneStringZ(key, keySize);
bits32 *pVal = val;
printf("%s:%d:%u\n", keyString, valSize, *pVal);
freeMem(keyString);
}

struct bigWigFile *bwgFileOpen(char *fileName, bits32 sig, char *typeName)
/* Open up big wig or big bed file. */
{
struct bigWigFile *bwf;
AllocVar(bwf);
bwf->fileName = cloneString(fileName);
FILE *f = bwf->f = mustOpen(fileName, "rb");

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = bwf->isSwapped = FALSE;
mustReadOne(f, magic);
if (magic != sig)
    {
    magic = byteSwap32(magic);
    isSwapped = TRUE;
    if (magic != sig)
       errAbort("%s is not a %s file", fileName, typeName);
    }
bwf->typeSig = sig;

/* Read rest of defined bits of header, byte swapping as needed. */
bwf->zoomLevels = readBits32(f, isSwapped);
bwf->chromTreeOffset = readBits64(f, isSwapped);
bwf->unzoomedDataOffset = readBits64(f, isSwapped);
bwf->unzoomedIndexOffset = readBits64(f, isSwapped);

/* Skip over reserved area. */
fseek(f, 32, SEEK_CUR);

/* Read zoom headers. */
int i;
struct bwgZoomLevel *level, *levelList = NULL;
for (i=0; i<bwf->zoomLevels; ++i)
    {
    AllocVar(level);
    level->reductionLevel = readBits32(f, isSwapped);
    level->reserved = readBits32(f, isSwapped);
    level->dataOffset = readBits64(f, isSwapped);
    level->indexOffset = readBits64(f, isSwapped);
    slAddHead(&levelList, level);
    }
slReverse(&levelList);
bwf->levelList = levelList;

/* Attach B+ tree of chromosome names and ids. */
bwf->chromBpt =  bptFileAttach(fileName, f);

return bwf;
}

struct bigWigFile *bigWigFileOpen(char *fileName)
/* Open up big wig file. */
{
return bwgFileOpen(fileName, bigWigSig, "big wig");
}

struct bigWigFile *bigBedFileOpen(char *fileName)
/* Open up big bed file. */
{
return bwgFileOpen(fileName, bigBedSig, "big bed");
}

void bigWigFileClose(struct bigWigFile **pBwf)
/* Close down a big wig file. */
{
struct bigWigFile *bwf = *pBwf;
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

struct bwgSectionHead
/* A header from a bigWig file section */
    {
    bits32 chromId;	/* Chromosome short identifier. */
    bits32 start,end;	/* Range covered. */
    bits32 itemStep;	/* For some section types, the # of bases between items. */
    bits32 itemSpan;	/* For some section types, the # of bases in each item. */
    UBYTE type;		/* Type byte. */
    UBYTE reserved;	/* Always zero for now. */
    bits16 itemCount;	/* Number of items in block. */
    };

void bwgSectionHeadRead(struct bigWigFile *bwf, struct bwgSectionHead *head)
/* Read section header. */
{
FILE *f = bwf->f;
boolean isSwapped = bwf->isSwapped;
head->chromId = readBits32(f, isSwapped);
head->start = readBits32(f, isSwapped);
head->end = readBits32(f, isSwapped);
head->itemStep = readBits32(f, isSwapped);
head->itemSpan = readBits32(f, isSwapped);
head->type = getc(f);
head->reserved = getc(f);
head->itemCount = readBits16(f, isSwapped);
}

#ifdef DEBUG
void bigWigBlockDump(struct bigWigFile *bwf, char *chrom, FILE *out)
/* Print out info on block starting at current position. */
{
boolean isSwapped = bwf->isSwapped;
FILE *f = bwf->f;
struct bwgSectionHead head;
bwgSectionHeadRead(bwf, &head);
bits16 i;
float val;

switch (head.type)
    {
    case bwgTypeBedGraph:
	{
	fprintf(out, "#bedGraph section %s:%u-%u\n",  chrom, head.start, head.end);
	for (i=0; i<head.itemCount; ++i)
	    {
	    bits32 start = readBits32(f, isSwapped);
	    bits32 end = readBits32(f, isSwapped);
	    mustReadOne(f, val);
	    fprintf(out, "%s\t%u\t%u\t%g\n", chrom, start, end, val);
	    }
	break;
	}
    case bwgTypeVariableStep:
	{
	fprintf(out, "variableStep chrom=%s span=%u\n", chrom, head.itemSpan);
	for (i=0; i<head.itemCount; ++i)
	    {
	    bits32 start = readBits32(f, isSwapped);
	    mustReadOne(f, val);
	    fprintf(out, "%u\t%g\n", start+1, val);
	    }
	break;
	}
    case bwgTypeFixedStep:
	{
	fprintf(out, "fixedStep chrom=%s start=%u step=%u span=%u\n", 
		chrom, head.start, head.itemStep, head.itemSpan);
	for (i=0; i<head.itemCount; ++i)
	    {
	    mustReadOne(f, val);
	    fprintf(out, "%g\n", val);
	    }
	break;
	}
    default:
        internalErr();
	break;
    }
}
#endif /* DEBUG */

struct fileOffsetSize *bigWigOverlappingBlocks(struct bigWigFile *bwf, struct cirTreeFile *ctf,
	char *chrom, bits32 start, bits32 end)
/* Fetch list of file blocks that contain items overlapping chromosome range. */
{
struct bwgChromIdSize idSize;
if (!bptFileFind(bwf->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return NULL;
return cirTreeFindOverlappingBlocks(ctf, idSize.chromId, start, end);
}

void chromNameCallback(void *context, void *key, int keySize, void *val, int valSize)
/* Callback that captures chromInfo from bPlusTree. */
{
struct bigWigChromInfo *info;
struct bwgChromIdSize *idSize = val;
assert(valSize == sizeof(*idSize));
AllocVar(info);
info->name = cloneStringZ(key, keySize);
info->id = idSize->chromId;
info->size = idSize->chromSize;
struct bigWigChromInfo **pList = context;
slAddHead(pList, info);
}

struct bigWigChromInfo *bigWigChromList(struct bigWigFile *bwf)
/* Return list of chromosomes. */
{
struct bigWigChromInfo *list = NULL;
bptFileTraverse(bwf->chromBpt, &list, chromNameCallback);
slReverse(&list);
return list;
}

bits32 bigWigChromSize(struct bigWigFile *bwf, char *chrom)
/* Return chromosome size, or 0 if no such chromosome in file. */
{
struct bwgChromIdSize idSize;
if (!bptFileFind(bwf->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return 0;
return idSize.chromSize;
}

void bigWigChromInfoFree(struct bigWigChromInfo **pInfo)
/* Free up one chromInfo */
{
struct bigWigChromInfo *info = *pInfo;
if (info != NULL)
    {
    freeMem(info->name);
    freez(pInfo);
    }
}

void bigWigChromInfoFreeList(struct bigWigChromInfo **pList)
/* Free a list of dynamically allocated bigWigChromInfo's */
{
struct bigWigChromInfo *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    bigWigChromInfoFree(&el);
    }
*pList = NULL;
}

void bwgAttachUnzoomedCir(struct bigWigFile *bwf)
/* Make sure unzoomed cir is attached. */
{
if (bwf->unzoomedCir == NULL)
    {
    fseek(bwf->f, bwf->unzoomedIndexOffset, SEEK_SET);
    bwf->unzoomedCir = cirTreeFileAttach(bwf->fileName, bwf->f);
    }
}

struct bigWigInterval *bigWigIntervalQuery(struct bigWigFile *bwf, char *chrom, int start, int end,
	struct lm *lm)
/* Get data for interval.  Return list allocated out of lm. */
{
if (bwf->typeSig != bigWigSig)
   errAbort("Trying to do bigWigIntervalQuery on a non big-wig file.");
bwgAttachUnzoomedCir(bwf);
struct bigWigInterval *el, *list = NULL;
struct fileOffsetSize *blockList = bigWigOverlappingBlocks(bwf, bwf->unzoomedCir, 
	chrom, start, end);
struct fileOffsetSize *block;
FILE *f = bwf->f;
boolean isSwapped = bwf->isSwapped;
float val;
int i;
for (block = blockList; block != NULL; block = block->next)
    {
    struct bwgSectionHead head;
    fseek(f, block->offset, SEEK_SET);
    bwgSectionHeadRead(bwf, &head);
    switch (head.type)
	{
	case bwgTypeBedGraph:
	    {
	    for (i=0; i<head.itemCount; ++i)
		{
		bits32 s = readBits32(f, isSwapped);
		bits32 e = readBits32(f, isSwapped);
		mustReadOne(f, val);
		if (s < start) s = start;
		if (e > end) e = end;
		if (s < e)
		    {
		    lmAllocVar(lm, el);
		    el->start = s;
		    el->end = e;
		    el->val = val;
		    slAddHead(&list, el);
		    }
		}
	    break;
	    }
	case bwgTypeVariableStep:
	    {
	    for (i=0; i<head.itemCount; ++i)
		{
		bits32 s = readBits32(f, isSwapped);
		bits32 e = s + head.itemSpan;
		mustReadOne(f, val);
		if (s < start) s = start;
		if (e > end) e = end;
		if (s < e)
		    {
		    lmAllocVar(lm, el);
		    el->start = s;
		    el->end = e;
		    el->val = val;
		    slAddHead(&list, el);
		    }
		}
	    break;
	    }
	case bwgTypeFixedStep:
	    {
	    bits32 s = head.start;
	    bits32 e = s + head.itemSpan;
	    for (i=0; i<head.itemCount; ++i)
		{
		mustReadOne(f, val);
		bits32 clippedS = s, clippedE = e;
		if (clippedS < start) clippedS = start;
		if (clippedE > end) clippedE = end;
		if (clippedS < clippedE)
		    {
		    lmAllocVar(lm, el);
		    el->start = clippedS;
		    el->end = clippedE;
		    el->val = val;
		    slAddHead(&list, el);
		    }
		s += head.itemStep;
		e += head.itemStep;
		}
	    break;
	    }
	default:
	    internalErr();
	    break;
	}
    }
slFreeList(&blockList);
slReverse(&list);
return list;
}

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
    errAbort("Unknown bigWigSummaryType %s", string);
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

struct bwgZoomLevel *bwgBestZoom(struct bwgZoomLevel *levelList, int desiredReduction)
/* Return zoom level that is the closest one that is less than or equal to 
 * desiredReduction. */
{
if (desiredReduction < 0)
   errAbort("bad value %d for desiredReduction in bwgBestZoom", desiredReduction);
if (desiredReduction <= 1)
    return NULL;
int closestDiff = BIGNUM;
struct bwgZoomLevel *closestLevel = NULL;
struct bwgZoomLevel *level;

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

static void bwgSummaryOnDiskRead(struct bigWigFile *bwf, struct bwgSummaryOnDisk *sum)
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

static struct bwgSummary *bwgSummaryFromOnDisk(struct bwgSummaryOnDisk *in)
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

static struct bwgSummary *bwgSummariesInRegion(struct bwgZoomLevel *zoom, struct bigWigFile *bwf, 
	int chromId, bits32 start, bits32 end)
/* Return list of all summaries in region at given zoom level of bigWig. */
{
struct bwgSummary *sumList = NULL, *sum;
FILE *f = bwf->f;
fseek(f, zoom->indexOffset, SEEK_SET);
struct cirTreeFile *ctf = cirTreeFileAttach(bwf->fileName, bwf->f);
struct fileOffsetSize *blockList = cirTreeFindOverlappingBlocks(ctf, chromId, start, end);
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

static bits32 bwgSummarySlice(struct bigWigFile *bwf, bits32 baseStart, bits32 baseEnd, 
	struct bwgSummary *sumList, enum bigWigSummaryType summaryType, double *retVal)
/* Update retVal with the average value if there is any data in interval.  Return number
 * of valid data bases in interval. */
{
bits32 validCount = 0;

if (sumList != NULL)
    {
    double minVal = sumList->minVal;
    double maxVal = sumList->maxVal;
    double sumData = 0;

    struct bwgSummary *sum;
    for (sum = sumList; sum != NULL && sum->start < baseEnd; sum = sum->next)
	{
	int overlap = rangeIntersection(baseStart, baseEnd, sum->start, sum->end);
	if (overlap > 0)
	    {
	    double overlapFactor = (double)overlap / (sum->end - sum->start);
	    validCount += sum->validCount * overlapFactor;
	    switch (summaryType)
	        {
		case bigWigSumMean:
		    sumData += sum->sumData * overlapFactor;
		    break;
		case bigWigSumMax:
		    if (maxVal < sum->maxVal)
		        maxVal = sum->maxVal;
		    break;
		case bigWigSumMin:
		    if (minVal > sum->minVal)
		        minVal = sum->minVal;
		    break;
		case bigWigSumDataCoverage:
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
	    case bigWigSumMean:
		val = sumData/validCount;
		break;
	    case bigWigSumMax:
	        val = maxVal;
		break;
	    case bigWigSumMin:
	        val = minVal;
		break;
	    case bigWigSumDataCoverage:
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

static bits32 bwgIntervalSlice(struct bigWigFile *bwf, bits32 baseStart, bits32 baseEnd, 
	struct bigWigInterval *intervalList, enum bigWigSummaryType summaryType, double *retVal)
/* Update retVal with the average value if there is any data in interval.  Return number
 * of valid data bases in interval. */
{
bits32 validCount = 0;

if (intervalList != NULL)
    {
    struct bigWigInterval *interval;
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
		case bigWigSumMean:
		    sumData += interval->val * intervalWeight;
		    break;
		case bigWigSumMax:
		    if (maxVal < interval->val)
		        maxVal = interval->val;
		    break;
		case bigWigSumMin:
		    if (minVal > interval->val)
		        minVal = interval->val;
		    break;
		case bigWigSumDataCoverage:
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
	    case bigWigSumMean:
		val = sumData/validCount;
		break;
	    case bigWigSumMax:
		val = maxVal;
		break;
	    case bigWigSumMin:
		val = minVal;
		break;
	    case bigWigSumDataCoverage:
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

static int bwgChromId(struct bigWigFile *bwf, char *chrom)
/* Return chromosome size */
{
struct bwgChromIdSize idSize;
if (!bptFileFind(bwf->chromBpt, chrom, strlen(chrom), &idSize, sizeof(idSize)))
    return -1;
return idSize.chromId;
}

static boolean bwgSummaryArrayFromZoom(struct bwgZoomLevel *zoom, struct bigWigFile *bwf, 
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

	if (bwgSummarySlice(bwf, baseStart, baseEnd, sum, summaryType, &summaryValues[i]))
	    result = TRUE;

	/* Next time round start where we left off. */
	baseStart = baseEnd;
	}
    slFreeList(&sumList);
    }
return result;
}

struct bigBedInterval *bigBedIntervalQuery(struct bigWigFile *bwf, char *chrom, int start, int end,
	struct lm *lm)
/* Get data for interval.  Return list allocated out of lm. */
{
bwgAttachUnzoomedCir(bwf);
struct bigBedInterval *el, *list = NULL;
struct fileOffsetSize *blockList = bigWigOverlappingBlocks(bwf, bwf->unzoomedCir, 
	chrom, start, end);
struct fileOffsetSize *block;
FILE *f = bwf->f;
boolean isSwapped = bwf->isSwapped;
float val;
int i;
struct dyString *dy = dyStringNew(32);
for (block = blockList; block != NULL; block = block->next)
    {
    bits64 endPos = block->offset + block->size;
    fseek(f, block->offset, SEEK_SET);
    while (ftell(f) < endPos)
        {
	/* Read next record into local variables. */
	bits32 chromId = readBits32(f, isSwapped);
	bits32 s = readBits32(f, isSwapped);
	bits32 e = readBits32(f, isSwapped);
	int c;
	dyStringClear(dy);
	while ((c = getc(f)) >= 0)
	    {
	    if (c == 0)
	        break;
	    dyStringAppendC(dy, c);
	    }

	/* If we're actually in range then copy it into a new  element and add to list. */
	if (rangeIntersection(s, e, start, end) > 0)
	    {
	    lmAllocVar(lm, el);
	    el->start = s;
	    el->end = e;
	    if (dy->stringSize > 0)
		el->rest = lmCloneString(lm, dy->string);
	    slAddHead(&list, el);
	    }
	}
    }
slFreeList(&blockList);
slReverse(&list);
return list;
}


struct bigWigInterval *bigBedCoverageIntervals(struct bigWigFile *bwf, 
	char *chrom, bits32 start, bits32 end, struct lm *lm)
/* Return intervals where the val is the depth of coverage. */
{
/* Get list of overlapping intervals */
struct bigBedInterval *bi, *biList = bigBedIntervalQuery(bwf, chrom, start, end, lm);
if (biList == NULL)
    return NULL;

/* Make a range tree that collects coverage. */
struct rbTree *rangeTree = rangeTreeNew();
for (bi = biList; bi != NULL; bi = bi->next)
    rangeTreeAddRangeToCoverageDepth(rangeTree, bi->start, bi->end);
struct range *range, *rangeList = rangeTreeList(rangeTree);

/* Convert rangeList to bigWigInterval list. */
struct bigWigInterval *bwi, *bwiList = NULL;
for (range = rangeList; range != NULL; range = range->next)
    {
    lmAllocVar(lm, bwi);
    bwi->start = range->start;
    if (bwi->start < start)
       bwi->start = start;
    bwi->end = range->end;
    if (bwi->end > end)
       bwi->end = end;
    bwi->val = ptToInt(range->val);
    slAddHead(&bwiList, bwi);
    }
slReverse(&bwiList);

/* Clean up and go home. */
rangeTreeFree(&rangeTree);
return bwiList;
}


static boolean bwgSummaryArrayFromFull(struct bigWigFile *bwf, 
	char *chrom, bits32 start, bits32 end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues)
/* Summarize data, not using zoom. */
{
struct bigWigInterval *intervalList = NULL, *interval;
struct lm *lm = lmInit(0);
switch (bwf->typeSig)
    {
    case bigWigSig:
	intervalList = bigWigIntervalQuery(bwf, chrom, start, end, lm);
	break;
    case bigBedSig:
        intervalList = bigBedCoverageIntervals(bwf, chrom, start, end, lm);
	break;
    default:
        internalErr();
	break;
    }
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

	if (bwgIntervalSlice(bwf, baseStart, baseEnd, interval, summaryType, &summaryValues[i]))
	    result = TRUE;

	/* Next time round start where we left off. */
	baseStart = baseEnd;
	}
    }

lmCleanup(&lm);
return result;
}

boolean bwgSummaryArray(struct bigWigFile *bwf, char *chrom, bits32 start, bits32 end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
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
struct bwgZoomLevel *zoom = bwgBestZoom(bwf->levelList, zoomLevel);
if (zoom != NULL)
    {
    result = bwgSummaryArrayFromZoom(zoom, bwf, chrom, start, end, summaryType, summarySize, summaryValues);
    }
else
    {
    result = bwgSummaryArrayFromFull(bwf, chrom, start, end, summaryType, summarySize, summaryValues);
    }
return result;
}

boolean bigWigSummaryArray(char *fileName, char *chrom, bits32 start, bits32 end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */
{
struct bigWigFile *bwf = bigWigFileOpen(fileName);
boolean ret = bwgSummaryArray(bwf, chrom, start, end, summaryType, summarySize, summaryValues);
bigWigFileClose(&bwf);
return ret;
}

boolean bigBedSummaryArray(char *fileName, char *chrom, bits32 start, bits32 end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bigBed file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */
{
struct bigWigFile *bwf = bigBedFileOpen(fileName);
boolean ret = bwgSummaryArray(bwf, chrom, start, end, summaryType, summarySize, summaryValues);
bigBedFileClose(&bwf);
return ret;
}


double bigWigSingleSummary(char *fileName, char *chrom, int start, int end,
    enum bigWigSummaryType summaryType, double defaultVal)
/* Return the summarized single value for a range. */
{
double arrayOfOne = defaultVal;
bigWigSummaryArray(fileName, chrom, start, end, summaryType, 1, &arrayOfOne);
return arrayOfOne;
}

