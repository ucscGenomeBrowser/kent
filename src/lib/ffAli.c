/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* Some relatively small utility functions that act on ffAlis.
 * (Separated from fuzzyFinder.c so people can do light ffAli 
 * work without including 100k of fuzzyFinder object code.) */

#include "common.h"
#include "dnautil.h"
#include "fuzzyFind.h"

int ffOneIntronOrientation(struct ffAli *left, struct ffAli *right)
/* Return 1 for GT/AG intron between left and right, -1 for CT/AC, 0 for no
 * intron. */
{
if (left->nEnd != right->nStart)
    return 0;
return intronOrientation(left->hEnd, right->hStart);
}

int ffIntronOrientation(struct ffAli *ff)
/* Return 1 if introns make it look like alignment is on + strand,
 *       -1 if introns make it look like alignment is on - strand,
 *        0 if can't tell. */
{
int intronDir = 0;
int oneDir;
struct ffAli *right;

for (right = ff->right; right != NULL; ff = right, right = right->right)
    {
    if ((oneDir = ffOneIntronOrientation(ff, right)) != 0)
	{
	if (intronDir && intronDir != oneDir)
	    return 0;
	intronDir = oneDir;
	}
    }
return intronDir;
}

struct ffAli *ffRightmost(struct ffAli *ff)
/* Return rightmost block of alignment. */
{
while (ff->right != NULL)
    ff = ff->right;
return ff;
}

struct ffAli *ffMakeRightLinks(struct ffAli *rightMost)
/* Given a pointer to the rightmost block in an alignment
 * which has all of the left pointers filled in, fill in
 * the right pointers and return the leftmost block. */
{
struct ffAli *ff, *last = NULL;

for (ff = rightMost; ff != NULL; ff = ff->left)
    {
    ff->right = last;
    last = ff;
    }
return last;
}

static int countGoodStart(struct ffAli *ali)
/* Return number of perfect matchers at start. */
{
DNA *n = ali->nStart;
DNA *h = ali->hStart;
int count = ali->nEnd - ali->nStart;
int i;
for (i=0; i<count; ++i)
    {
    if (*n++ != *h++)
        break;
    }
return i;
}

static int countGoodEnd(struct ffAli *ali)
/* Return number of perfect matchers at start. */
{
DNA *n = ali->nEnd;
DNA *h = ali->hEnd;
int count = ali->nEnd - ali->nStart;
int i;
for (i=0; i<count; ++i)
    {
    if (*--n != *--h)
        break;
    }
return i;
}

void ffCountGoodEnds(struct ffAli *aliList)
/* Fill in the goodEnd and badEnd scores. */
{
struct ffAli *ali;
for (ali = aliList; ali != NULL; ali = ali->right)
    {
    ali->startGood = countGoodStart(ali);
    ali->endGood = countGoodEnd(ali);
    }
}

int ffAliCount(struct ffAli *d)
/* How many blocks in alignment? */
{
int acc = 0;
while (d != NULL)
    {
    ++acc;
    d = d->right;
    }
return acc;
}

boolean ffSolidMatch(struct ffAli **pLeft, struct ffAli **pRight, DNA *needle, 
    int minMatchSize, int *retStartN, int *retEndN)
/* Return start and end (in needle coordinates) of solid parts of 
 * match if any. Necessary because fuzzyFinder algorithm will extend
 * ends a little bit beyond where they're really solid.  We want
 * to effectively save these bases for aligning somewhere else. */
{
struct ffAli *next;
int segSize;
int runTotal = 0;
int gapSize;
struct ffAli *left = *pLeft, *right = *pRight;

/* Get rid of small segments on left end that are separated from main body. */
for (;;)
    {
    if (left == NULL)
        return FALSE;
    next = left->right;
    segSize = left->nEnd - left->nStart;
    runTotal += segSize;
    if (segSize > minMatchSize || runTotal > minMatchSize*2)
        break;
    if (next != NULL)
        {
        gapSize = next->nStart - left->nEnd;
        if (gapSize > 1)
            runTotal = 0;
        }
    left = next;
    }
*retStartN = left->nStart - needle;

/* Do same thing on right end... */
runTotal = 0;
for (;;)
    {
    if (right == NULL)
        return FALSE;
    next = right->left;
    segSize = right->nEnd - right->nStart;
    runTotal += segSize;
    if (segSize > minMatchSize || runTotal > minMatchSize*2)
        break;
    if (next != NULL)
        {
        gapSize = next->nStart - left->nEnd;
        if (gapSize > 1)
            runTotal = 0;
        }
    right = next;
    }
*retEndN = right->nEnd - needle;

*pLeft = left;
*pRight = right;
return *retEndN - *retStartN >= minMatchSize;
}

struct ffAli *ffTrimFlakyEnds(struct ffAli *ali, int minMatchSize, 
	boolean freeFlakes)
/* Trim off ends of ffAli that aren't as solid as you'd like.  
 * If freeFlakes is true memory for flakes is freeMem'd. */
{
struct ffAli *left = ali;
struct ffAli *right = ffRightmost(ali);
int startN, endN;
if (ffSolidMatch(&left, &right, left->nStart, minMatchSize, &startN, &endN))
    {
    if (freeFlakes)
	{
	struct ffAli *ali, *next;
	ffFreeAli(&right->right);
	for (ali = left->left; ali != NULL; ali = next)
	    {
	    next = ali->left;
	    freeMem(ali);
	    }
	}
    right->right = NULL;
    left->left = NULL;
    return left;
    }
else
    {
    if (freeFlakes)
	ffFreeAli(&ali);
    return NULL;
    }
}

