/* ffAliHelp - Helper routines for things that produce (rather than just
 * consume) ffAli type alignments. */
/* Copyright 2000-2003 Jim Kent.  All rights reserved. */

#include "common.h"
#include "fuzzyFind.h"
#include "dnaseq.h"


void ffCat(struct ffAli **pA, struct ffAli **pB)
/* Concatenate B to the end of A. Eat up second list
 * in process. */
{
struct ffAli *a = *pA;
struct ffAli *b = *pB;

/* If list to add is empty our job is real easy. */
if (b == NULL)
    return;

/* If list to add into is empty, then just switch in the
 * second list. */
if (a == NULL)
    {
    *pA = *pB;
    *pB = NULL;
    return;
    }

/* Neither list empty.  Find rightmost element of first list
 * and cross-link it with leftmost element of second list. */
while (a->right != NULL) a = a->right;
b->left = a;
a->right = b;
*pB = NULL;
}

void ffAliSort(struct ffAli **pList, int (*compare )(const void *elem1,  const void *elem2))
/* Sort a doubly linked list of ffAlis. */
{
/* Get head of list and handle easy special empty case. */
struct ffAli *r = *pList;
if (r == NULL) return;

/* Since first pointer is "left", in order to reuse slSort, have
 * to jump through some minor hoops. First go to right end of list,
 * then sort it. */
while (r->right) r = r->right;
slSort(&r, compare);

/* We're sorted, but our right links are all broken.  Fix this. */
slReverse(&r);
r = ffMakeRightLinks(r);
*pList = r;
}

