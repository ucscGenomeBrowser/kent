/* ffScore - stuff to score ffAli alignments. */

#include "common.h"
#include "dnautil.h"
#include "obscure.h"
#include "fuzzyFind.h"


int ffIntronMax = ffIntronMaxDefault;

void setFfIntronMax(int value)
{
ffIntronMax = value;
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

int ffCalcCdnaGapPenalty(int hGap, int nGap)
/* Return gap penalty for given h and n gaps. */
{
int acc = 2;
if (hGap > 400000)	/* Discourage really long introns. */
    {
    acc += (hGap - 400000)/3000;
    if (hGap > ffIntronMax)
        acc += (hGap - ffIntronMax)/2000;
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
acc += digitsBaseTwo(hGap)/2;
if (nGap != 0)
    {
    acc += digitsBaseTwo(nGap);
    }
else
    {
    if (hGap > 30)
	acc -= 1;
    }
return acc;
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

int ffScoreSomething(struct ffAli *ali, enum ffStringency stringency,
   boolean isProt)
/* Score alignment. */
{
int score = 0;
int oneScore;
int (*scoreMatch)(char *a, char *b, int size);

if (ali == NULL)
    return -0x7FFFFFFF;
scoreMatch = (isProt ? aaScoreMatch : dnaScoreMatch );
while (ali->left != NULL) ali = ali->left;
while (ali != NULL)
    {
    int len = ali->hEnd - ali->hStart;
    struct ffAli *right = ali->right;
    oneScore = scoreMatch(ali->hStart, ali->nStart, len);
    score += oneScore;
    if (right)  /* Calculate gap penalty */
        {
        score -= ffGapPenalty(ali, right, stringency);
        }
    ali = right;
    }
return score;
}

int ffScore(struct ffAli *ali, enum ffStringency stringency)
/* Score alignment. */
{
return ffScoreSomething(ali, stringency, FALSE);
}

int ffScoreCdna(struct ffAli *ali)
/* Figure out overall score of this alignment. 
 * Perfect match is number of bases in needle. */
{
return ffScore(ali, ffCdna);
}

int ffScoreProtein(struct ffAli *ali, enum ffStringency stringency)
/* Figure out overall score of protein alignment. */
{
return ffScoreSomething(ali, stringency, TRUE);
}

