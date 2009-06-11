/* bigBed - interface to binary file with bed-style values (that is a bunch of
 * possibly overlapping regions. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"
#include "obscure.h"
#include "dystring.h"
#include "rangeTree.h"
#include "cirTree.h"
#include "bPlusTree.h"
#include "basicBed.h"
#include "asParse.h"
#include "sig.h"
#include "udc.h"
#include "bbiFile.h"
#include "bigBed.h"

struct bbiFile *bigBedFileOpen(char *fileName)
/* Open up big bed file. */
{
return bbiFileOpen(fileName, bigBedSig, "big bed");
}

static int ppBedCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,start,end. */
{
const struct ppBed *a = *((struct ppBed **)va);
const struct ppBed *b = *((struct ppBed **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    {
    dif = a->start - b->start;
    if (dif == 0)
        dif = a->end - b->end;
    }
return dif;
}

static struct cirTreeRange ppBedFetchKey(const void *va, void *context)
/* Fetch ppBed key for r-tree */
{
struct cirTreeRange res;
const struct ppBed *a = *((struct ppBed **)va);
res.chromIx = a->chromId;
res.start = a->start;
res.end = a->end;
return res;
}

static bits64 ppBedFetchOffset(const void *va, void *context)
/* Fetch ppBed file offset for r-tree */
{
const struct ppBed *a = *((struct ppBed **)va);
return a->fileOffset;
}

static struct ppBed *ppBedLoadOne(char **row, int fieldCount, struct lineFile *lf, 
	struct hash *chromHash, boolean clip, struct lm *lm, struct asObject *as, bits64 *diskSize)
/* Return a ppBed record from a line of bed file in lf.
   Return the disk size it would occupy in *diskSize.
   row is a preallocated array of pointers to the individual fields in this row to load.
   fieldCount is the number of fields.
   lf is the lineFile the row is coming from, used for error messages and parsing fields.
   chromHash is a hash of the chromosome sizes.
   lm is localMem to allocate ppBed memory from - don't ppBedFreeList or slFree
   list!
   as is the autoSql object describing this bed file or NULL if standard bed.
   */
{
char *chrom = row[0];
int start = lineFileNeedNum(lf, row, 1);
int end = lineFileNeedNum(lf, row, 2);

struct hashEl *hel = hashLookup(chromHash, chrom);
if (hel == NULL)
    errAbort("%s is not in chrom.sizes line %d of %s", chrom, lf->lineIx, lf->fileName);
int chromSize = ptToInt(hel->val);

if (start < 0)
    {
    if (clip)
        start = 0;
    else
        errAbort("Start coordinate %d is negative line %d of %s", start, lf->lineIx, lf->fileName);
    }
if (end > chromSize)
    {
    if (clip)
        end = chromSize;
    else
        errAbort("End coordinate %d is bigger than %s, which just has %d bases. Line %d of %s",
		end, chrom, chromSize, lf->lineIx, lf->fileName);
    }
if (start > end)
    {
    if (clip)
        return NULL;
    else
        errAbort("Start coordinate %d after end coordinate %d line %d of %s",
		start, end, lf->lineIx, lf->fileName);
    }

/* Allocate variable and fill in first three fields. */
struct ppBed *pb;
lmAllocVar(lm, pb);
pb->chrom = hel->name;
pb->start = start;
pb->end = end;
int i;

/* Check remaining fields are formatted right, and concatenate them into "rest" string. */
if (fieldCount > 3)
    {
    /* Count up string sizes and allocate something big enough. */
    int textSize = 0;
    for (i=3; i<fieldCount; ++i)
	textSize += strlen(row[i]) + 1;
    char *s = pb->rest = lmAlloc(lm, textSize);

    /* Go through and check that numerical strings really are numerical. */
    struct asColumn *asCol = slElementFromIx(as->columnList, 3);
    for (i=3; i<fieldCount; ++i)
	{
	enum asTypes type = asCol->lowType->type;
        if (! (asCol->isList || asCol->isArray))
            {
            if (asTypesIsInt(type))
                lineFileNeedFullNum(lf, row, i);
            else if (asTypesIsFloating(type))
                lineFileNeedDouble(lf, row, i);
            }
	int len = strlen(row[i]);
	memcpy(s, row[i], len);
	s[len] = '\t';
	s += len+1;
	asCol = asCol->next;
	}
    /* Convert final tab to a zero. */
    pb->rest[textSize-1] = 0;
    *diskSize = textSize + 3*sizeof(bits32);
    }
else
    {
    *diskSize = 3*sizeof(bits32) + 1;  /* Still will write terminal 0 */
    }
return pb;
}

static struct ppBed *ppBedLoadAll(char *fileName, struct hash *chromHash, struct lm *lm, 
	struct asObject *as, int definedFieldCount, boolean clip,
	bits64 *retDiskSize, bits16 *retFieldCount, boolean *isSorted, 
	bits64 *count, double *avgSize)
/* Read bed file and return it as list of ppBeds. The whole thing will
 * be allocated in the passed in lm - don't ppBedFreeList or slFree
 * list! 
 * Returns TRUE in isSorted if the input file is already sorted,
 * Returns the count of records and the average size of records */
{
struct ppBed *pbList = NULL, *pb;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line;
int fieldCount = 0, fieldAlloc=0;
char **row = NULL;
char *prevChrom = NULL; // set after first time through loop
bits32 prevStart = 0;   // ditto
double totalSize = 0.0;
long n = 0;
*retDiskSize = 0;
*isSorted = TRUE;
while (lineFileNextReal(lf, &line))
    {
    /* First time through figure out the field count, and if not set, the defined field count. */
    if (fieldCount == 0)
	{
	if (as == NULL)
	    {
	    fieldCount = chopByWhite(line, NULL, 0);
	    if (definedFieldCount == 0)
		definedFieldCount = fieldCount;
	    if (as == NULL)
		{
		char *asText = bedAsDef(definedFieldCount, fieldCount);
		as = asParseText(asText);
		freeMem(asText);
		}
	    }
	else
	    {
	    fieldCount = slCount(as->columnList);
	    }
	fieldAlloc = fieldCount+1;
	AllocArray(row, fieldAlloc);
	}
    /* Chop up line and make sure the word count is right. */
    int wordCount = chopByWhite(line, row, fieldAlloc);
    lineFileExpectWords(lf, fieldCount, wordCount);
    bits64 diskSize = 0;
    pb = ppBedLoadOne(row, fieldCount, lf, chromHash, clip, lm, as, &diskSize);
    if (pb == NULL)
        continue;
    if (*isSorted && prevChrom && prevChrom == pb->chrom && pb->start < prevStart)
	*isSorted = FALSE; // first time through the loop prevChrom is NULL so test fails
    prevChrom = pb->chrom;
    prevStart = pb->start;
    ++n;
    totalSize += pb->end - pb->start;
    *retDiskSize += diskSize;
    slAddHead(&pbList, pb);
    }
if (n > 0)
    *avgSize = totalSize/n;
*count = n;
slReverse(&pbList);
freeMem(row);
*retFieldCount = fieldCount;
return pbList;
}

// Not used now, compiler gives a 'unused code' warning if its left in
// static double ppBedAverageSize(struct ppBed *pbList)
// /* Return size of average bed in list. */
// {
// struct ppBed *pb;
// double total = 0.0;
// long count = 0;
// for (pb = pbList; pb != NULL; pb = pb->next)
//     {
//     total += pb->end - pb->start;
//     count += 1;
//     }
// return total/count;
// }

static void makeChromInfo(struct ppBed *pbList, struct hash *chromSizeHash,
	int *retChromCount, struct bbiChromInfo **retChromArray,
	int *retMaxChromNameSize)
/* Fill in chromId field in pbList.  Return array of chromosome name/ids. 
 * The chromSizeHash is keyed by name, and has int values. */
{
/* Build up list of unique chromosome names. */
struct ppBed *pb;
char *chromName = "";
int chromCount = 0;
int maxChromNameSize = 0;
struct slRef *uniq, *uniqList = NULL;
for (pb = pbList; pb != NULL; pb = pb->next)
    {
    if (!sameString(pb->chrom, chromName))
        {
	chromName = pb->chrom;
	refAdd(&uniqList, chromName);
	++chromCount;
	int len = strlen(chromName);
	if (len > maxChromNameSize)
	    maxChromNameSize = len;
	}
    pb->chromId = chromCount-1;
    }
slReverse(&uniqList);

/* Allocate and fill in results array. */
struct bbiChromInfo *chromArray;
AllocArray(chromArray, chromCount);
int i;
for (i = 0, uniq = uniqList; i < chromCount; ++i, uniq = uniq->next)
    {
    chromArray[i].name = uniq->val;
    chromArray[i].id = i;
    chromArray[i].size = hashIntVal(chromSizeHash, uniq->val);
    }

/* Clean up, set return values and go home. */
slFreeList(&uniqList);
*retChromCount = chromCount;
*retChromArray = chromArray;
*retMaxChromNameSize = maxChromNameSize;
}

static struct bbiSummary *summaryOnDepth(struct ppBed *pbList, struct bbiChromInfo *chromInfoArray, 
	int reduction)
/* Produce a summary based on depth of coverage. */
{
struct bbiSummary *outList = NULL;
struct ppBed *chromStart, *chromEnd;

for (chromStart = pbList; chromStart != NULL; chromStart = chromEnd)
    {
    /* Figure out where end of data on this chromosome is. */
    bits32 chromId = chromStart->chromId;
    for (chromEnd = chromStart; chromEnd != NULL; chromEnd = chromEnd->next)
        if (chromEnd->chromId != chromId)
	    break;

    /* Get a range tree */
    struct rbTree *chromRanges = rangeTreeNew();
    struct ppBed *pb;
    for (pb = chromStart; pb != chromEnd; pb = pb->next)
	rangeTreeAddToCoverageDepth(chromRanges, pb->start, pb->end);

    struct range *range, *rangeList = rangeTreeList(chromRanges);
    verbose(3, "Got %d ranges on %d\n", slCount(rangeList), chromId);
    for (range = rangeList; range != NULL; range = range->next)
        {
	bbiAddRangeToSummary(chromId, chromInfoArray[chromId].size, range->start, range->end,
		ptToInt(range->val), reduction, &outList);
	}
    }
slReverse(&outList);
return outList;
}

static void bigBedFileCreateReadInfile(
	char *inName, 	  /* Input file in a tabular bed format <chrom><start><end> + whatever. */
	char *chromSizes, /* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,	  /* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot, /* Number of items in lowest level of tree.  64 is good. */
	bits16 definedFieldCount,  /* Number of defined bed fields - 3-16 or so.  0 means all fields
				    * are the defined bed ones. */
	char *asFileName, /* If non-null points to a .as file that describes fields. */
	boolean clip,     /* If set silently clip out of bound coordinates. */
	char *outName,    /* BigBed output file name. */
	struct ppBed **ppbList,   /* Input bed data, will be sorted. */
	bits64 *count,            /* size of input pbList */
	double *averageSize,      /* average size of elements in pbList */
	struct hash **pChromHash,  /* Hash containing sizes of all chroms. */
	bits16 *fieldCount,       /* actual field count from input data. */
	struct asObject **pAs,    /* If non-null contains as object that describes fields. */
	bits64 *fullSize)         /* full size of ppBed on disk */
/* Load data to prepare bigBed. */
{
/* Load up as object if defined in file. */
struct asObject *as = NULL;
boolean sorted;
if (asFileName != NULL)
    {
    /* Parse it and do sanity check. */
    as = asParseFile(asFileName);
    if (as->next != NULL)
        errAbort("Can only handle .as files containing a single object.");
    }
/* Load in chromosome sizes. */
struct hash *chromHash = bbiChromSizesFromFile(chromSizes);
verbose(1, "Read %d chromosomes and sizes from %s\n",  chromHash->elCount, chromSizes);
/* Load and sort input file. */
struct ppBed *pbList = ppBedLoadAll(inName, chromHash, chromHash->lm, as, 
	definedFieldCount, clip, fullSize, fieldCount, &sorted, count, averageSize);
verbose(1, "Read %llu items from %s\n", *count, inName);
if (!sorted)
    slSort(&pbList, ppBedCmp);
*ppbList = pbList;
*pChromHash = chromHash;
*pAs = as;
}

void bigBedFileCreateDetailed(
	struct ppBed *pbList, 	  /* Input bed data. Must be sorted. */
	bits64 pbCount,           /* size of input pbList */
	double pbAverageSize,     /* average size of elements in pbList */
	char *inName,             /* Input file name (for error message reporting) */
	struct hash *chromHash,   /* Hash containing sizes of all chroms. */
	int blockSize,	  /* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot, /* Number of items in lowest level of tree.  64 is good. */
	bits16 definedFieldCount, /* Number of defined bed fields - 3-16 or so.  0 means all fields
				    * are the defined bed ones. */
	bits16 fieldCount,        /* actual field count from input data. */
	char *asFileName,         /* If non-null points to a .as file that describes fields. */
	struct asObject *as,      /* If non-null contains as object that describes fields. */
	bits64 fullSize,          /* full size of ppBed on disk */
	char *outName)            /* BigBed output file name. */
/* create zoomed bigBed version from ppBed list. */
{
/* This code needs to agree with code in two other places currently - bigWigFileCreate,
 * and bbiFileOpen.  I'm thinking of refactoring to share at least between
 * bigBedFileCreate and bigWigFileCreate.  It'd be great so it could be structured
 * so that it could send the input in one chromosome at a time, and send in the zoom
 * stuff only after all the chromosomes are done.  This'd potentially reduce the memory
 * footprint by a factor of 2 or 4.  Still, for now it works. -JK */

/* Declare a bunch of variables. */
int i;
bits32 reserved32 = 0;
bits64 reserved64 = 0;
bits64 dataOffset = 0, dataOffsetPos;
bits64 indexOffset = 0, indexOffsetPos;
bits64 chromTreeOffset = 0, chromTreeOffsetPos;
bits64 asOffset = 0, asOffsetPos;
bits32 sig = bigBedSig;
struct bbiSummary *reduceSummaries[10];
bits32 reductionAmounts[10];
bits64 reductionDataOffsetPos[10];
bits64 reductionDataOffsets[10];
bits64 reductionIndexOffsets[10];
struct ppBed *pb;

if (definedFieldCount == 0)
    definedFieldCount = fieldCount;

/* Make chromosomes. */
int chromCount, maxChromNameSize;
struct bbiChromInfo *chromInfoArray;
makeChromInfo(pbList, chromHash, &chromCount, &chromInfoArray, &maxChromNameSize);
verbose(1, "%d of %d chromosomes used (%4.2f%%)\n", chromCount, chromHash->elCount, 
	100.0*chromCount/chromHash->elCount);

/* Calculate first meaningful reduction size and make first reduction. */
bits16 summaryCount = 0;
bits16 version = 1;
int initialReduction = pbAverageSize*10;

verbose(2, "averageBedSize=%d\n", initialReduction/10);
if (initialReduction > 10000)
    initialReduction = 10000;
bits64 lastSummarySize = 0, summarySize;
struct bbiSummary *summaryList, *firstSummaryList;
for (;;)
    {
    summaryList = summaryOnDepth(pbList, chromInfoArray, initialReduction);
    summarySize = bbiTotalSummarySize(summaryList);
    if (summarySize >= fullSize && summarySize != lastSummarySize)
        {
	initialReduction *= 2;
	bbiSummaryFreeList(&summaryList);
	lastSummarySize = summarySize;
	}
    else
        break;
    }
summaryCount = 1;
reduceSummaries[0] = firstSummaryList = summaryList;
reductionAmounts[0] = initialReduction;
verbose(1, "Initial zoom reduction x%d data size %4.2f%%\n", 
	initialReduction, 100.0 * summarySize/fullSize);

/* Now calculate up to 10 levels of further summary. */
bits64 reduction = initialReduction;
for (i=0; i<ArraySize(reduceSummaries)-1; i++)
    {
    reduction *= 10;
    if (reduction > 1000000000)
        break;
    summaryList = bbiReduceSummaryList(reduceSummaries[summaryCount-1], chromInfoArray, 
    	reduction);
    summarySize = bbiTotalSummarySize(summaryList);
    if (summarySize != lastSummarySize)
        {
 	reduceSummaries[summaryCount] = summaryList;
	reductionAmounts[summaryCount] = reduction;
	++summaryCount;
	}
    int summaryItemCount = slCount(summaryList);
    if (summaryItemCount <= chromCount)
        break;
    }


/* Open output file and write out fixed size header, including some dummy values for
 * offsets we'll fill in later. */
FILE *f = mustOpen(outName, "wb");
writeOne(f, sig);
writeOne(f, version);
writeOne(f, summaryCount);
chromTreeOffsetPos = ftell(f);
writeOne(f, chromTreeOffset);
dataOffsetPos = ftell(f);
writeOne(f, dataOffset);
indexOffsetPos = ftell(f);
writeOne(f, indexOffset);
writeOne(f, fieldCount);
writeOne(f, definedFieldCount);
asOffsetPos = ftell(f);
writeOne(f, asOffset);
for (i=0; i<5; ++i)
    writeOne(f, reserved32);

/* Write summary headers */
for (i=0; i<summaryCount; ++i)
    {
    writeOne(f, reductionAmounts[i]);
    writeOne(f, reserved32);
    reductionDataOffsetPos[i] = ftell(f);
    writeOne(f, reserved64);	// Fill in with data offset later
    writeOne(f, reserved64);	// Fill in with index offset later
    }

/* Optionally write out .as file. */
if (asFileName != NULL)
    {
    int colCount = slCount(as->columnList);
    if (colCount != fieldCount)
        errAbort("%d columns in %s, %d columns in %s. These must match!",
		colCount, asFileName, fieldCount, inName);
    asOffset = ftell(f);
    FILE *asFile = mustOpen(asFileName, "r");
    copyOpenFile(asFile, f);
    fputc(0, f);
    carefulClose(&asFile);
    }

/* Write chromosome bPlusTree */
chromTreeOffset = ftell(f);
int chromBlockSize = min(blockSize, chromCount);
bptFileBulkIndexToOpenFile(chromInfoArray, sizeof(chromInfoArray[0]), chromCount, chromBlockSize,
    bbiChromInfoKey, maxChromNameSize, bbiChromInfoVal, 
    sizeof(chromInfoArray[0].id) + sizeof(chromInfoArray[0].size), 
    f);

/* Write the main data. */
dataOffset = ftell(f);
writeOne(f, pbCount);
for (pb = pbList; pb != NULL; pb = pb->next)
    {
    pb->fileOffset = ftell(f);
    writeOne(f, pb->chromId);
    writeOne(f, pb->start);
    writeOne(f, pb->end);
    if (pb->rest != NULL)
	{
	int len = 0;
        len = strlen(pb->rest);
	mustWrite(f, pb->rest, len);
	}
    putc(0, f);
    }

/* Write the main index. */
indexOffset = ftell(f);
struct ppBed **pbArray;
AllocArray(pbArray, pbCount);
for (pb = pbList, i=0; pb != NULL; pb = pb->next, ++i)
    pbArray[i] = pb;
cirTreeFileBulkIndexToOpenFile(pbArray, sizeof(pbArray[0]), pbCount,
    blockSize, itemsPerSlot, NULL, ppBedFetchKey, ppBedFetchOffset, 
    indexOffset, f);
freez(&pbArray);

/* Write out summary sections. */
verbose(2, "bigBedFileCreate writing %d summaries\n", summaryCount);
for (i=0; i<summaryCount; ++i)
    {
    reductionDataOffsets[i] = ftell(f);
    reductionIndexOffsets[i] = bbiWriteSummaryAndIndex(reduceSummaries[i], blockSize, itemsPerSlot, f);
    }

/* Go back and fill in offsets properly in header. */
fseek(f, dataOffsetPos, SEEK_SET);
writeOne(f, dataOffset);
fseek(f, indexOffsetPos, SEEK_SET);
writeOne(f, indexOffset);
fseek(f, chromTreeOffsetPos, SEEK_SET);
writeOne(f, chromTreeOffset);
if (asOffset != 0)
    {
    fseek(f, asOffsetPos, SEEK_SET);
    writeOne(f, asOffset);
    }

/* Also fill in offsets in zoom headers. */
for (i=0; i<summaryCount; ++i)
    {
    fseek(f, reductionDataOffsetPos[i], SEEK_SET);
    writeOne(f, reductionDataOffsets[i]);
    writeOne(f, reductionIndexOffsets[i]);
    }

carefulClose(&f);
freez(&chromInfoArray);
}

void bigBedFileCreate(
	char *inName, 	  /* Input file in a tabular bed format <chrom><start><end> + whatever. */
	char *chromSizes, /* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,	  /* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot, /* Number of items in lowest level of tree.  64 is good. */
	bits16 definedFieldCount,  /* Number of defined bed fields - 3-16 or so.  0 means all fields
				    * are the defined bed ones. */
	char *asFileName, /* If non-null points to a .as file that describes fields. */
	boolean clip,     /* If set silently clip out of bound coordinates. */
	char *outName)    /* BigBed output file name. */
/* Convert tab-separated bed file to binary indexed, zoomed bigBed version. */
{
bits16 fieldCount;
bits64 fullSize;
bits64 count;
double averageSize = 0;
struct asObject *as = NULL;
struct hash *chromHash = NULL;
struct ppBed *pbList = NULL;
bigBedFileCreateReadInfile(inName, chromSizes, blockSize, itemsPerSlot, definedFieldCount, 
	asFileName, clip, outName, &pbList, &count, &averageSize, &chromHash, &fieldCount, &as, 
	&fullSize);
bigBedFileCreateDetailed(pbList, count, averageSize, inName, chromHash, blockSize, itemsPerSlot, 
    definedFieldCount, fieldCount, asFileName, as, fullSize, outName);
}

struct bigBedInterval *bigBedIntervalQuery(struct bbiFile *bbi, char *chrom, 
	bits32 start, bits32 end, int maxItems, struct lm *lm)
/* Get data for interval.  Return list allocated out of lm.  Set maxItems to maximum
 * number of items to return, or to 0 for all items. */
{
int itemCount = 0;
bbiAttachUnzoomedCir(bbi);
struct bigBedInterval *el, *list = NULL;
bits32 chromId;
struct fileOffsetSize *blockList = bbiOverlappingBlocks(bbi, bbi->unzoomedCir, 
	chrom, start, end, &chromId);
struct fileOffsetSize *block;
struct udcFile *f = bbi->udc;
boolean isSwapped = bbi->isSwapped;
struct dyString *dy = dyStringNew(32);
for (block = blockList; block != NULL; block = block->next)
    {
    bits64 endPos = block->offset + block->size;
    udcSeek(f, block->offset);
    while (udcTell(f) < endPos)
        {
	++itemCount;
	if (maxItems > 0 && itemCount > maxItems)
	    break;

	/* Read next record into local variables. */
	bits32 chr = udcReadBits32(f, isSwapped);	// Read and discard chromId
	bits32 s = udcReadBits32(f, isSwapped);
	bits32 e = udcReadBits32(f, isSwapped);
	int c;
	dyStringClear(dy);
	while ((c = udcGetChar(f)) >= 0)
	    {
	    if (c == 0)
	        break;
	    dyStringAppendC(dy, c);
	    }

	/* If we're actually in range then copy it into a new  element and add to list. */
	if (chr == chromId && rangeIntersection(s, e, start, end) > 0)
	    {
	    lmAllocVar(lm, el);
	    el->start = s;
	    el->end = e;
	    if (dy->stringSize > 0)
		el->rest = lmCloneString(lm, dy->string);
	    slAddHead(&list, el);
	    }
	}
    if (maxItems > 0 && itemCount > maxItems)
        break;
    }
slFreeList(&blockList);
slReverse(&list);
return list;
}

int bigBedIntervalToRow(struct bigBedInterval *interval, char *chrom, char *startBuf, char *endBuf,
	char **row, int rowSize)
/* Convert bigBedInterval into an array of chars equivalent to what you'd get by
 * parsing the bed file. The startBuf and endBuf are used to hold the ascii representation of
 * start and end.  Note that the interval->rest string will have zeroes inserted as a side effect. 
 */
{
int fieldCount = 3;
sprintf(startBuf, "%u", interval->start);
sprintf(endBuf, "%u", interval->end);
row[0] = chrom;
row[1] = startBuf;
row[2] = endBuf;
if (!isEmpty(interval->rest))
    {
    int wordCount = chopByWhite(interval->rest, row+3, rowSize-3);
    fieldCount += wordCount;
    }
return fieldCount;
}

struct bbiInterval *bigBedCoverageIntervals(struct bbiFile *bbi, 
	char *chrom, bits32 start, bits32 end, struct lm *lm)
/* Return intervals where the val is the depth of coverage. */
{
/* Get list of overlapping intervals */
struct bigBedInterval *bi, *biList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
if (biList == NULL)
    return NULL;

/* Make a range tree that collects coverage. */
struct rbTree *rangeTree = rangeTreeNew();
for (bi = biList; bi != NULL; bi = bi->next)
    rangeTreeAddToCoverageDepth(rangeTree, bi->start, bi->end);
struct range *range, *rangeList = rangeTreeList(rangeTree);

/* Convert rangeList to bbiInterval list. */
struct bbiInterval *bwi, *bwiList = NULL;
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

boolean bigBedSummaryArrayExtended(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	int summarySize, struct bbiSummaryElement *summary)
/* Get extended summary information for summarySize evenely spaced elements into
 * the summary array. */
{
return bbiSummaryArrayExtended(bbi, chrom, start, end, bigBedCoverageIntervals,
	summarySize, summary);
}

boolean bigBedSummaryArray(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bigBed file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */
{
return bbiSummaryArray(bbi, chrom, start, end, bigBedCoverageIntervals,
	summaryType, summarySize, summaryValues);
}


struct asObject *bigBedAs(struct bbiFile *bbi)
/* Get autoSql object definition if any associated with file. */
{
if (bbi->asOffset == 0)
    return NULL;
struct udcFile *f = bbi->udc;
udcSeek(f, bbi->asOffset);
char *asText = udcReadStringAndZero(f);
struct asObject *as = asParseText(asText);
freeMem(asText);
return as;
}

bits64 bigBedItemCount(struct bbiFile *bbi)
/* Return total items in file. */
{
udcSeek(bbi->udc, bbi->unzoomedDataOffset);
return udcReadBits64(bbi->udc, bbi->isSwapped);
}
