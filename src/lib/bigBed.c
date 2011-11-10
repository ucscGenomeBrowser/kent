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
	    if (chr == chromId && rangeIntersection(s, e, start, end) > 0)
		{
		++itemCount;
		if (maxItems > 0 && itemCount > maxItems)
		    break;

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
    int wordCount = chopByWhite(interval->rest, row+3, rowSize-3);
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
