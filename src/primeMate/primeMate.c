#include <string.h>
#include "common.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "dnautil.h"

/* Below is a crude hack to keep program from
 * hanging.  Limits iteration of inner loop. */
int maxLoopCount = 5000;

char *skipFirstWord(char *s)
{
s += strspn(s, whiteSpaceChopper);
s += strcspn(s, whiteSpaceChopper);
s += strspn(s, whiteSpaceChopper);
return s;
}

int calcTm(char *dna, int size)
{
int res = 0;
int i;
char c;
for (i=0; i<size; ++i)
    {
    c = ntChars[(int)(*dna++)];
    if (c == 'a' || c == 't')
	res += 2;
    else if (c == 'g' || c == 'c')
	res += 4;
    else 
	res += 3;  /* Hell, we'll do our best. */
    }
return res;
}

boolean oneHairpin(char *dna,int dnaSize,int pinSize)
{
int startIx;
int endIx;
int i;
boolean gotPin;

for (startIx = 0; startIx <= dnaSize-pinSize*2; startIx += 1)
    {
    for (endIx = dnaSize; endIx >= startIx + 2*pinSize; endIx -= 1)
	{
	gotPin = TRUE;
	if (--maxLoopCount <= 0)
	    errAbort("Sorry, this is taking too long to calculate, "
	     "Please reduce the sequence size and try again.");
	for (i=0; i<pinSize; i+=1)
	    {
	    if (dna[startIx+i] != ntCompTable[(int)dna[endIx-i-1]])
		{
		gotPin = FALSE;
		break;
		}
	    }
	if (gotPin)
	    {
	    return TRUE;
	    }
	}
    }
return FALSE;
}


long score2(char *as, int aSize, char *bs, int bSize)
{
int size = (aSize > bSize ? bSize : aSize);
int i;
long score = 0;
long contigScore = 0;
long bestContigScore = 0;
int baseScore = 0;
boolean inMatch = FALSE;
char a,b;


for (i=0; i<size; ++i)
    {
    a = *as++;
    b = *bs++;
    if (a == b)
	{
	if (a == 'c' || a == 'g') baseScore = 4;
	else if (a == 'a' || a == 't') baseScore = 2;
	score += baseScore;
	contigScore += baseScore;
	inMatch = TRUE;
	}
    else
	{
	if ((a == 'g' && b == 't') || (a == 't' && b == 'u')) /* Wobble */
	    score += 1;
	else
	    score += -4;
	if (inMatch)
	    {
	    if (contigScore > bestContigScore) 
		bestContigScore = contigScore;
	    contigScore = 0;
	    inMatch = FALSE;
	    }
	}
    }
return (score > bestContigScore ? score : bestContigScore);
}

long bestScore(char *a, int aSize, char *b, int bSize)
{
long bestScore = -0x7000000;
long score;
int i;
for (i=0;i<aSize; i++)
    {
    score = score2(a+i, aSize-i, b, bSize);
    if (score > bestScore) bestScore = score;
    }
for (i=0; i<bSize; i++)
    {
    score = score2(a, aSize, b+i, bSize-i);
    if (score > bestScore) bestScore = score;
    }
return bestScore;
}

/* Find a primer in source that meets the following specifications:
       1. It is within 3 degrees of tm.
       2. It has no hairpins.
       3. It doesn't complement with compo.
       4. It ends in a C or a G.
   Return TRUE if found one, and put it in the return variables
   pPrimer and pPrimerSize.

   If the compo var is null, it won't be checked.
 */
