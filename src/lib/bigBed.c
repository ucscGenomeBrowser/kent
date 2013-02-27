/* bigBed - interface to binary file with bed-style values (that is a bunch of
 * possibly overlapping regions. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "rangeTree.h"
#include "cirTree.h"
#include "bPlusTree.h"
#include "basicBed.h"
#include "asParse.h"
#include "zlibFace.h"
#include "sig.h"
#include "udc.h"
#include "bbiFile.h"
#include "bigBed.h"

struct bbiFile *bigBedFileOpen(char *fileName)
/* Open up big bed file. */
{
return bbiFileOpen(fileName, bigBedSig, "big bed");
}

boolean bigBedFileCheckSigs(char *fileName)
/* check file signatures at beginning and end of file */
{
return bbiFileCheckSigs(fileName, bigBedSig, "big bed");
}

struct bigBedInterval *bigBedIntervalQuery(struct bbiFile *bbi, char *chrom,
	bits32 start, bits32 end, int maxItems, struct lm *lm)
/* Get data for interval.  Return list allocated out of lm.  Set maxItems to maximum
 * number of items to return, or to 0 for all items. */
{
struct bigBedInterval *el, *list = NULL;
int itemCount = 0;
bbiAttachUnzoomedCir(bbi);
bits32 chromId;
struct fileOffsetSize *blockList = bbiOverlappingBlocks(bbi, bbi->unzoomedCir,
	chrom, start, end, &chromId);
struct fileOffsetSize *block, *beforeGap, *afterGap;
struct udcFile *udc = bbi->udc;
boolean isSwapped = bbi->isSwapped;
struct dyString *dy = dyStringNew(32);

/* Set up for uncompression optionally. */
char *uncompressBuf = NULL;
if (bbi->uncompressBufSize > 0)
    uncompressBuf = needLargeMem(bbi->uncompressBufSize);

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

	while (blockPt < blockEnd)
	    {
	    /* Read next record into local variables. */
	    bits32 chr = memReadBits32(&blockPt, isSwapped);	// Read and discard chromId
	    bits32 s = memReadBits32(&blockPt, isSwapped);
	    bits32 e = memReadBits32(&blockPt, isSwapped);
	    int c;
	    dyStringClear(dy);
	    while ((c = *blockPt++) >= 0)
		{
		if (c == 0)
		    break;
		dyStringAppendC(dy, c);
		}

	    /* If we're actually in range then copy it into a new  element and add to list. */
	    if (chr == chromId && s < end && e > start)
		{
		++itemCount;
		if (maxItems > 0 && itemCount > maxItems)
		    break;

		lmAllocVar(lm, el);
		el->start = s;
		el->end = e;
		if (dy->stringSize > 0)
		    el->rest = lmCloneString(lm, dy->string);
		el->chromId = chromId;
		slAddHead(&list, el);
		}
	    }
	if (maxItems > 0 && itemCount > maxItems)
	    break;
	blockBuf += block->size;
        }
    if (maxItems > 0 && itemCount > maxItems)
        break;
    freez(&mergedBuf);
    }
freeMem(uncompressBuf);
dyStringFree(&dy);
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
    int wordCount = chopByChar(interval->rest, '\t', row+3, rowSize-3);
    fieldCount += wordCount;
    }
return fieldCount;
}

