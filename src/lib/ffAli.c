/* Some relatively small utility functions that act on ffAlis.
 * (Separated from fuzzyFinder.c so people can do light ffAli 
 * work without including 100k of fuzzyFinder object code.) 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

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

int ffIntronOrientation(struct ffAli *ali)
/* Return + for positive orientation overall, - for negative,
 * 0 if can't tell. */
{
struct ffAli *left = ali, *right;
int orient = 0;

if (left != NULL)
    {
    while((right = left->right) != NULL)
	{
	orient += intronOrientation(left->hEnd, right->hStart);
	left = right;
	}
    }
return orient;
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

