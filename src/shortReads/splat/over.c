/* over Handle processing over-occurring 25-mers. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "dnautil.h"

struct el64
/*  An element in a list of 64-bit numbers */
    {
    struct el64 *next;
    bits64 val;
    };

bits64 basesToBits64(char *dna, int size)
/* Convert dna of given size (up to 32) to binary representation */
{
if (size > 32)
    errAbort("basesToBits64 called on %d bases, can only go up to 32", size);
bits64 result = 0;
int i;
for (i=0; i<size; ++i)
    {
    result <<= 2;
    result += ntValNoN[(int)dna[i]];
    }
return result;
}

void overRead(char *fileName, int minCount, int *retArraySize, bits64 **retArray)
/* Read in file of format <25-mer> <count> and return an array the 25-mers that have
 * counts greater or equal to minCount.  The 25-mers will be returned as 64-bit numbers
 * with the DNA packed 2 bits per base. */
{
/* Declare list and element, and a local memory pool to use for list. */
struct el64 *list = NULL, *el;
struct lm *lm = lmInit(0);

/* Open two column file and stream through it, adding items to list. */
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
int arraySize = 0;
while (lineFileRow(lf, row))
    {
    if (lineFileNeedNum(lf, row, 1) >= minCount)
        {
	lmAllocVar(lm, el);
	el->val = basesToBits64(row[0], 25);
	slAddHead(&list, el);
	arraySize += 1;
	}
    }
slReverse(&list);

/* Allocate and fill in array from list. */
bits64 *array;
AllocArray(array, arraySize);
int i;
for (i=0, el=list; i<arraySize; ++i, el = el->next)
    array[i] = el->val;

/* Clean up, save results and return. */
lmCleanup(&lm);
*retArraySize = arraySize;
*retArray = array;
}

