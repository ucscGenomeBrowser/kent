/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* fuzzyFind - searches a large DNA sequence (the haystack) for 
 * a smaller DNA sequence (the needle).  What makes this tricky
 * is that the match need not be exact.
 *
 * The algorithm looks for exact matches to (typically ten) evenly 
 * spaced subsequences of the needle long enough for the match to
 * occur only once or less by chance in the haystack.  If
 * in fact they occur more than 3 times, the subsequence is
 * extended by a base.
 *
 * Next these ten-mers (called "tiles" in the code)
 * are extended as far as possible exactly.  Each tile at
 * this point may match the haystack in multiple places.
 *
 * Next the "weave" routines try to find a good 
 * way to link the tiles together.  Good is scored
 * by a routine which awards a point for a matching
 * base, and subtracts the log-base-two of gaps.  It
 * penalizes an additionallly if a tile is
 * placed before instead of after a previous tile.
 *
 * Now that a preliminary alignment exists, the algorithm
 * searches for exact matches between tiles. These matches
 * are constrained to be long enough that they only occur
 * once in the space between tiles.  These matches are 
 * extended as far as they can be exactly, and added as 
 * further tiles to the alignment list.
 *
 * Finally the tiles are extended inexactly until they
 * abutt each other, or until even inexact extension is
 * fruitless.  
 * 
 * The inexact extension is perhaps the trickiest part of the
 * algorithm.  It's implemented in expandLeft and expandRight.
 * In these the extension is first taken as far as it can go
 * exactly.  Then if four of the next five bases match it's
 * taken five bases further. Then a break is generated, and the
 * next significant match between the needle and haystack is
 * scanned for.
 */

#include "common.h"
#include "dnautil.h"
#include "localmem.h"
#include "errabort.h"
#include "fuzzyFind.h"
#include "obscure.h"

/* ffMemory routines - 
 *    FuzzyFinder allocates memory internally from the
 *    following routines, which allocate blocks from the
 *    main allocator and then dole them out.
 *
 *    At the end of fuzzy-finding, the actual alignment
 *    is copied to more permanent memory, and all the
 *    blocks freed.  This saves from having to remember
 *    to free every little thing, and also is faster.
 *
 *    If memory can't be allocated this will throw
 *    back to the root of the fuzzy-finder system.
 *
 *    (In typical usage this only allocates about
 *    as much memory as the size of the DNA
 *    you're scanning though.)
 *    
 */

static jmp_buf ffRecover;

static void ffAbort()
/* Abort fuzzy finding. */
{
longjmp(ffRecover, -1);
}

static struct lm *ffMemPool = NULL;

static void ffMemInit()
/* Initialize fuzzyFinder local memory system. */
{
ffMemPool = lmInit(2048);
}

static void ffMemCleanup()
/* Free up fuzzyFinder local memory system. */
{
lmCleanup(&ffMemPool);
}

static void *ffNeedMem(size_t size)
/* Allocate from fuzzyFinder local memory system. */
{
return lmAlloc(ffMemPool, size);
}

static void makeFreqTable(DNA *dna, int dnaSize, double freq[4])
{
int histo[4];
double total;
int i;

dnaBaseHistogram(dna, dnaSize, histo);
total = histo[0] + histo[1] + histo[2] + histo[3];
for (i=0; i<4; ++i)
    freq[i] = (double)histo[i] / total;
}

static double oligoProb(DNA *oligo, int size, double freq[4])
{
double prob = 1.0;
int i;
int baseVal;

for (i=0; i<size; ++i)
    {
    if ((baseVal = ntVal[oligo[i]]) >= 0)
        prob *= freq[baseVal];
    }
return prob;
}

static boolean findImprobableOligo(DNA *needle, int needleLength, double maxProb, double freq[4],
    DNA **rOligo, int *rOligoLength, double *rOligoProb)
/* Find an oligo that's got less than maxProb probability of matching to
 * random DNA with the given base frequency distribution. */
{
int i;
double totalProb = 1.0;
int base;
int startIx = 0;

for (i=0;i<needleLength;++i)
    {
    if ((base = ntVal[needle[i]]) < 0)
        {
        totalProb = 1.0;
        startIx = i+1;
        }
    else
        {
        if ((totalProb *= freq[base]) <= maxProb)
            {
            *rOligo = needle+startIx;
            *rOligoLength = i-startIx+1;
            *rOligoProb = totalProb;
            return TRUE;
            }
        }
    }
return FALSE;
}


static boolean hasRepeat(DNA *oligo, int oligoLen)
/* Returns TRUE if oligo has an internal repeat mod 1, 2, or 3. */
{
int mod;
int i;
DNA b;
boolean gotRepeat;
int repSize;
int maxRep = (oligoLen+1)/2;

b = oligo[0];
gotRepeat = TRUE;
for (i=1; i<oligoLen; ++i)
    {
    if (oligo[i] != b)
        {
        gotRepeat = FALSE;
        break;
        }
    }
if (gotRepeat)
    return TRUE;

gotRepeat = TRUE;
for (i=2; i<oligoLen; ++i)
    {
    if (oligo[i&1] != oligo[i])
        {
        gotRepeat = FALSE;
        break;
        }
    }
if (gotRepeat)
    return TRUE;

for (repSize = 3; repSize <= maxRep; ++repSize)
    {
    mod = 0;
    gotRepeat = TRUE;
    for (i=repSize; i<oligoLen; ++i)
        {
        if (oligo[mod] != oligo[i])
            {
            gotRepeat = FALSE;
            break;
            }
        if (++mod == repSize)
            mod = 0;
        }
    if (gotRepeat)
        return TRUE;
    }
return gotRepeat;
}



boolean ffFindGoodOligo(DNA *needle, int needleLength, double maxProb, double freq[4],
    DNA **rOligo, int *rOligoLength, double *rOligoProb)
/* Find an oligo that's suitably improbable and doesn't contain 
 * short internal repeats. */
{
int oligoLen;
DNA *oligo;

/* Loop around until you get one that doesn't repeat. */
for (;;)
    {
    if (!findImprobableOligo(needle, needleLength, maxProb, freq, rOligo, rOligoLength, rOligoProb))
        return FALSE;
    oligoLen = *rOligoLength;
    oligo = *rOligo;
    if (hasRepeat(oligo, oligoLen))
        {
        DNA *newNeedle = oligo+oligoLen;
        int newSize = needleLength - (newNeedle-needle);
        if (newSize <= 0)
            return FALSE;
        needle = newNeedle;
        needleLength = newSize;
        }
    else
        return TRUE;
    }
}