static struct bbiInterval *bigBedCoverageIntervals(struct bbiFile *bbi,
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
/* Get extended summary information for summarySize evenly spaced elements into
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

void bigBedAttachNameIndex(struct bbiFile *bbi)
/* Attach name index part of bbiFile to bbi */
{
if (bbi->nameBpt == NULL)
    {
    if (bbi->nameIndexOffset == 0)
	errAbort("%s has no name index", bbi->fileName);
    udcSeek(bbi->udc, bbi->nameIndexOffset);
    bbi->nameBpt = bptFileAttach(bbi->fileName, bbi->udc);
    }
}

struct offsetSize 
/* Simple file offset and file size. */
    {
    bits64 offset; 
    bits64 size;
    };

static int cmpOffsetSizeRef(const void *va, const void *vb)
/* Compare to sort slRef pointing to offsetSize.  Sort is kind of hokey,
 * but guarantees all items that are the same will be next to each other
 * at least, which is all we care about. */
{
const struct slRef *a = *((struct slRef **)va);
const struct slRef *b = *((struct slRef **)vb);
return memcmp(a->val, b->val, sizeof(struct offsetSize));
}

static struct fileOffsetSize *bigBedChunksMatchingName(struct bbiFile *bbi, char *name)
/* Get list of file chunks that match name.  Can slFreeList this when done. */
{
bigBedAttachNameIndex(bbi);
struct slRef *blockList = bptFileFindMultiple(bbi->nameBpt, name, strlen(name), sizeof(struct offsetSize));
slSort(&blockList, cmpOffsetSizeRef);

struct fileOffsetSize *fosList = NULL, *fos;
struct offsetSize lastOffsetSize = {0,0};
struct slRef *blockRef;
for (blockRef = blockList; blockRef != NULL; blockRef = blockRef->next)
    {
    if (memcmp(&lastOffsetSize, blockRef->val, sizeof(lastOffsetSize)) != 0)
        {
	memcpy(&lastOffsetSize, blockRef->val, sizeof(lastOffsetSize));
	AllocVar(fos);
	if (bbi->isSwapped)
	    {
	    fos->offset = byteSwap64(lastOffsetSize.offset);
	    fos->size = byteSwap64(lastOffsetSize.size);
	    }
	else
	    {
	    fos->offset = lastOffsetSize.offset;
	    fos->size = lastOffsetSize.size;
	    }
	slAddHead(&fosList, fos);
	}
    }
slRefFreeListAndVals(&blockList);
slReverse(&fosList);
return fosList;
}

struct bigBedInterval *bigBedNameQuery(struct bbiFile *bbi, char *name, struct lm *lm)
/* Return list of intervals matching file. These intervals will be allocated out of lm. */
{
bigBedAttachNameIndex(bbi);
boolean isSwapped = bbi->isSwapped;
struct fileOffsetSize *fos, *fosList = bigBedChunksMatchingName(bbi, name);
struct bigBedInterval *interval, *intervalList = NULL;
for (fos = fosList; fos != NULL; fos = fos->next)
    {
    /* Read in raw data */
    udcSeek(bbi->udc, fos->offset);
    char *rawData = needLargeMem(fos->size);
    udcRead(bbi->udc, rawData, fos->size);

    /* Optionally uncompress data, and set data pointer to uncompressed version. */
    char *uncompressedData = NULL;
    char *data = NULL;
    int dataSize = 0;
    if (bbi->uncompressBufSize > 0)
	{
	data = uncompressedData = needLargeMem(bbi->uncompressBufSize);
	dataSize = zUncompress(rawData, fos->size, uncompressedData, bbi->uncompressBufSize);
	}
    else
	{
        data = rawData;
	dataSize = fos->size;
	}

    /* Set up for "memRead" routines to more or less treat memory block like file */
    char *blockPt = data, *blockEnd = data + dataSize;
    struct dyString *dy = dyStringNew(32); // Keep bits outside of chrom/start/end here


    /* Read next record into local variables. */
    while (blockPt < blockEnd)
	{
	bits32 chromIx = memReadBits32(&blockPt, isSwapped);
	bits32 s = memReadBits32(&blockPt, isSwapped);
	bits32 e = memReadBits32(&blockPt, isSwapped);
	int c;
	dyStringClear(dy);
	while ((c = *blockPt++) >= 0)
	    {
	    if (c == 0)
		break;
	    dyStringAppendC(dy, c);
	    }
	if (startsWithWordByDelimiter(name, '\t', dy->string))
	    {
	    lmAllocVar(lm, interval);
	    interval->start = s;
	    interval->end = e;
	    interval->rest = cloneString(dy->string);
	    interval->chromId = chromIx;
	    slAddHead(&intervalList, interval);
	    }
	}

    /* Clean up temporary buffers. */
    dyStringFree(&dy);
    freez(&uncompressedData);
    freez(&rawData);
    }
slFreeList(&fosList);
slReverse(&intervalList);
return intervalList;
}

void bigBedIntervalListToBedFile(struct bbiFile *bbi, struct bigBedInterval *intervalList, FILE *f)
/* Write out big bed interval list to bed file, looking up chromosome. */
{
char chromName[bbi->chromBpt->keySize+1];
int lastChromId = -1;
struct bigBedInterval *interval;
for (interval = intervalList; interval != NULL; interval = interval->next)
    {
    bbiCachedChromLookup(bbi, interval->chromId, lastChromId, chromName, sizeof(chromName));
    lastChromId = interval->chromId;
    fprintf(f, "%s\t%u\t%u\t%s\n", chromName, interval->start, interval->end, interval->rest);
    }
}

int bigBedIntervalToRowLookupChrom(struct bigBedInterval *interval, 
    struct bigBedInterval *prevInterval, struct bbiFile *bbi,
    char *chromBuf, int chromBufSize, char *startBuf, char *endBuf, char **row, int rowSize)
/* Convert bigBedInterval to array of chars equivalend to what you'd get by parsing the
 * bed file.  If you already know what chromosome the interval is on use the simpler
 * bigBedIntervalToRow.  This one will look up the chromosome based on the chromId field
 * of the interval,  which is relatively time consuming.  To avoid doing this unnecessarily
 * pass in a non-NULL prevInterval,  and if the chromId is the same on prevInterval as this,
 * it will avoid the lookup.  The chromBufSize should be at greater or equal to 
 * bbi->chromBpt->keySize+1. The startBuf and endBuf are used to hold the ascii representation of
 * start and end, and should be 16 bytes.  Note that the interval->rest string will have zeroes 
 * inserted as a side effect.  Returns number of fields in row.  */
{
int lastChromId = (prevInterval == NULL ? -1 : prevInterval->chromId);
bbiCachedChromLookup(bbi, interval->chromId, lastChromId, chromBuf, chromBufSize);
return bigBedIntervalToRow(interval, chromBuf, startBuf, endBuf, row, rowSize);
}

char *bigBedAutoSqlText(struct bbiFile *bbi)
/* Get autoSql text if any associated with file.  Do a freeMem of this when done. */
{
if (bbi->asOffset == 0)
    return NULL;
struct udcFile *f = bbi->udc;
udcSeek(f, bbi->asOffset);
return udcReadStringAndZero(f);
}

struct asObject *bigBedAs(struct bbiFile *bbi)
/* Get autoSql object definition if any associated with file. */
{
if (bbi->asOffset == 0)
    return NULL;
char *asText = bigBedAutoSqlText(bbi);
struct asObject *as = asParseText(asText);
freeMem(asText);
return as;
}

struct asObject *bigBedAsOrDefault(struct bbiFile *bbi)
// Get asObject associated with bigBed - if none exists in file make it up from field counts.
{
struct asObject *as = bigBedAs(bbi);
if (as == NULL)
    as = asParseText(bedAsDef(bbi->definedFieldCount, bbi->fieldCount));
return as;
}

struct asObject *bigBedFileAsObjOrDefault(char *fileName)
// Get asObject associated with bigBed file, or the default.
{
struct bbiFile *bbi = bigBedFileOpen(fileName);
if (bbi)
    {
    struct asObject *as = bigBedAsOrDefault(bbi);
    bbiFileClose(&bbi);
    return as;
    }
return NULL;
}

bits64 bigBedItemCount(struct bbiFile *bbi)
/* Return total items in file. */
{
udcSeek(bbi->udc, bbi->unzoomedDataOffset);
return udcReadBits64(bbi->udc, bbi->isSwapped);
}
