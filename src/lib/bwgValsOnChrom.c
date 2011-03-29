/* bwgValsOnChrom - implements the bigWigValsOnChrom access to bigWig. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
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
return TRUE;
}


