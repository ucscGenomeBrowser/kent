/* over Handle processing over-occurring 25-mers. */
/* Copyright 2008 Jim Kent all rights reserved. */

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

int overCmp(const void *va, const void *vb)
/* Compare two bit64s. */
{
bits64 a = *((bits64*)va), b = *((bits64*)vb);
if (a < b) return -1;
else if (a > b) return 1;
else return 0;
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
	if (list != NULL && el->val < list->val)
	    errAbort("%s is unsorted on line %d", lf->fileName, lf->lineIx);
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


boolean overCheck(bits64 query, int arraySize, bits64 *array)
/* Return TRUE if query is in array.  */
{
if (arraySize == 0)
    return FALSE;
return bsearch(&query, array, arraySize, sizeof(query), overCmp) != NULL;
}

