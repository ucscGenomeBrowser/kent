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
#include "bbiFile.h"
#include "bwgInternal.h"
#include "bigWig.h"
#include "bigBed.h"

static char const rcsid[] = "$Id: bwgQuery.c,v 1.13 2009/02/02 06:02:00 kent Exp $";

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
void bigWigBlockDump(struct bbiFile *bwf, char *chrom, FILE *out)
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

struct bbiInterval *bigWigIntervalQuery(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end,
	struct lm *lm)
/* Get data for interval.  Return list allocated out of lm. */
{
if (bwf->typeSig != bigWigSig)
   errAbort("Trying to do bigWigIntervalQuery on a non big-wig file.");
bbiAttachUnzoomedCir(bwf);
struct bbiInterval *el, *list = NULL;
struct fileOffsetSize *blockList = bbiOverlappingBlocks(bwf, bwf->unzoomedCir, 
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

double bigWigSingleSummary(char *fileName, char *chrom, int start, int end,
    enum bbiSummaryType summaryType, double defaultVal)
/* Return the summarized single value for a range. */
{
double arrayOfOne = defaultVal;
bigWigSummaryArray(fileName, chrom, start, end, summaryType, 1, &arrayOfOne);
return arrayOfOne;
}