int ffCmpHitsHayFirst(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * target offset followed by ascending query offset. */
{
const struct ffAli *a = *((struct ffAli **)va);
const struct ffAli *b = *((struct ffAli **)vb);
int diff;
if ((diff = a->hStart - b->hStart) != 0)
    return diff;
return a->nStart - b->nStart;
}

int ffCmpHitsNeedleFirst(const void *va, const void *vb)
/* Compare function to sort hit array by ascending
 * query offset followed by ascending target offset. */
{
const struct ffAli *a = *((struct ffAli **)va);
const struct ffAli *b = *((struct ffAli **)vb);
int diff;
if ((diff = a->nStart - b->nStart) != 0)
    return diff;
return a->hStart - b->hStart;
}

void ffExpandExactRight(struct ffAli *ali, DNA *needleEnd, DNA *hayEnd)
/* Expand aligned segment to right as far as can exactly. */
{
DNA *nEnd = ali->nEnd;
DNA *hEnd = ali->hEnd;
while (nEnd < needleEnd && hEnd < hayEnd)
    {
    if (*nEnd != *hEnd)
        break;
    nEnd += 1;
    hEnd += 1;
    }
ali->nEnd = nEnd;
ali->hEnd = hEnd;
return;
}

void ffExpandExactLeft(struct ffAli *ali, DNA *needleStart, DNA *hayStart)
/* Expand aligned segment to left as far as can exactly. */
{
DNA *nStart = ali->nStart-1;
DNA *hStart = ali->hStart-1;
while (nStart >= needleStart && hStart >= hayStart)
    {
    if (*nStart != *hStart)
        break;
    nStart -= 1;
    hStart -= 1;
    }
ali->nStart = nStart + 1;
ali->hStart = hStart + 1;
return;
}

struct ffAli *ffMergeClose(struct ffAli *aliList, 
	DNA *needleStart, DNA *hayStart)
/* Remove overlapping areas needle in alignment. Assumes ali is sorted on
 * ascending nStart field. Also merge perfectly abutting neighbors or
 * ones that could be merged at the expense of just a few mismatches.*/
{
struct ffAli *mid, *ali;
int closeEnough = -3;

if (aliList == NULL)
    return NULL;
for (mid = aliList->right; mid != NULL; mid = mid->right)
    {
    for (ali = aliList; ali != mid; ali = ali->right)
	{
	char *nStart, *nEnd;
	int nOverlap;
	nStart = max(ali->nStart, mid->nStart);
	nEnd = min(ali->nEnd, mid->nStart);
	nOverlap = nEnd - nStart;
	/* Overlap or perfectly abut in needle, and needle/hay
	 * offset the same. */
	if (nOverlap >= closeEnough)
	    {
	    int aliDiag = (ali->nStart - needleStart) - (ali->hStart - hayStart);
	    int midDiag = (mid->nStart - needleStart) - (mid->hStart - hayStart);
	    if (aliDiag == midDiag)
		{
		/* Make mid encompass both, and make ali empty. */
		mid->nStart = min(ali->nStart, mid->nStart);
		mid->nEnd = max(ali->nEnd, mid->nEnd);
		mid->hStart = min(ali->hStart, mid->hStart);
		mid->hEnd = max(ali->hEnd, mid->hEnd);
		ali->hStart = ali->hEnd = mid->hStart;
		ali->nEnd = ali->nStart = mid->nStart;;
		}
	    }
	}
    }
aliList = ffRemoveEmptyAlis(aliList, TRUE);
return aliList;
}


int ffScoreIntron(DNA a, DNA b, DNA y, DNA z, int orientation)
/* Return a better score the closer an intron is to
 * consensus. */
{
int score = 0;
int revScore = 0;

if (orientation >= 0)
    {
    if (a == 'g' || a == 'G') ++score;
    if (b == 't' || b == 'T') ++score;
    if (y == 'a' || y == 'A') ++score;
    if (z == 'g' || z == 'G') ++score;
    }

if (orientation <= 0)
    {
    if (a == 'c' || a == 'C') ++revScore;
    if (b == 't' || b == 'T') ++revScore;
    if (y == 'a' || y == 'A') ++revScore;
    if (z == 'c' || z == 'C') ++revScore;
    }

return score > revScore ? score : revScore;
}


static int slideIntron(struct ffAli *left, struct ffAli *right, int orientation)
/* Slides space between alignments if possible to match
 * intron consensus better.  Returns how much it slid intron. */
{
DNA *nLeft = left->nEnd;
DNA *hLeft = left->hEnd;
DNA *nRight = right->nStart;
DNA *hRight = right->hStart;
DNA nl, nr, hl, hr;
DNA *nLeftEnd = left->nStart;
DNA *nRightEnd = right->nEnd;
DNA *nBestLeft = NULL;
int bestScore = -0x7fffffff;
int curScore;
int offset;

if (hRight-hLeft < 4)   /* Too short to be an intron. */
    return 0;
if (nRight-nLeft > 2)   /* Too big of a gap to be an intron. */
    return 0;

/* Slide as far to the left as possible without inserting mismatches. */
while (nLeft > nLeftEnd)
    {
    nl = nLeft[-1];
    hl = hLeft[-1];
    nr = nRight[-1];
    hr = hRight[-1];
    if (!(nl == 'n' && nr == 'n'))  /* N's in needle freely slide. */
        {
        if (nr != hr)
            break;
        }
    nLeft -= 1;
    hLeft -= 1;
    nRight -= 1;
    hRight -= 1;
    }
/* Slide as far to the right as possible computing
   intron score as you go. */
while (nRight < nRightEnd)
    {
    curScore = ffScoreIntron(hLeft[0], hLeft[1], hRight[-2], hRight[-1], orientation);
    if (curScore > bestScore)
        {
        bestScore = curScore;
        nBestLeft = nLeft;
        }
    nl = nLeft[0];
    hl = hLeft[0];
    if (nl != 'n' && nl != hl)
        break;
    nr = nRight[0];
    hr = hRight[0];
    nLeft += 1;
    hLeft += 1;
    nRight += 1;
    hRight += 1;
    }
if (nBestLeft == NULL)
    return 0;
offset = nBestLeft - left->nEnd;
if (offset == 0)
    return offset;
left->nEnd += offset;
left->hEnd += offset;
right->nStart += offset;
right->hStart += offset;
return offset;
}


boolean ffSlideOrientedIntrons(struct ffAli *ali, int orient)
/* Slide introns (or spaces between aligned blocks)
 * to match consensus on given strand. */
{
struct ffAli *left = ali, *right;
boolean slid = FALSE;
if (left == NULL)
    return FALSE;
while((right = left->right) != NULL)
    {
    if (slideIntron(left, right, orient))
        slid = TRUE;
    left = right;
    }
return slid;
}

boolean ffSlideIntrons(struct ffAli *ali)
/* Slide introns (or spaces between aligned blocks)
 * to match consensus.  Return TRUE if any slid. */
{
int orient = ffIntronOrientation(ali);
return ffSlideOrientedIntrons(ali, orient);
}

struct ffAli *ffRemoveEmptyAlis(struct ffAli *ali, boolean doFree)
/* Remove empty blocks from list. Optionally free empties too. */
{
struct ffAli *leftAli;
struct ffAli *startAli;
struct ffAli *rightAli;

if (ali == NULL)
    return NULL;

/* Figure out left most non-empty ali. */
while (ali->left)
    ali = ali->left;
while (ali)
    {
    /* If current ali is empty, chuck it out. */
    if (ali->nEnd <= ali->nStart || ali->hEnd <= ali->hStart)
	{
	struct ffAli *empty = ali;
        ali = ali->right;
	if (doFree) freeMem(empty);
	}
    else
        break;
    }

if (ali == NULL)
    return NULL;

ali->left = NULL;

/* Get rid of empty middle alis. */
startAli = leftAli = ali;
ali = ali->right;
while (ali)
    {
    rightAli = ali->right;
    if (ali->nEnd <= ali->nStart || ali->hEnd <= ali->hStart)
        {
        leftAli->right = rightAli;
        if (rightAli != NULL)
            rightAli->left = leftAli;
	if (doFree) freeMem(ali);
        }
    else
        {
        leftAli = ali;
        }
    ali = rightAli;
    }
return startAli;
}

struct ffAli *ffMergeHayOverlaps(struct ffAli *ali)
/* Remove overlaps in haystack that perfectly abut in needle.
 * These are transformed into perfectly abutting haystacks
 * that have a gap in the needle. */
{
struct ffAli *a = NULL;
struct ffAli *leftA = NULL;

if (ali == NULL)
    return NULL;
a = ali;
for (;;)
    {
    int nOverlap;
    int hOverlap;
    int aSize;

    /* Advance to next ali */
    leftA = a;
    a = a->right;
    if (a == NULL)
        break;

    nOverlap = leftA->nEnd - a->nStart;
    hOverlap = leftA->hEnd - a->hStart;
    aSize = a->nEnd - a->nStart;
    if (hOverlap > 0 && hOverlap < aSize && nOverlap <= 0)
        {
        a->hStart += hOverlap;
        a->nStart += hOverlap;
        }
    }
return ali;
}

struct ffAli *ffMergeNeedleAlis(struct ffAli *ali, boolean doFree)
/* Remove overlapping areas needle in alignment. Assumes ali is sorted on
 * ascending nStart field. Also merge perfectly abutting neighbors.*/
{
struct ffAli *a = NULL;
struct ffAli *leftA = NULL;
struct ffAli *rightA;

if (ali == NULL)
    return NULL;
rightA = ali;
for (;;)
    {
    /* Advance to next ali */
    leftA = a;
    a = rightA;
    if (a == NULL)
        break;
    rightA = a->right;
    

    /* See if can merge current alignment into left one. */
    if (leftA != NULL)
        {
        int overlap = leftA->nEnd - a->nStart;

        /* Deal with overlaps in needle */
        if (overlap > 0)
            {
            /* See if left encompasses current segment. */
            if (leftA->nStart <= a->nStart && leftA->nEnd >= a->nEnd)
                {
                /* Eliminate current segment. */
                leftA->right = rightA;
                if (rightA != NULL)
                    rightA->left = leftA;
		if (doFree) freeMem(a);
                a = leftA;
                }
            else
                {
                /* Remove overlapping area from current segment, leave
                 * it in left segment. */
                a->hStart += overlap;
                a->nStart += overlap;
                }
            }
        else if (overlap == 0 && leftA->hEnd == a->hStart)
            {
            /* Remove current segment from list. */
            leftA->right = rightA;
            if (rightA != NULL)
                rightA->left = leftA;
            /* Fold data from current segment into left segment */
            leftA->nEnd = a->nEnd;
            leftA->hEnd = a->hEnd;
	    if (doFree) freeMem(a); 
	    a = leftA;
            }
        }
    }
return ali;
}

