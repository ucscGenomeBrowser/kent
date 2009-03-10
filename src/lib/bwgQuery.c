/* bwgQuery - implements the query side of bigWig.  See bwgInternal.h for definition of file
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
#include "udc.h"
#include "bbiFile.h"
#include "bwgInternal.h"
#include "bigWig.h"
#include "bigBed.h"

static char const rcsid[] = "$Id: bwgQuery.c,v 1.21 2009/03/10 01:14:53 kent Exp $";

struct bbiFile *bigWigFileOpen(char *fileName)
/* Open up big wig file. */
{
return bbiFileOpen(fileName, bigWigSig, "big wig");
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

void bwgSectionHeadRead(struct bbiFile *bwf, struct bwgSectionHead *head)
/* Read section header. */
{
struct udcFile *udc = bwf->udc;
boolean isSwapped = bwf->isSwapped;
head->chromId = udcReadBits32(udc, isSwapped);
head->start = udcReadBits32(udc, isSwapped);
head->end = udcReadBits32(udc, isSwapped);
head->itemStep = udcReadBits32(udc, isSwapped);
head->itemSpan = udcReadBits32(udc, isSwapped);
head->type = udcGetChar(udc);
head->reserved = udcGetChar(udc);
head->itemCount = udcReadBits16(udc, isSwapped);
}

void bwgSectionHeadFromMem(char **pPt, struct bwgSectionHead *head, boolean isSwapped)
/* Read section header. */
{
char *pt = *pPt;
head->chromId = memReadBits32(&pt, isSwapped);
head->start = memReadBits32(&pt, isSwapped);
head->end = memReadBits32(&pt, isSwapped);
head->itemStep = memReadBits32(&pt, isSwapped);
head->itemSpan = memReadBits32(&pt, isSwapped);
head->type = *pt++;
head->reserved = *pt++;
head->itemCount = memReadBits16(&pt, isSwapped);
*pPt = pt;
}

static int bigWigBlockDumpIntersectingRange(struct bbiFile *bwf, char *chrom, 
	bits32 rangeStart, bits32 rangeEnd, int maxCount, FILE *out)
/* Print out info on parts of block that intersect start-end, block starting at current position. */
{
boolean isSwapped = bwf->isSwapped;
struct udcFile *udc = bwf->udc;
struct bwgSectionHead head;
bwgSectionHeadRead(bwf, &head);
bits16 i;
float val;
int outCount = 0;

switch (head.type)
    {
    case bwgTypeBedGraph:
	{
	fprintf(out, "#bedGraph section %s:%u-%u\n",  chrom, head.start, head.end);
	for (i=0; i<head.itemCount; ++i)
	    {
	    bits32 start = udcReadBits32(udc, isSwapped);
	    bits32 end = udcReadBits32(udc, isSwapped);
	    udcMustReadOne(udc, val);
	    if (rangeIntersection(rangeStart, rangeEnd, start, end) > 0)
		{
		fprintf(out, "%s\t%u\t%u\t%g\n", chrom, start, end, val);
		++outCount;
		if (maxCount != 0 && outCount >= maxCount)
		    break;
		}
	    }
	break;
	}
    case bwgTypeVariableStep:
	{
	fprintf(out, "variableStep chrom=%s span=%u\n", chrom, head.itemSpan);
	for (i=0; i<head.itemCount; ++i)
	    {
	    bits32 start = udcReadBits32(udc, isSwapped);
	    udcMustReadOne(udc, val);
	    if (rangeIntersection(rangeStart, rangeEnd, start, start+head.itemSpan) > 0)
		{
		fprintf(out, "%u\t%g\n", start+1, val);
		++outCount;
		if (maxCount != 0 && outCount >= maxCount)
		    break;
		}
	    }
	break;
	}
    case bwgTypeFixedStep:
	{
	boolean gotStart = FALSE;
	bits32 start = head.start;
	for (i=0; i<head.itemCount; ++i)
	    {
	    udcMustReadOne(udc, val);
	    if (rangeIntersection(rangeStart, rangeEnd, start, start+head.itemSpan) > 0)
	        {
		if (!gotStart)
		    {
		    fprintf(out, "fixedStep chrom=%s start=%u step=%u span=%u\n", 
			    chrom, start, head.itemStep, head.itemSpan);
		    gotStart = TRUE;
		    }
		fprintf(out, "%g\n", val);
		++outCount;
		if (maxCount != 0 && outCount >= maxCount)
		    break;
		}
	    start += head.itemStep;
	    }
	break;
	}
    default:
        internalErr();
	break;
    }
return outCount;
}

void bigWigBlockDump(struct bbiFile *bwf, char *chrom, FILE *out)
/* Print out info on block starting at current position. */
{
bigWigBlockDumpIntersectingRange(bwf, chrom, 0, BIGNUM, 0, out);
}


struct bbiInterval *bigWigIntervalQuery(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end,
	struct lm *lm)
/* Get data for interval.  Return list allocated out of lm. */
{
if (bwf->typeSig != bigWigSig)
   errAbort("Trying to do bigWigIntervalQuery on a non big-wig file.");
bbiAttachUnzoomedCir(bwf);
struct bbiInterval *el, *list = NULL;
struct fileOffsetSize *blockList = bbiOverlappingBlocks(bwf, bwf->unzoomedCir, 
	chrom, start, end, NULL);
struct fileOffsetSize *block;
struct udcFile *udc = bwf->udc;
boolean isSwapped = bwf->isSwapped;
float val;
int i;

// slSort(&blockList, fileOffsetSizeCmp);
struct fileOffsetSize *mergedBlocks = fileOffsetSizeMerge(blockList);
for (block = mergedBlocks; block != NULL; block = block->next)
    {
    udcSeek(udc, block->offset);
    char *blockBuf = needLargeMem(block->size);
    udcRead(udc, blockBuf, block->size);
    char *blockPt = blockBuf, *blockEnd = blockBuf + block->size;
    while (blockPt < blockEnd)
	{
	struct bwgSectionHead head;
	bwgSectionHeadFromMem(&blockPt, &head, isSwapped);
	switch (head.type)
	    {
	    case bwgTypeBedGraph:
		{
		for (i=0; i<head.itemCount; ++i)
		    {
		    bits32 s = memReadBits32(&blockPt, isSwapped);
		    bits32 e = memReadBits32(&blockPt, isSwapped);
		    val = memReadFloat(&blockPt, isSwapped);
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
		    bits32 s = memReadBits32(&blockPt, isSwapped);
		    bits32 e = s + head.itemSpan;
		    val = memReadFloat(&blockPt, isSwapped);
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
		    val = memReadFloat(&blockPt, isSwapped);
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
    }
slFreeList(&mergedBlocks);
slFreeList(&blockList);
slReverse(&list);
return list;
}

int bigWigIntervalDump(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end, int maxCount,
	FILE *out)
/* Print out info on bigWig parts that intersect chrom:start-end.   Set maxCount to 0 if you 
 * don't care how many are printed.  Returns number printed. */
{
if (bwf->typeSig != bigWigSig)
   errAbort("Trying to do bigWigIntervalDump on a non big-wig file.");
bbiAttachUnzoomedCir(bwf);
struct fileOffsetSize *blockList = bbiOverlappingBlocks(bwf, bwf->unzoomedCir, 
	chrom, start, end, NULL);
struct fileOffsetSize *block;
struct udcFile *udc = bwf->udc;
int printCount = 0;

for (block = blockList; block != NULL; block = block->next)
    {
    udcSeek(udc, block->offset);
    int oneCount = bigWigBlockDumpIntersectingRange(bwf, chrom, start, end, maxCount, out);
    printCount += oneCount;
    if (maxCount != 0)
        {
	if (oneCount >= maxCount)
	    break;
	maxCount -= oneCount;
	}
    }
slFreeList(&blockList);
return printCount;
}

boolean bigWigSummaryArray(char *fileName, char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */
{
struct bbiFile *bwf = bigWigFileOpen(fileName);
boolean ret = bbiSummaryArray(bwf, chrom, start, end, bigWigIntervalQuery,
	summaryType, summarySize, summaryValues);
bbiFileClose(&bwf);
return ret;
}

boolean bigWigSummaryArrayExtended(char *fileName, char *chrom, bits32 start, bits32 end,
	int summarySize, struct bbiSummaryElement *summary)
/* Get extended summary information for summarySize evenely spaced elements into
 * the summary array. */
{
struct bbiFile *bbi = bigWigFileOpen(fileName);
boolean ret = bbiSummaryArrayExtended(bbi, chrom, start, end, bigWigIntervalQuery,
	summarySize, summary);
bbiFileClose(&bbi);
return ret;
}

double bigWigSingleSummary(char *fileName, char *chrom, int start, int end,
    enum bbiSummaryType summaryType, double defaultVal)
/* Return the summarized single value for a range. */
{
double arrayOfOne = defaultVal;
bigWigSummaryArray(fileName, chrom, start, end, summaryType, 1, &arrayOfOne);
return arrayOfOne;
}

