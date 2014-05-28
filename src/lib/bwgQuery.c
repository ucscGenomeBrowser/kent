/* bwgQuery - implements the query side of bigWig.  See bwgInternal.h for definition of file
 * format. */

/* Copyright (C) 2012 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sig.h"
#include "sqlNum.h"
#include "obscure.h"
#include "dystring.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "rangeTree.h"
#include "udc.h"
#include "zlibFace.h"
#include "bbiFile.h"
#include "bwgInternal.h"
#include "bigWig.h"
#include "bigBed.h"


struct bbiFile *bigWigFileOpen(char *fileName)
/* Open up big wig file. */
{
return bbiFileOpen(fileName, bigWigSig, "big wig");
}

boolean bigWigFileCheckSigs(char *fileName)
/* check file signatures at beginning and end of file */
{
return bbiFileCheckSigs(fileName, bigWigSig, "big wig");
}

#ifdef OLD
static void bwgSectionHeadRead(struct bbiFile *bwf, struct bwgSectionHead *head)
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
#endif /* OLD */

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

static int bigWigBlockDumpIntersectingRange(boolean isSwapped, char *blockPt, char *blockEnd, 
	char *chrom, bits32 rangeStart, bits32 rangeEnd, int maxCount, FILE *out)
/* Print out info on parts of block that intersect start-end, block starting at current position. */
{
struct bwgSectionHead head;
bwgSectionHeadFromMem(&blockPt, &head, isSwapped);
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
	    bits32 start = memReadBits32(&blockPt, isSwapped);
	    bits32 end = memReadBits32(&blockPt, isSwapped);
	    val = memReadFloat(&blockPt, isSwapped);
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
	    bits32 start = memReadBits32(&blockPt, isSwapped);
	    val = memReadFloat(&blockPt, isSwapped);
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
	    val = memReadFloat(&blockPt, isSwapped);
	    if (rangeIntersection(rangeStart, rangeEnd, start, start+head.itemSpan) > 0)
	        {
		if (!gotStart)
		    {
		    fprintf(out, "fixedStep chrom=%s start=%u step=%u span=%u\n", 
			    chrom, start+1, head.itemStep, head.itemSpan);
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
assert( (maxCount != 0 && outCount >= maxCount) || (blockPt == blockEnd));
return outCount;
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
struct fileOffsetSize *block, *beforeGap, *afterGap;
struct udcFile *udc = bwf->udc;
boolean isSwapped = bwf->isSwapped;
float val;
int i;

/* Set up for uncompression optionally. */
char *uncompressBuf = NULL;
if (bwf->uncompressBufSize > 0)
    uncompressBuf = needLargeMem(bwf->uncompressBufSize);

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
	    int uncSize = zUncompress(blockBuf, block->size, uncompressBuf, bwf->uncompressBufSize);
	    blockEnd = blockPt + uncSize;
	    }
	else
	    {
	    blockPt = blockBuf;
	    blockEnd = blockPt + block->size;
	    }

	/* Deal with insides of block. */
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
	assert(blockPt == blockEnd);
	blockBuf += block->size;
	}
    freeMem(mergedBuf);
    }
freeMem(uncompressBuf);
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
struct fileOffsetSize *block, *beforeGap, *afterGap;
struct udcFile *udc = bwf->udc;
int printCount = 0;

/* Set up for uncompression optionally. */
char *uncompressBuf = NULL;
if (bwf->uncompressBufSize > 0)
    uncompressBuf = needLargeMem(bwf->uncompressBufSize);

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
	    int uncSize = zUncompress(blockBuf, block->size, uncompressBuf, bwf->uncompressBufSize);
	    blockEnd = blockPt + uncSize;
	    }
	else
	    {
	    blockPt = blockBuf;
	    blockEnd = blockPt + block->size;
	    }

	/* Do the actual dump. */
	int oneCount = bigWigBlockDumpIntersectingRange(bwf->isSwapped, blockPt, blockEnd, 
		chrom, start, end, maxCount, out);

	/* Keep track of how many dumped, not exceeding maximum. */
	printCount += oneCount;
	if (maxCount != 0)
	    {
	    if (oneCount >= maxCount)
		{
		block = NULL;	 // we want to drop out of the outer loop too
		break;
		}

	    maxCount -= oneCount;
	    }
	blockBuf += block->size;
	}
    freeMem(mergedBuf);
    }
freeMem(uncompressBuf);

slFreeList(&blockList);
return printCount;
}

boolean bigWigSummaryArray(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues)
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */
{
boolean ret = bbiSummaryArray(bwf, chrom, start, end, bigWigIntervalQuery,
	summaryType, summarySize, summaryValues);
return ret;
}

boolean bigWigSummaryArrayExtended(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end,
	int summarySize, struct bbiSummaryElement *summary)
/* Get extended summary information for summarySize evenely spaced elements into
 * the summary array. */
{
boolean ret = bbiSummaryArrayExtended(bwf, chrom, start, end, bigWigIntervalQuery,
	summarySize, summary);
return ret;
}

double bigWigSingleSummary(struct bbiFile *bwf, char *chrom, int start, int end,
    enum bbiSummaryType summaryType, double defaultVal)
/* Return the summarized single value for a range. */
{
double arrayOfOne = defaultVal;
bigWigSummaryArray(bwf, chrom, start, end, summaryType, 1, &arrayOfOne);
return arrayOfOne;
}

boolean isBigWig(char *fileName)
/* Peak at a file to see if it's bigWig */
{
FILE *f = mustOpen(fileName, "rb");
bits32 sig;
mustReadOne(f, sig);
fclose(f);
if (sig == bigWigSig)
    return TRUE;
sig = byteSwap32(sig);
return sig == bigWigSig;
}