int ffScoreMatch(DNA *a, DNA *b, int size)
/* Compare two pieces of DNA base by base. Total mismatches are
 * subtracted from total matches and returned as score. 'N's 
 * neither hurt nor help score. */
{
int i;
int score = 0;
for (i=0; i<size; ++i)
    {
    DNA aa = a[i];
    DNA bb = b[i];
    if (aa == 'n' || bb == 'n')
        continue;
    if (aa == bb)
        ++score;
    else
        score -= 1;
    }
return score;
}
#ifdef OLD   /* This got renamed to dnaScoreMatch and moved to dnaUtils. */
#endif /* OLD */

int ffCalcCdnaGapPenalty(int hGap, int nGap)
/* Return gap penalty for given h and n gaps. */
{
int acc = 2;
if (hGap > 100000)	/* Discourage really long introns. */
    {
    acc += (hGap - 100000)/3000;
    if (hGap > 500000)
        acc += (hGap - 500000)/2000;
    }
if (hGap < 0)   /* Discourage jumping back in haystack. */
    {
    hGap = -8*hGap;
    if (hGap > 48)
        hGap = (hGap*hGap);
    }
if (nGap < 0)   /* Jumping back in needle gets rid of previous alignment. */
    {
    acc += -nGap;
    nGap = 0;
    }
return acc + digitsBaseTwo(hGap+nGap);
}

static int calcTightGap(int hGap, int nGap)
/* Figure out gap penalty using tight model (gaps bad!) */
{
if (hGap == 0 && nGap == 0)
    return 0;
else
    {
    int overlap = min(hGap, nGap);
    int penalty = 8;
    if (overlap < 0)
	overlap = 0;

    if (hGap < 0)
	hGap = -8*hGap;
    if (nGap < 0)
	nGap = -2*nGap;
    penalty += (hGap-overlap + nGap-overlap) + overlap;
    return penalty;
    }
}

static int calcLooseGap(int hGap, int nGap)
/* Figure out gap penalty using loose model (gaps not so bad) */
{
if (hGap == 0 && nGap == 0)
    return 0;
else
    {
    int overlap = min(hGap, nGap);
    int penalty = 8;
    if (overlap < 0)
	overlap = 0;

    if (hGap < 0)
	hGap = -8*hGap;
    if (nGap < 0)
	nGap = -2*nGap;
    penalty += log(hGap-overlap+1) + log(nGap-overlap+1);
    return penalty;
    }
}


int ffCalcGapPenalty(int hGap, int nGap, enum ffStringency stringency)
/* Return gap penalty for given h and n gaps. */
{
switch (stringency)
    {
    case ffCdna:
	return ffCalcCdnaGapPenalty(hGap, nGap);
    case ffTight:
	return calcTightGap(hGap,nGap);
    case ffLoose:
	return calcLooseGap(hGap,nGap);
    default:
        errAbort("Unknown stringency type %d", stringency);
	return 0;
    }
}


int ffCdnaGapPenalty(struct ffAli *left, struct ffAli *right)
/* What is penalty for gap between two. */
{
int hGap = right->hStart - left->hEnd;
int nGap = right->nStart - left->nEnd;
return ffCalcCdnaGapPenalty(hGap, nGap);
}

int ffGapPenalty(struct ffAli *left, struct ffAli *right, enum ffStringency stringency)
/* What is penalty for gap between two given stringency? */
{
int hGap = right->hStart - left->hEnd;
int nGap = right->nStart - left->nEnd;
return ffCalcGapPenalty(hGap, nGap, stringency);
}

int ffScoreSomeAlis(struct ffAli *ali, int count, enum ffStringency stringency)
/* Figure out score of count alis. */
{
int score = 0;
int oneScore;

while (--count >= 0)
    {
    int len = ali->hEnd - ali->hStart;
    struct ffAli *right = ali->right;
    oneScore = dnaScoreMatch(ali->hStart, ali->nStart, len);
    score += oneScore;
    if (count > 0)  /* Calculate gap penalty */
        score -= ffGapPenalty(ali, right,stringency);
    ali = right;
    }
return score;
}

int ffScore(struct ffAli *ali, enum ffStringency stringency)
/* Score alignment. */
{
int score = 0;
int oneScore;

if (ali == NULL)
    return -0x7FFFFFFF;
while (ali->left != NULL) ali = ali->left;
while (ali != NULL)
    {
    int len = ali->hEnd - ali->hStart;
    struct ffAli *right = ali->right;
    oneScore = dnaScoreMatch(ali->hStart, ali->nStart, len);
    score += oneScore;
    if (right)  /* Calculate gap penalty */
        {
        score -= ffGapPenalty(ali, right, stringency);
        }
    ali = right;
    }
return score;
}

int ffScoreCdna(struct ffAli *ali)
/* Figure out overall score of this alignment. 
 * Perfect match is number of bases in needle. */
{
return ffScore(ali, ffCdna);
}


static boolean leftNextMatch(struct ffAli *ali, DNA *ns, DNA *ne, DNA *hs, DNA *he, 
    int gapPenalty, int maxSkip)
/* Scan to the left for something that matches the next bit of the needle
 * in the haystack. */
{
int haySize = he - hs;
int needleSize = ne - ns;
int diagSize = haySize + needleSize;
int matchSize;
int i;

/* We take care of bigger skips on the "tile" level. */
if (diagSize > maxSkip)
    diagSize = maxSkip;
/* Scan diagonally... */
/*
0 1 2 3
1 2 3 4
2 3 4 5
3 4 5 6
*/
for (i=1; i<=diagSize; ++i)
    {
    int hOff = i;
    int nOff = 0;
    int hDiff = hOff - haySize;
    matchSize = gapPenalty + digitsBaseTwo(i);
    if (hDiff > 0)
        {
        nOff += hDiff;
        hOff -= hDiff;
        }
    for (;hOff >= 0; --hOff, ++nOff)
        {
        int needleLeft = needleSize - nOff;
        int hayLeft = haySize - hOff;
        if (matchSize > needleLeft) break;
        if (matchSize > hayLeft) continue;
        if (ne[-nOff-1] == he[-hOff-1] && memcmp(ne-nOff-matchSize, he-hOff-matchSize, matchSize) == 0)
            {
            ali->nStart = ne - nOff - matchSize;
            ali->nEnd = ne - nOff;
            ali->hStart = he - hOff - matchSize;
            ali->hEnd = he- hOff;
            return TRUE;
            }
        }
    }
return FALSE;
}