boolean findPrimer(char *source, int sourceSize, 
    char *compo, int compoSize, char *compoRcBuf, 
    int desiredTm, int tmRange, char **pPrimer, int *pPrimerSize)
{
int startIx;
int endIx;
int tm;
int hairpinCutoff = 4;
int compoCutoff = 16;
char firstBase;

if (compo != NULL)
    {
    memcpy(compoRcBuf, compo, compoSize);
    reverseComplement(compoRcBuf, compoSize);
    }
for (startIx=0; startIx<sourceSize; ++startIx)
    {
    firstBase = source[startIx];
    if (firstBase == 'c' || firstBase == 'g')
	{
	for (endIx = startIx+1; endIx <= sourceSize; ++endIx)
	    {
	    tm = calcTm(source + startIx, endIx - startIx);
	    if (desiredTm - tmRange <= tm  && tm < desiredTm + tmRange)
		{
		if (!oneHairpin(source+startIx, endIx - startIx,
		    hairpinCutoff))
		    {
		    if (compo == NULL || 
			bestScore( source+startIx, endIx-startIx,
					     compoRcBuf, compoSize) <
					     compoCutoff)
			{
			*pPrimer = source+startIx;
			*pPrimerSize = endIx-startIx;
			return TRUE;
			}
		    }
		}
	    }
	}
    }
return FALSE;
}

/* Return length of longest string in list */
int longestStrlen(char *strings[], int stringCount)
{
int size;
int max = 0;
int i;

for (i=0; i<stringCount; ++i)
    {
    size = strlen(strings[i]);
    if (size > max) max = size;
    }
return max;
}

struct primerPair
    {
    char *forward;
    int forwardSize;
    char *reverse;
    int reverseSize;
    };

boolean findTmPrimerPair(char *leftRcBuf, int tm, int tmRange, 
    char *left, char *right, struct primerPair *pp)
{
char *fPrimer;
int  fPrimerSize;
int leftSize = strlen(left);
int leftStart;
char *rPrimer;
int rPrimerSize;

for (leftStart=0; leftStart<leftSize;++leftStart)
    {
    if (findPrimer(left+leftStart, leftSize-leftStart, 
	NULL, 0, NULL, tm, tmRange, &fPrimer, &fPrimerSize))
	{
	if (findPrimer(right, strlen(right), 
	    left+leftStart, leftSize-leftStart, leftRcBuf,
	    tm, tmRange, &rPrimer, &rPrimerSize))
	    {
	    pp->forward = fPrimer;
	    pp->forwardSize = fPrimerSize;
	    pp->reverse = rPrimer;
	    pp->reverseSize = rPrimerSize;
	    return TRUE;
	    }
	}
    }
return FALSE;
}

boolean findTmPrimerList(int tm, int tmRange, int count, 
    char *lefts[], char *rights[], struct primerPair *primers)
{
int i;
int longestLeftSize = longestStrlen(lefts, count);
char *leftRcBuf = needMem(longestLeftSize);
boolean gotAll = TRUE;

for (i=0; i<count; i += 1)
    {
    if (!findTmPrimerPair(leftRcBuf, tm, tmRange, 
	lefts[i], rights[i], &primers[i]))
	{
	gotAll = FALSE;
	break;
	}
    }
gentleFree(leftRcBuf);
return gotAll;
}

boolean findPrimerList(int count, char *lefts[], char *rights[], 
    struct primerPair *primers, int idealTm)
{
static int tmsToTry[] = {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5};
static int moreTmsToTry[] = {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5,
   6, -6, 7, -7, 8, -8, 9, -9, 10, -10};
int i;
int tm;
int tmRange;

/* First try for an exact fit. */
if (findTmPrimerList(idealTm, 0, count, lefts, rights, primers))
    return TRUE;

/* Then try for a pretty strict fit. */
for (tmRange=0; tmRange<4; tmRange+= 2)
    {
    for (i = 0; i < ArraySize(tmsToTry); i += 1)
	{
	tm = idealTm + tmsToTry[i];
	if (findTmPrimerList(tm, tmRange, count, lefts, rights, primers))
	    return TRUE;
	}
    }
/* If that's not possible loosen up. */
for (tmRange=2; tmRange<10; tmRange+= 2)
    {
    for (i = 0; i < ArraySize(moreTmsToTry); i += 1)
	{
	tm = idealTm + moreTmsToTry[i];
	if (findTmPrimerList(tm, tmRange, count, lefts, rights, primers))
	    return TRUE;
	}
    }
return FALSE;
}

