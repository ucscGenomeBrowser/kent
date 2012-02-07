/* Some relatively small utility functions that act on ffAlis.
 * (Separated from fuzzyFinder.c so people can do light ffAli 
 * work without including 100k of fuzzyFinder object code.) 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dnautil.h"
#include "fuzzyFind.h"


void ffFreeAli(struct ffAli **pAli)
/* Dispose of memory gotten from fuzzyFind(). */
{
struct ffAli *ali = *pAli;
if (ali != NULL)
    {
    while (ali->right)
        ali = ali->right;
    slFreeList(&ali);
    }
*pAli = NULL;
}

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

struct ffAli *ffAliFromSym(int symCount, char *nSym, char *hSym,
	struct lm *lm, char *nStart, char *hStart)
/* Convert symbol representation of alignments (letters plus '-')
 * to ffAli representation.  If lm is nonNULL, ffAli result 
 * will be lmAlloced, else it will be needMemed. This routine
 * depends on nSym/hSym being zero terminated. */
{
struct ffAli *ffList = NULL, *ff = NULL;
char n, h;
int i;

for (i=0; i<=symCount; ++i)
    {
    boolean isGap;
    n = nSym[i];
    h = hSym[i];
    isGap = (n == '-' || n == 0 || h == '-' || h == 0);
    if (isGap)
	{
	if (ff != NULL)
	    {
	    ff->nEnd = nStart;
	    ff->hEnd = hStart;
	    ff->left = ffList;
	    ffList = ff;
	    ff = NULL;
	    }
	}
    else
	{
	if (ff == NULL)
	    {
	    if (lm != NULL)
		{
		lmAllocVar(lm, ff);
		}
	    else
		{
		AllocVar(ff);
		}
	    ff->nStart = nStart;
	    ff->hStart = hStart;
	    }
	}
    if (n != '-')
	{
	++nStart;
	}
    if (h != '-')
	{
	++hStart;
	}
    }
ffList = ffMakeRightLinks(ffList);
return ffList;
}
