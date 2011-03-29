/* bwgValsOnChrom - implements the bigWigValsOnChrom access to bigWig. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "bits.h"
#include "sig.h"
#include "udc.h"
#include "zlibFace.h"
#include "bbiFile.h"
#include "bwgInternal.h"
#include "bigWig.h"

struct bigWigValsOnChrom *bigWigValsOnChromNew()
/* Allocate new empty bigWigValsOnChromStructure. */
{
return needMem(sizeof(struct bigWigValsOnChrom));
}

void bigWigValsOnChromFree(struct bigWigValsOnChrom **pChromVals)
/* Free up bigWigValsOnChrom */
{
struct bigWigValsOnChrom *chromVals = *pChromVals;
if (chromVals != NULL)
    {
    freeMem(chromVals->chrom);
    freeMem(chromVals->valBuf);
    freeMem(chromVals->covBuf);
    freez(pChromVals);
    }
}

static void fetchIntoBuf(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end,
	struct bigWigValsOnChrom *chromVals)
/* Get data for interval.  Return list allocated out of lm. */
{
/* A lot of code duplicated with bigWigIntervalQuery, but here the clipping
 * is simplified since always working across full chromosome, and the output is
 * different.  Since both of these are in inner loops and speed critical, it's hard
 * to factor out without perhaps making it worse than the bit of duplication. */
if (bwf->typeSig != bigWigSig)
   errAbort("Trying to do fetchIntoBuf on a non big-wig file.");
bbiAttachUnzoomedCir(bwf);
struct fileOffsetSize *blockList = bbiOverlappingBlocks(bwf, bwf->unzoomedCir, 
	chrom, start, end, NULL);
struct fileOffsetSize *block, *beforeGap, *afterGap;
struct udcFile *udc = bwf->udc;
boolean isSwapped = bwf->isSwapped;
float val;
int i;
Bits *covBuf = chromVals->covBuf;
double *valBuf = chromVals->valBuf;

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
		    bitSetRange(covBuf, s, e-s);
		    val = memReadFloat(&blockPt, isSwapped);
		    bits32 j;
		    for (j=s; j<e; ++j)
		        valBuf[j] = val;
		    }
		break;
		}
	    case bwgTypeVariableStep:
		{
		for (i=0; i<head.itemCount; ++i)
		    {
		    bits32 s = memReadBits32(&blockPt, isSwapped);
		    val = memReadFloat(&blockPt, isSwapped);
		    bitSetRange(covBuf, s, head.itemSpan);
		    bits32 e = s + head.itemSpan;
		    bits32 j;
		    for (j=s; j<e; ++j)
		        valBuf[j] = val;
		    }
		break;
		}
	    case bwgTypeFixedStep:
		{
		/* Do a little optimization for the most common and worst case - step1/span1 */
		if (head.itemStep == 1 && head.itemSpan == 1)
		    {
		    bits32 s = head.start;
		    bits32 e = head.end;
		    bitSetRange(covBuf, s, e-s);
		    bits32 j;
		    for (j=s; j<e; ++j)
		        valBuf[j] = memReadFloat(&blockPt, isSwapped);
		    }
		else
		    {
		    bits32 s = head.start;
		    bits32 e = s + head.itemSpan;
		    for (i=0; i<head.itemCount; ++i)
			{
			bitSetRange(covBuf, s, head.itemSpan);
			val = memReadFloat(&blockPt, isSwapped);
			bits32 j;
			for (j=s; j<e; ++j)
			    valBuf[j] = val;
			s += head.itemStep;
			e += head.itemStep;
			}
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
}


boolean bigWigValsOnChromFetchData(struct bigWigValsOnChrom *chromVals, char *chrom, 
	struct bbiFile *bigWig)
/* Fetch data for chromosome from bigWig. Returns FALSE if not data on that chrom. */
{
/* Fetch chromosome and size into self. */
freeMem(chromVals->chrom);
chromVals->chrom = cloneString(chrom);
long chromSize = chromVals->chromSize = bbiChromSize(bigWig, chrom);

if (chromSize <= 0)
    return FALSE;

/* Make sure buffers are big enough. */
if (chromSize > chromVals->bufSize)
    {
    freeMem(chromVals->valBuf);
    freeMem(chromVals->covBuf);
    chromVals->valBuf = needHugeMem((sizeof(double))*chromSize);
    chromVals->covBuf = bitAlloc(chromSize);
    chromVals->bufSize = chromSize;
    }

/* Zero out buffers */
bitClear(chromVals->covBuf, chromSize);
double *valBuf = chromVals->valBuf;
int i;
for (i=0; i<chromSize; ++i)
    valBuf[i] = 0.0;

fetchIntoBuf(bigWig, chrom, 0, chromSize, chromVals);

#ifdef OLD
/* Fetch intervals for this chromosome and fold into buffers. */
struct lm *lm = lmInit(0);
struct bbiInterval *iv, *ivList = bigWigIntervalQuery(bigWig, chrom, 0, chromSize, lm);
for (iv = ivList; iv != NULL; iv = iv->next)
    {
    double val = iv->val;
    int end = iv->end;
    for (i=iv->start; i<end; ++i)
	valBuf[i] = val;
    bitSetRange(chromVals->covBuf, iv->start, iv->end - iv->start);
    }
lmCleanup(&lm);
#endif /* OLD */
return TRUE;
}