static boolean rightNextMatch(struct ffAli *ali, DNA *ns, DNA *ne, DNA *hs, DNA *he, 
    int gapPenalty, int maxSkip)
/* Scan to the right for something that matches the next bit of the needle
 * in the haystack. */
{
int haySize = he - hs;
int needleSize = ne - ns;
int diagSize = haySize + needleSize;
int matchSize;
int i;

/* We take care of bigger skips on the "tile" level. */
if (diagSize > maxSkip)
    diagSize = maxSkip;
/* Scan diagonally... */
/*
0 1 2 3
1 2 3 4
2 3 4 5
3 4 5 6
*/
for (i=1; i<=diagSize; ++i)
    {
    int hOff = i;
    int nOff = 0;
    int hDiff = hOff - haySize;
    matchSize = gapPenalty + digitsBaseTwo(i);
    if (hDiff > 0)
        {
        nOff += hDiff;
        hOff -= hDiff;
        }
    for (;hOff >= 0; --hOff, ++nOff)
        {
        int needleLeft = needleSize - nOff;
        int hayLeft = haySize - hOff;
        if (matchSize > needleLeft) break;
        if (matchSize > hayLeft) continue;
        if (ns[nOff] == hs[hOff] && memcmp(ns+nOff, hs+hOff, matchSize) == 0)
            {
            ali->nStart = ns + nOff;
            ali->nEnd = ns + nOff + matchSize;
            ali->hStart = hs + hOff;
            ali->hEnd = hs + hOff + matchSize;
            return TRUE;
            }
        }
    }
return FALSE;
}

static boolean expandRight(struct ffAli *ali, DNA *needleStart, DNA *needleEnd,
    DNA *hayStart, DNA *hayEnd, int numSkips, int gapPenalty, int maxSkip);


static boolean expandLeft(struct ffAli *ali, DNA *needleStart, DNA *needleEnd,
    DNA *hayStart, DNA *hayEnd, int numSkips, int gapPenalty, int maxSkip)
/* Given a matching segment, try to expand the aligned parts to the
 * right. */
{
DNA *ns = ali->nStart;
DNA *hs = ali->hStart;
int score;
DNA *oldNs = ns;

for (;;)
    {
    while (ns > needleStart && hs > hayStart && ns[-1] == hs[-1])
        {
        --hs;
        --ns;
        }
    if (ns <= needleStart || hs <= hayStart)
        {
        ali->nStart = ns;
        ali->hStart = hs;
        return ns != oldNs;
        }
    /* Find fuzzy score to the left. */
        {
        int windowSize = 5;
        int nSize = ns-needleStart;
        int hSize = hs-hayStart;
        if (windowSize > nSize) windowSize = nSize;
        if (windowSize > hSize) windowSize = hSize;
        if (windowSize > 0)
            score = dnaScoreMatch(ns-windowSize, hs-windowSize, windowSize); 
        else
            score = -1;

        if (score >= windowSize-2)
            {
            hs -= windowSize;
            ns -= windowSize;
            }
        else if (--numSkips >= 0)
            {
            struct ffAli *newAli = ffNeedMem(sizeof(*newAli));
            ali->nStart = ns;
            ali->hStart = hs;
            if (ns - needleStart < 3 || 
                !leftNextMatch(newAli, needleStart, ns, hayStart, hs, gapPenalty, maxSkip))
                {
                return ns != oldNs; /* We did our best... */
                }
            newAli->right = ali;
            newAli->left = ali->left;
            if (ali->left)
                ali->left->right = newAli;
            ali->left = newAli;
            ali = newAli;
            expandRight(ali, needleStart, ns, hayStart, hs, 0, gapPenalty, maxSkip);
            ns = ali->nStart;
            hs = ali->hStart;
            }
        else
            {
            ali->nStart = ns;
            ali->hStart = hs;
            return ns != oldNs;
            }
        }
    }
}

static boolean expandRight(struct ffAli *ali, DNA *needleStart, DNA *needleEnd,
    DNA *hayStart, DNA *hayEnd, int numSkips, int gapPenalty, int maxSkip)
/* Given a matched segment, try to expand it to the right. */
{
int score;
DNA *ne = ali->nEnd;
DNA *he = ali->hEnd;
DNA *oldNe = ne;

for (;;)
    {
    /* Expand as far to the right as you can keeping perfect match */
    while (ne < needleEnd && he < hayEnd && *ne == *he)
        {
        ++he;
        ++ne;
        }

    /* Check to see if have come to the end. */
    if (ne >= needleEnd || he >= hayEnd)
        {
        ali->nEnd = ne;
        ali->hEnd = he;
        return oldNe != ne;
        }

    /* Find fuzzy score to the right. */
        {
        int windowSize = 5;
        int nSize = needleEnd-ne;
        int hSize = hayEnd - he;
        if (windowSize > nSize) windowSize = nSize;
        if (windowSize > hSize) windowSize = hSize;
        if (windowSize > 0)
            score = dnaScoreMatch(ne, he, windowSize); 
        else
            score = -1;

        if (score >= windowSize-2)
            {
            he += windowSize;
            ne += windowSize;
            }
        else if (--numSkips >= 0)
            {
            struct ffAli *newAli = ffNeedMem(sizeof(*newAli));
            ali->nEnd = ne;
            ali->hEnd = he;
            if (needleEnd - ne < 3 || 
                !rightNextMatch(newAli, ne, needleEnd, he, hayEnd, gapPenalty, maxSkip))
                {
                return oldNe != ne; /* We did our best... */
                }
            newAli->left = ali;
            newAli->right = ali->right;
            if (ali->right)
                ali->right->left = newAli;
            ali->right = newAli;
            ali = newAli;
            expandLeft(ali, ne, needleEnd, he, hayEnd, 0, gapPenalty, maxSkip);
            ne = ali->nEnd;
            he = ali->hEnd;
            }
        else 
            {
            ali->nEnd = ne;
            ali->hEnd = he;
            return oldNe != ne;
            }
        }
    }
}

static boolean exactFind(DNA *needle, int nSize, DNA *hay, int hSize, int *hOffset)
/* Look for exact match of needle in haystack. */
{
DNA firstC = needle[0];
DNA *restOfNeedle = needle+1;
int i;
int endIx = hSize - nSize;
int restSize = nSize-1;

for (i = 0; i<=endIx; ++i)
    {
    if (firstC == hay[i])
        {
	if (memcmp(restOfNeedle, hay+i+1, restSize) == 0)
	    {
	    *hOffset = i;
	    return TRUE;
	    }
	}
    }
*hOffset = -1;
return FALSE;
}