void filterBadChars(char *in, char *out)
/* Remove everything but nucleotides and parenthesis as you
 * copy from in to out.  (In and out may point to same place. */
{
char filter[256];
char c;
zeroBytes(filter, sizeof(filter));
filter['a'] = 'a';
filter['A'] = 'A';
filter['c'] = 'c';
filter['C'] = 'C';
filter['g'] = 'g';
filter['G'] = 'G';
filter['t'] = 't';
filter['T'] = 'T';
filter['('] = '(';
filter[')'] = ')';
while ((c = *in++) != 0)
    {
    if ((c = filter[(int)c]) != 0)
        *out++ = c;
    }
*out++ = 0;
}

void doMiddle()
{
char *dnaLines;
char *origDna;  /* Not chopped up and inverse complemented */
char *left, *right;
struct primerPair primer;
int idealTm;

errAbort("primeMate is currently down due to an overloaded web server");
/* Grab the input. */
dnaLines = cgiString("dnaLines");
idealTm = cgiInt("idealTm");
if (idealTm < 25 || idealTm > 90)
    errAbort("Ideal Tm should be between about 45 and 70, not %d", idealTm);

/* Massage it into a form we can work with. */
tolowers(dnaLines);
    {
    char *oparen, *cparen;

    /* Get input line, make sure it has two parenthesis */
    oparen = strchr(dnaLines, '(');
    if (oparen == NULL)
	errAbort("Missing parenthesis in input.");
    cparen = strchr(oparen, ')');
    if (cparen == NULL)
	errAbort("Missing parenthesis in input.");

    /* Get rid of junk in input. */
    filterBadChars(dnaLines,dnaLines);

    /* Grab stuff to left and right of parenthesis. */
    if ((origDna = strdup(dnaLines)) == NULL)
	errAbort("Out of memory.");
    oparen = strchr(dnaLines, '(');
    cparen = strchr(oparen, ')');
    *oparen = 0;
    left = dnaLines;
    right = cparen;

    /* Reverse complement the left side to make things
     * easier later. */
    reverseComplement(left, strlen(left));
    }
if (findPrimerList(1, &left, &right, &primer, idealTm))
    {
    struct primerPair *pp = &primer;
    char fbuf[128],rbuf[128];
    char *s;

    /* convert primerse to null terminated strings. */
    memcpy(fbuf, pp->forward, pp->forwardSize);
    fbuf[pp->forwardSize] = 0;
    memcpy(rbuf, pp->reverse, pp->reverseSize);
    rbuf[pp->reverseSize] = 0;

    /* Uppercase input corresponding to reverse primer */
    s = strstr(origDna, rbuf);
    if (s == NULL)
	errAbort("How strange, primer is not in the input!");
    toUpperN(s, pp->reverseSize);

    /* Reverse complement both primers, so forward one comes first.  */
    reverseComplement(rbuf, pp->reverseSize);
    reverseComplement(fbuf, pp->forwardSize);

    /* Uppercase input corresponding to forward primer */
    s = strstr(origDna, fbuf);
    if (s == NULL)
	errAbort("Weird, primer is not in the input!");
    toUpperN(s, pp->forwardSize);


    printf("<P>%s<BR>\n", origDna);
    printf("forward %s %d bases Tm = %d<BR>\n", fbuf, pp->forwardSize,
	calcTm(pp->forward, pp->forwardSize));
    printf("reverse %s %d bases Tm = %d</P>\n", rbuf, pp->reverseSize,
	calcTm(pp->reverse, pp->reverseSize));
    }
else
    {
    printf("<P>Sorry, couldn't find suitable primers. Try again with\n");
    printf("more flanking DNA to search.</P>\n");
    }
}

int main(int argc, char *argv[])
{
dnaUtilOpen();
htmShell("PrimeMate Results", doMiddle, "POST");
return 0;
}