static void expandExactRight(struct ffAli *ali, DNA *needleStart, DNA *needleEnd,
    DNA *hayStart, DNA *hayEnd)
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

static void expandExactLeft(struct ffAli *ali, DNA *needleStart, DNA *needleEnd,
    DNA *hayStart, DNA *hayEnd)
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


static int countAlis(struct ffAli *ali)
/* Count number of blocks in alignment. */
{
int count = 0;
if (ali == NULL)
    return 0;
while (ali->left)
    ali = ali->left;
while (ali)
    {
    count += 1;
    ali = ali->right;
    }
return count;
}


struct ffAli *ffRemoveEmptyAlis(struct ffAli *ali, boolean doFree)
/* Remove empty blocks from list. Optionally free empties too. */
{
struct ffAli *leftAli;
struct ffAli *startAli;
struct ffAli *rightAli;

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

static struct ffAli *reconsiderAlignedGaps(struct ffAli *ali)
/* If the gap between two blocks is the same in both needle and
 * haystack, see if we're actually better off with the two
 * blocks merged together. */
{
struct ffAli *a = NULL;
struct ffAli *left = NULL;

if (ali == NULL)
    return NULL;
a = ali;
for (;;)
    {
    int gapSize;

    /* Advance to next ali */
    left = a;
    a = a->right;
    if (a == NULL)
        break;
    gapSize = a->nStart - left->nEnd;
    if (gapSize == a->hStart - left->hEnd)
        {
        int gapScore;
        int matchScore;
        gapScore = -ffCdnaGapPenalty(left, a);
        matchScore = dnaScoreMatch(left->nEnd, left->hEnd, gapSize);
        if (matchScore > gapScore)
            {
            /* Make current cover left. RemoveEmpty will take
             * care of empty shell of left later. */
            a->hStart = left->hEnd = left->hStart;
            a->nStart = left->nEnd = left->nStart;
            }
        }
    }
return ali;
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

static struct ffAli *trimAlis(struct ffAli *aliList)
/* If ends of an ali don't match trim them off. */
{
struct ffAli *ali;
int trimCount = 0;
for (ali = aliList; ali != NULL; ali = ali->right)
    {
    while (ali->nStart[0] != ali->hStart[0] && ali->nStart < ali->nEnd)
        {
        ali->nStart += 1;
        ali->hStart += 1;
        ++trimCount;
        }
    while (ali->nEnd[-1] != ali->hEnd[-1] && ali->nEnd > ali->nStart)
        {
        ali->nEnd -= 1;
        ali->hEnd -= 1;
        ++trimCount;
        }
    }
return aliList;
}

boolean expandThroughNRight(struct ffAli *ali, DNA *needleStart, DNA *needleEnd,
    DNA *hayStart, DNA *hayEnd)
{
DNA *nEnd = ali->nEnd;
DNA *hEnd = ali->hEnd;
DNA n,h;
boolean expanded = FALSE;
while (nEnd < needleEnd && hEnd < hayEnd)
    {
    n = *nEnd;
    h = *hEnd;
    if (n == h || n == 'n' || h == 'n')
        {
        nEnd += 1;
        hEnd += 1;
        expanded = TRUE;
        }
    else
        break;
    }
ali->nEnd = nEnd;
ali->hEnd = hEnd;
return expanded;
}

boolean expandThroughNLeft(struct ffAli *ali, DNA *needleStart, DNA *needleEnd,
    DNA *hayStart, DNA *hayEnd)
{
DNA *nStart = ali->nStart-1;
DNA *hStart = ali->hStart-1;
DNA n,h;
boolean expanded = FALSE;
while (nStart >= needleStart && hStart >= hayStart)
    {
    n = *nStart;
    h = *hStart;
    if (n == h || n == 'n' || h == 'n')
        {
        nStart -= 1;
        hStart -= 1;
        expanded = TRUE;
        }
    else
        break;
    }
ali->nStart = nStart + 1;
ali->hStart = hStart + 1;
return expanded;
}

static struct ffAli *expandAlis(struct ffAli *ali, DNA *nStart, DNA *nEnd, DNA *hStart, DNA *hEnd, 
    int gapPenalty, int maxSkip)
/* Expand alignment to cover in-between tiles as well. */
{
struct ffAli *a, *left, *right;
DNA *ns, *ne, *hs, *he;
boolean expanded = TRUE;
int expCount = 0;

while (expanded)
    {
    expanded = FALSE;
    /* Loop through expanding three times. */
    /* First just expand through N's that don't require an insertion/deletion. */
    for (a = ali; a != NULL; a = a->right)
        {
        if ((left = a->left) != NULL)
            {
            ns = left->nEnd;
            hs = left->hEnd;
            }
        else
            {
            ns = nStart;
            hs = hStart;
            }
        if ((right = a->right) != NULL)
            {
            ne = right->nStart;
            he = right->hStart;
            }
        else
            {
            ne = nEnd;
            he = hEnd;
            }
        expanded |= expandThroughNLeft(a, ns, ne, hs, he);
        expanded |= expandThroughNRight(a, ns, ne, hs, he);
        }
    /* Second do other expansion that doesn't require an insertion/deletion. */
    for (a = ali; a != NULL; a = a->right)
        {
        if ((left = a->left) != NULL)
            {
            ns = left->nEnd;
            hs = left->hEnd;
            }
        else
            {
            ns = nStart;
            hs = hStart;
            }
        if ((right = a->right) != NULL)
            {
            ne = right->nStart;
            he = right->hStart;
            }
        else
            {
            ne = nEnd;
            he = hEnd;
            }
        expanded |= expandLeft(a, ns, ne, hs, he, 0, gapPenalty, maxSkip);
        expanded |= expandRight(a, ns, ne, hs, he, 0, gapPenalty, maxSkip);
        }
    /* Finally do insertion/deletion. */
    for (a = ali; a != NULL; a = a->right)
        {
        if ((left = a->left) != NULL)
            {
            ns = left->nEnd;
            hs = left->hEnd;
            }
        else
            {
            ns = nStart;
            hs = hStart;
            }
        if ((right = a->right) != NULL)
            {
            ne = right->nStart;
            he = right->hStart;
            }
        else
            {
            ne = nEnd;
            he = hEnd;
            }
        expanded |= expandLeft(a, ns, ne, hs, he, 1, gapPenalty, maxSkip);
        expanded |= expandRight(a, ns, ne, hs, he, 1, gapPenalty, maxSkip);
        }
    while (ali->left)
        ali = ali->left;
    }
return ali;
}



DNA *matchInMem(DNA *ns, DNA *ne, DNA *hs, DNA *he)
{
int nLen = ne - ns;
DNA c = *ns++;
he -= nLen;
nLen -= 1;
while (hs < he)
    {   
    if (*hs++ == c && memcmp(ns, hs, nLen) == 0)
        {
        return hs-1;
        }
    }
return NULL;
}

struct ffAli *findAliBetween(DNA *tile, int tileSize, DNA *ns, DNA *ne, DNA *hs, DNA *he)
{
DNA *match;
DNA *tileEnd = tile + tileSize;
int tries = 0;
for (;;)
    {
    if ((match = matchInMem(tile, tileEnd, hs, he)) == NULL)
        return NULL;
    if (matchInMem(tile, tileEnd, match+1, he) == NULL)
        {
        /* Got exactly one match, whoopie! */
        struct ffAli *ali = ffNeedMem(sizeof(*ali));
        ali->nStart = tile;
        ali->nEnd = tileEnd;
        ali->hStart = match;
        ali->hEnd = match + (tileEnd-tile);
        expandExactLeft(ali, ns, ne, hs, he);
        expandExactRight(ali, ns, ne, hs, he);
        return ali;
        }

    /* Got more than one match.  Expand tile to make it more specific.
     * In general first add base to start, then base to beginning.  If
     * at beginning or end already are constrained.  If already spanning
     * full needle, can't expand, so you're done. */
    if (tile <= ns)
        {
        if (tileEnd >= ne)
            return NULL;
        ++tileEnd;
        }
    else if (tile >= tileEnd)
        {
        --tile;
        }
    else if (tries & 1)
        {
        ++tileEnd;
        }
    else
        {
        --tile;
        }    
    ++tries;
    }    
}

double evalExactAli(struct ffAli *ali, DNA *ns, DNA *ne, DNA *hs, DNA *he, int numTiles,
    double freq[4])
{
int haySize = he-hs;
int needleSize = ne-ns;
double prob = 1.0;
double allPossibles = haySize*numTiles;

for (;ali != NULL;ali=ali->right)
    {
    double p = oligoProb(ali->nStart, ali->nEnd - ali->nStart, freq) * allPossibles;
    if (p < 1.0)
        prob *= p;
    }
return prob;
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

void ffUnlink(struct ffAli **pList, struct ffAli *el)
/* Unlink element from doubly linked list. If leftmost
 * element update list pointer. */
{
struct ffAli *list = *pList;
struct ffAli *right = el->right;
struct ffAli *left = el->left;

if (el == list)    /* If first element need to update list. */
    *pList = right;
if (right != NULL)
    right->left = left;
if (left != NULL)
    left->right = right;
el->left = el->right = NULL;
}

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

void unlinkAli(struct ffAli **pList, struct ffAli *ali)
/* Unlink ali from list. */
{
ffUnlink(pList, ali);
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

struct protoGene
    {
    struct protoGene *left;
    struct protoGene *right;
    struct ffAli *hits;
    DNA *hStart, *hEnd, *nStart, *nEnd;
    int score;
    };

int cmpProtoSize(const void *va, const void *vb)
/* Sort biggest size first. */
{
const struct protoGene *a = *((struct protoGene **)va);
const struct protoGene *b = *((struct protoGene **)vb);
int aSize, bSize;
aSize = a->nEnd - a->nStart;
bSize = b->nEnd - b->nStart;
return (bSize - aSize);
}

int cmpProtoNeedle(const void *va, const void *vb)
/* Sort smallest offset into needle first. */
{
const struct protoGene *a = *((struct protoGene **)va);
const struct protoGene *b = *((struct protoGene **)vb);
return (a->nStart - b->nStart);
}

int cmpProtoScore(const void *va, const void *vb)
/* Sort biggest score first. */
{
const struct protoGene *a = *((struct protoGene **)va);
const struct protoGene *b = *((struct protoGene **)vb);
return (b->score - a->score);
}

boolean canAdd(struct protoGene *a, struct protoGene *b)
/* Can b fit into a?  It can if it doesn't overlap
 * by more than 1/4 with what's already there. */
{
struct ffAli *pa;
DNA *bnEnd = b->nEnd;
DNA *bnStart = b->nStart;
DNA *bhEnd = b->hEnd;
DNA *bhStart = b->hStart;
int bSize = bnEnd - bnStart;
int maxOverlap;
for (pa = a->hits; pa != NULL; pa = pa->right)
    {
    int aSize = pa->nEnd - pa->nStart;
    DNA *start = (bnStart > pa->nStart ? bnStart : pa->nStart);
    DNA *end = (bnEnd < pa->nEnd ? bnEnd : pa->nEnd);
    maxOverlap = (aSize < bSize ? aSize : bSize) / 4;
    if (maxOverlap < 2) maxOverlap = 2;
    if (end - start >= maxOverlap)
       return FALSE;
    start = (bhStart > pa->hStart ? bhStart : pa->hStart);
    end = (bhEnd < pa->hEnd ? bhEnd : pa->hEnd);
    if (end - start >= maxOverlap)
        return FALSE;
    }
return TRUE;
}

boolean bestMerger(struct protoGene *list, enum ffStringency stringency,
    DNA *ns, DNA *hs, struct protoGene **retA, struct protoGene **retB)
/* Figure out best scoring merger in list. */
{
int bestScore = -0x7fffffff;
int score;
struct protoGene *bestA = NULL, *bestB = NULL;
struct protoGene *a, *b;
boolean isCdna = (stringency == ffCdna);

if (list == NULL)
    return FALSE;

for (a = list; a != NULL; a = a->right)
    {
    for (b = a->right; b != NULL; b = b->right)
        {
        if (canAdd(a,b))
            {
            int hGap = b->hStart - a->hEnd;
            int nGap = b->nStart - a->nEnd;
            if (hGap < 0)
                {
                hGap = -8*hGap;
                if (!isCdna || hGap < 32)
                    hGap = (hGap*hGap);
                }
            if (nGap < 0)
                nGap = -8*nGap;
            score = -hGap - nGap*nGap;
            if (score > bestScore)
                {
                bestA = a;
                bestB = b;
                bestScore = score;
                }
            }
        }
    }
*retA = bestA;
*retB = bestB;
return bestA != NULL;
}

void mergeProtoGenes(struct protoGene **pList, struct protoGene *a, struct protoGene *b)
/* Absorb protoGene b into protoGene a. */
{
ffUnlink((struct ffAli**)pList, (struct ffAli *)b);
ffCat(&a->hits, &b->hits);
if (a->hStart > b->hStart)
    a->hStart = b->hStart;
if (a->nStart > b->nStart)
    a->nStart = b->nStart;
if (a->hEnd < b->hEnd)
    a->hEnd = b->hEnd;
if (a->nEnd < b->nEnd)
    a->nEnd = b->nEnd;
}

void lumpProtoGenes(struct protoGene **pList,
    DNA *ns, DNA *hs, enum ffStringency stringency)
/* Lump together as many protogenes as we can. */
{
struct protoGene *a, *b;

while (bestMerger(*pList, stringency, ns, hs, &a, &b))
    {
    mergeProtoGenes(pList, a, b);
    }
}

struct protoGene *lumpHits(struct ffAli **pHitList, DNA *ns, DNA *hs)
/* Lump together as many hits as can. Criteria - they must be close
 * to same "diagonal." That is the distance between them must be
 * nearly the same in the needle and the haystack. */
{
struct protoGene proto;
struct protoGene *retProto;
struct ffAli *ali, *nextAli;
int dif, lastDif;
int lumpCount = 1;

if ((ali = *pHitList) == NULL)
    return NULL;

/* Move first hit onto our crude exon. */
nextAli = ali->right;
proto.left = proto.right = NULL;
unlinkAli(pHitList, ali);
proto.hits = ali;
proto.hStart = ali->hStart;
proto.hEnd = ali->hEnd;
proto.nStart = ali->nStart;
proto.nEnd = ali->nEnd;
lastDif = ali->hStart - ali->nStart;

/* Go through rest of list and put others that are close to
 * on the same diagonal onto this exon. */
while ((ali = nextAli) != NULL)
    {
    nextAli = ali->right;
    dif = ali->hStart - ali->nStart;
    if (lastDif - 2 <= dif && dif <= lastDif + 2)
        {
        lastDif = dif;
        unlinkAli(pHitList, ali);
        ali->left = proto.hits;
        proto.hits = ali;
        proto.hEnd = ali->hEnd;
        proto.nEnd = ali->nEnd;
        ++lumpCount;
        }
    }
proto.hits = ffMakeRightLinks(proto.hits);
retProto = ffNeedMem(sizeof(*retProto));
memcpy(retProto, &proto, sizeof(*retProto));
return retProto;
}

void removeThrowbackHits(struct protoGene *proto)
/* Remove hits that cause backtracking in hay coordinates. */
{
struct ffAli *left, *right;
boolean gotThrowback = FALSE;
right = proto->hits;
for (;;)
    {
    left = right;
    right = right->right;
    if (right == NULL)
        break;
    if (left->hStart > right->hStart)
        {
        int lSize = left->nEnd - left->nStart;
        int rSize = right->nEnd - right->nStart;
        gotThrowback = TRUE;
        if (lSize > rSize)
            {
            /* Empty out right side. */
            right->hStart = right->hEnd;
            right->nStart = right->nEnd;
            }
        else
            {
            /* Empty out left side. */
            left->hStart = left->hEnd;
            left->nStart = left->nEnd;
            }
        }
    }
if (gotThrowback)
    proto->hits = ffRemoveEmptyAlis(proto->hits, FALSE);
}


void thinProtoList(struct protoGene **pList, int maxToTake)
/* Reduce list to only top scoring members.  At this
 * point protoList is singly linked on left. */
{
struct protoGene *newList = NULL, *pg, *next;
int i;

slSort(pList, cmpProtoScore);
for (pg=*pList, i=0; pg != NULL && i < maxToTake; pg = next, ++i)
    {
    next = pg->left;
    pg->left = newList;
    newList = pg;
    }
*pList = newList;
}

static struct ffAli *weaveAli(struct ffAli *hitList, 
    DNA *ns, DNA *ne, DNA *hs, DNA *he, double freq[4], int *rBestVal, 
    enum ffStringency stringency)
/* Weave together best looking alignment out of the hitList list. */
{
struct ffAli *ali, *lastAli;
struct protoGene *proto, *protoList = NULL;
int protoCount = 0;

/* Sort initial hits and remove duplicates. */
ffAliSort(&hitList, ffCmpHitsHayFirst);
ali = hitList;
for (;;)
    {
    lastAli = ali;
    if ((ali = ali->right) == NULL)
        break;
    if (ali->hStart == lastAli->hStart && ali->hEnd == lastAli->hEnd
       && ali->nStart == lastAli->nStart && ali->nEnd == lastAli->nEnd)
       {
       unlinkAli(&hitList, lastAli);
       }
    }

/* Lump together things which look to be separated only by small
 * amounts of noise. */
while ((proto = lumpHits(&hitList, ns, hs)) != NULL)
    {
    proto->left = protoList;
    protoList = proto;
    ++protoCount;
    }
if (protoCount > 200)
    thinProtoList(&protoList, 200);
protoList = (struct protoGene *)ffMakeRightLinks((struct ffAli*)protoList);


/* Lump together any other ones that can, starting with ones
 * that go together best. */
ffAliSort((struct ffAli**)&protoList, cmpProtoNeedle);
lumpProtoGenes(&protoList, ns, hs, stringency );
for (proto = protoList; proto != NULL; proto = proto->right)
    {
    ffAliSort((struct ffAli**)&proto->hits, ffCmpHitsNeedleFirst);
    removeThrowbackHits(proto);
    proto->score = ffScore(proto->hits,stringency);
    }

/* Sort by score and return the best. */
ffAliSort((struct ffAli **)&protoList, cmpProtoScore);
*rBestVal = protoList->score;
return protoList->hits;
}

/* This set of variables is set before calling the recursive tile finders - the
 * below two routines. */
static double rwFreq[4];
static boolean rwIsCdna;
static boolean rwCheckGoodEnough;

static struct ffAli *rwFindTilesBetween(DNA *ns, DNA *ne, DNA *hs, DNA *he, 
    enum ffStringency stringency, double probMax)
/* Search for more or less regularly spaced exact matches that are
 * in the right order. */
{
int searchOffset;
int endTileOffset;
int haySize = he - hs;
int needleSize = ne - ns;
int numTiles = 0;
int bestWeaveVal;
int tileIx;
struct ffAli *hitList = NULL;
struct ffAli *bestAli;
struct ffAli *ali;
int possibleTiles;
double tileProbOne;

possibleTiles = (haySize - nextPowerOfFour(needleSize));
if (possibleTiles < 1) possibleTiles = 1;
tileProbOne = probMax/possibleTiles;

searchOffset = 0;
endTileOffset = 0;
tileIx = 0;
for (;;)
    {
    DNA *tile;
    int tileSize;
    double tileProb;
    DNA *h = hs;

    searchOffset = endTileOffset;
    if (!ffFindGoodOligo(ns+searchOffset, needleSize-searchOffset, tileProbOne,
        rwFreq, &tile, &tileSize, &tileProb))
        {
        break;
        }
    /* Find all parts that match the tile. */
    for (;;)
        {
        if ((h = matchInMem(tile, tile+tileSize, h, he)) == NULL)
            break;
        ali = ffNeedMem(sizeof(*ali));
        ali->hStart = h;
        ali->hEnd = ali->hStart + tileSize;
        ali->nStart = tile;
        ali->nEnd = tile + tileSize;
        ali->left = hitList;
        hitList = ali;
        h += tileSize;
        } 
    endTileOffset = tile + tileSize - ns;
    ++numTiles;
    }
if (hitList == NULL)
    return NULL;

hitList = ffMakeRightLinks(hitList);
for (ali = hitList; ali != NULL; ali = ali->right)
    {
    expandExactLeft(ali, ns, ne, hs, he);
    expandExactRight(ali, ns, ne, hs, he);
    }
bestAli = weaveAli(hitList, ns, ne, hs, he, rwFreq, &bestWeaveVal, stringency);
if (rwCheckGoodEnough)
    {
    double prob;
    prob = evalExactAli(bestAli, ns, ne, hs, he, numTiles, rwFreq);
    if (prob > 0.1)
        return NULL;
    rwCheckGoodEnough = FALSE;
    }

return bestAli;
}

static struct ffAli *recursiveWeave(DNA *ns, DNA *ne, DNA *hs, DNA *he, 
    enum ffStringency stringency, double probMax, int level)
/* Find a set of tiles, then recurse to find set of tiles between the tiles
 * at somewhat lower stringency. */
{
struct ffAli *left = NULL, *right = NULL, *aliList;

aliList = rwFindTilesBetween(ns, ne, hs, he, stringency, probMax);
if (aliList != NULL)
    {
    DNA *lne, *rns, *lhe, *rhs;
    int ndif, hdif;
    right = aliList;
    for (;;)
        {
        /* Figure out the end points to recurse to. */
        if (left != NULL)
            {
            lne = left->nEnd;
            lhe = left->hEnd;
            }
        else
            {
            lne = ns;
            lhe = hs;
            }
        if (right != NULL)
            {
            rns = right->nStart;
            rhs = right->hStart;
            }
        else
            {
            rns = ne;
            rhs = he;
            }
        ndif = rns-lne;
        hdif = rhs-lhe;
        /* If a big enough gap left recurse. */
        if (ndif >= 5 && hdif >= 5)
            {
            struct ffAli *newLeft, *newRight;
            newLeft = recursiveWeave(lne, rns, lhe, rhs, stringency, probMax*2, level+1);
            if (newLeft != NULL)
                {
                /* Insert new tiles between left and right. */
                
                /* Find right end of tile set. */
                newRight = newLeft;
                while (newRight->right != NULL)
                    newRight = newRight->right;
                
                
                if (left != NULL)
                    {
                    left->right = newLeft;
                    newLeft->left = left;
                    }
                else
                    {
                    aliList = newLeft;
                    }
                if (right != NULL)
                    {
                    right->left = newRight;
                    newRight->right = right;
                    }
                }
            }
        if ((left = right) == NULL)
            break;
        right = right->right;
        }
    }
return aliList;
}


static struct ffAli *findWovenTiles(DNA *ns, DNA *ne, DNA *hs, DNA *he, enum ffStringency stringency)
{
struct ffAli *bestAli;
int haySize = he - hs;
int needleSize = ne - ns;
static double tileStrinProbMult[] = { 0.0001, 0.001, 0.001, 0.5, };
                                    /* exact  cDNA        tight    loose */
if (needleSize < 2 || haySize < 2)  /* Be serious man! */
    return NULL;

/* Set up rwVariables - essentially locals except that they don't change over the course
 * of the recursion. */
makeFreqTable(hs, haySize, rwFreq);
rwIsCdna = (stringency == ffCdna);
rwCheckGoodEnough = (stringency == ffTight || stringency == ffCdna);

bestAli = recursiveWeave(ns, ne, hs, he, stringency, tileStrinProbMult[stringency], 1);

return bestAli;
}

static int scoreIntron(DNA a, DNA b, DNA y, DNA z)
/* Return a better score the closer an intron is to
 * consensus. */
{
int score = 0;
int revScore = 0;

if (a == 'g') ++score;
if (b == 't') ++score;
if (y == 'a') ++score;
if (z == 'g') ++score;

if (a == 'c') ++revScore;
if (b == 't') ++revScore;
if (y == 'a') ++revScore;
if (z == 'c') ++revScore;

return score > revScore ? score : revScore;
}

static int slideIntron(struct ffAli *left, struct ffAli *right)
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

/* Slide as far to the left as possible. */
while (nLeft > nLeftEnd)
    {
    nl = nLeft[-1];
    hl = hLeft[-1];
    nr = nRight[-1];
    hr = hRight[-1];
    if (!(nl == 'n' && nr == 'n'))  /* N's in needle freely slide. */
        {
        if (nl != hl || nr != hr || hl != hr)
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
    curScore = scoreIntron(hLeft[0], hLeft[1], hRight[-2], hRight[-1]);
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
    if (nr != 'n' && nr != hr)
        break;
    if (hl != hr)
        break;
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


void ffSlideIntrons(struct ffAli *ali)
/* Slide introns (or spaces between aligned blocks)
 * to match consensus. */
{
struct ffAli *left = ali, *right;
int slid;

if (left == NULL)
    return;
while((right = left->right) != NULL)
    {
    slid = slideIntron(left, right);
    left = right;
    }
}

static struct ffAli *findBestAli(DNA *ns, DNA *ne, DNA *hs, DNA *he, enum ffStringency stringency)
{
static int iniExpGapPen[] = {0 /* (exact) */, 4 /* cDna */, 4 /* tight */, 4 /* loose */};
static int addExpGapPen[] = {0 /* (exact) */, 3 /* cDna */, 3 /* tight */, 3 /* loose */};
static int midTileMinSize[] ={0 /*(exact) */,12 /* cDNA */, 12 /* tight */, 4 /* loose */};

struct ffAli *bestAli;
int matchSize;
int exactOffset;
int needleSize = ne-ns;

/* First check for an exact match and quickly return it if it exists. */
if (exactFind(ns, ne-ns, hs, he-hs, &exactOffset))
    {
    struct ffAli *ali = ffNeedMem(sizeof(*ali));
    ali->nStart = ns;
    ali->nEnd = ne;
    ali->hStart = hs + exactOffset;
    ali->hEnd = ali->hStart + needleSize;
    if (ali->hEnd > he)
        ali->hEnd = he;
    return ali;
    }
if (stringency == ffExact)
    return NULL;

matchSize = nextPowerOfFour(he-hs)+1;
if (matchSize < midTileMinSize[stringency])
    matchSize = midTileMinSize[stringency];

bestAli = findWovenTiles(ns, ne, hs, he, stringency);
if (bestAli == NULL)
    return NULL;

bestAli = ffMergeNeedleAlis(bestAli, FALSE);
bestAli = expandAlis(bestAli,ns,ne,hs,he,iniExpGapPen[stringency], 1);
bestAli = ffMergeNeedleAlis(bestAli, FALSE);
bestAli = expandAlis(bestAli,ns,ne,hs,he,addExpGapPen[stringency], 2*matchSize);
bestAli = trimAlis(bestAli);
bestAli = ffMergeNeedleAlis(bestAli, FALSE);
bestAli = ffMergeHayOverlaps(bestAli);
bestAli = reconsiderAlignedGaps(bestAli);
bestAli = ffRemoveEmptyAlis(bestAli, FALSE);
bestAli = ffMergeNeedleAlis(bestAli, FALSE);
if (stringency == ffCdna)
    ffSlideIntrons(bestAli);
bestAli = ffRemoveEmptyAlis(bestAli, FALSE);
return bestAli;
}


static struct ffAli *saveAliToPermanentMem(struct ffAli *volatileAli)
/* Save alignment to memory that doesn't get thrown away. */
{
struct ffAli *leftList = NULL;
struct ffAli *newAli, *ali;
struct ffAli *rightList = NULL;

/* Allocate memory, copy contents and set left pointer. */
for (ali = volatileAli; ali != NULL; ali = ali->right)
    {
    newAli = needMem(sizeof(*newAli));
    if (newAli == NULL)
        {
        slFreeList(leftList);
        ffAbort();
        }
    memcpy(newAli, ali, sizeof(*newAli));
    newAli->left = leftList;
    leftList = newAli;
    }

/* Set right pointer. */
for (ali = leftList; ali != NULL; ali = ali->left)
    {
    ali->right = rightList;
    rightList = ali;
    }
return rightList;
}

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

struct ffAli *ffFind(DNA *needleStart, DNA *needleEnd, DNA *hayStart, DNA *hayEnd,
     enum ffStringency stringency)
/* Return an alignment of needle in haystack. */
{
struct ffAli *bestAli;
int status;

assert(needleStart <= needleEnd);
assert(hayStart <= hayEnd);

ffMemInit();
dnaUtilOpen();


/* Set up error recovery. */
status = setjmp(ffRecover);
if (status == 0)    /* Always true except after long jump. */
    {
    pushAbortHandler(ffAbort);
    bestAli = findBestAli(needleStart, needleEnd, hayStart, hayEnd, stringency);
    ffCountGoodEnds(bestAli);
    bestAli = saveAliToPermanentMem(bestAli);
    }
else    /* They long jumped here because of an error. */
    {
    bestAli = NULL;
    }
popAbortHandler();
ffMemCleanup();
return bestAli;
}

boolean ffFindAndScore(DNA *needle, int needleSize, DNA *haystack, int haySize,
    enum ffStringency stringency, struct ffAli **pAli, boolean *pRcNeedle, int *pScore)
/* Return TRUE if find an allignment using needle, or reverse complement of 
 * needle to search haystack. DNA must be lower case. If pScore is non-NULL returns
 * score of alignment. */
{
int fScore, rScore;
struct ffAli *fAli, *rAli;

/* Get forward alignment and score it. */
fAli = ffFind(needle, needle+needleSize, haystack, haystack+haySize, stringency);
fScore = ffScore(fAli,stringency);

/* Get reverse alignment and score it. */
reverseComplement(needle, needleSize);
rAli = ffFind(needle, needle+needleSize, haystack, haystack+haySize, stringency);
rScore = ffScore(rAli,stringency);
reverseComplement(needle, needleSize);

/* If no good alignment on either strand return FALSE. */
if (fAli == NULL && rAli == NULL)
    {
    *pAli = NULL;
    return FALSE;
    }

/* Return TRUE with best alignment.  Free the other one. */
if (fScore > rScore)
    {
    *pAli = fAli;
    *pRcNeedle = FALSE;
    if (pScore != NULL)
        *pScore = fScore;
    ffFreeAli(&rAli);
    }   
else
    {
    *pAli = rAli;
    *pRcNeedle = TRUE;
    if (pScore != NULL)
        *pScore = rScore;
    ffFreeAli(&fAli);
    }
return TRUE;
}

boolean ffFindEitherStrandN(DNA *needle, int needleSize, DNA *haystack, int haySize,
    enum ffStringency stringency, struct ffAli **pAli, boolean *pRcNeedle)
/* Return TRUE if find an allignment using needle, or reverse complement of 
 * needle to search haystack. DNA must be lower case. */
{
return ffFindAndScore(needle, needleSize, haystack, haySize, stringency, pAli, pRcNeedle, NULL);
}

boolean ffFindEitherStrand(DNA *needle, DNA *haystack, enum ffStringency stringency,
    struct ffAli **pAli, boolean *pRcNeedle)
/* Return TRUE if find an alignment using needle, or reverse complement of 
 * needle to search haystack. DNA must be lower case. Needle and haystack
 * are zero terminated. */
{
return ffFindEitherStrandN(needle, strlen(needle), haystack, strlen(haystack),
stringency, pAli, pRcNeedle);
}
