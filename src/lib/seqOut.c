/* seqOut - stuff to output sequences and alignments in web
 * or ascii viewable form.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "obscure.h"
#include "dnautil.h"
#include "fuzzyFind.h"
#include "seqOut.h"
#include "htmshell.h"
#include "axt.h"

static char const rcsid[] = "$Id: seqOut.c,v 1.28 2009/08/21 18:39:59 angie Exp $";

struct cfm *cfmNew(int wordLen, int lineLen,
	boolean lineNumbers, boolean countDown, FILE *out, int numOff)
/* Set up colored sequence formatting for html. */
{
struct cfm *cfm;
AllocVar(cfm);
cfm->inWord = cfm->inLine = cfm->charCount = 0;
cfm->color = 0;
cfm->wordLen = wordLen;
cfm->lineLen = lineLen;
cfm->lineNumbers = lineNumbers;
cfm->countDown = countDown;
cfm->out = out;
cfm->numOff = numOff;
cfm->bold = cfm->underline = cfm->italic = FALSE;
return cfm;
}

static void cfmPopFormat(struct cfm *cfm)
/* Restore format to default. */
{
if (cfm->color != 0)
   fprintf(cfm->out, "</span>");
if (cfm->underline)
  fprintf(cfm->out, "</span>");
if (cfm->bold)
  fprintf(cfm->out, "</B>");
if (cfm->italic)
  fprintf(cfm->out, "</I>");
}

static void cfmPushFormat(struct cfm *cfm)
/* Set format. */
{
if (cfm->italic)
  fprintf(cfm->out, "<I>");
if (cfm->bold)
  fprintf(cfm->out, "<B>");
if (cfm->underline)
  fprintf(cfm->out, "<span style='text-decoration:underline;'>");
if (cfm->color != 0)
  fprintf(cfm->out, "<span style='color:#%06X;'>", cfm->color);
}

void cfmOutExt(struct cfm *cfm, char c, int color, boolean underline, boolean bold, boolean italic)
/* Write out a byte, and formatting extras  */
{
if (color != cfm->color || underline != cfm->underline
   || bold != cfm->bold || italic != cfm->italic)
   {
   cfmPopFormat(cfm);
   cfm->color = color;
   cfm->underline = underline;
   cfm->bold = bold;
   cfm->italic = italic;
   cfmPushFormat(cfm);
   }

++cfm->charCount;
fputc(c, cfm->out);
if (cfm->wordLen)
    {
    if (++cfm->inWord >= cfm->wordLen)
	{
	cfmPopFormat(cfm);
	fputc(' ', cfm->out);
	cfmPushFormat(cfm);
	cfm->inWord = 0;
	}
    }
if (cfm->lineLen)
    {
    if (++cfm->inLine >= cfm->lineLen)
	{
	if (cfm->lineNumbers)
	    {
	    int pos = cfm->charCount;
	    if (cfm->countDown)
		{
	        pos = 1-pos;
		}
	    pos += cfm->numOff;
	    cfmPopFormat(cfm);
	    fprintf(cfm->out, " %d", pos);
	    cfmPushFormat(cfm);
	    }
	fprintf(cfm->out, "\n");
	cfm->inLine = 0;
	}
    }
}

void cfmOut(struct cfm *cfm, char c, int color)
/* Write out a byte, and depending on color formatting extras  */
{
cfmOutExt(cfm, c, color, FALSE, FALSE, FALSE);
}

void cfmFree(struct cfm **pCfm)
/* Finish up cfm formatting job. */
{
struct cfm *cfm = *pCfm;
if (cfm != NULL)
    {
    cfmPopFormat(cfm);
    freez(pCfm);
    }
}

int seqOutColorLookup[] =
    {
    0x000000,
    0x3300FF,
    0x22CCEE,
    0xFF0033,
    0xFFcc22,
    0x00aa00,
    0xFF0000,
    };


void bafInit(struct baf *baf, DNA *needle, int nNumOff,  boolean nCountDown,
	DNA *haystack, int hNumOff, boolean hCountDown, FILE *out,
	int lineSize, boolean isTrans )
/* Initialize block alignment formatter. */
{
baf->cix = 0;
baf->needle = needle;
baf->nCountDown = nCountDown;
baf->haystack = haystack;
baf->nNumOff = nNumOff;
baf->hNumOff = hNumOff;
baf->hCountDown = hCountDown;
baf->out = out;
baf->lineSize = lineSize;
baf->isTrans = isTrans;
baf->nCurPos = baf->hCurPos = 0;
baf->nLineStart = baf->hLineStart = 0;
}

void bafSetAli(struct baf *baf, struct ffAli *ali)
/* Set up block formatter around an ffAli block. */
{
baf->nCurPos = ali->nStart - baf->needle;
baf->hCurPos = ali->hStart - baf->haystack;
}

void bafSetPos(struct baf *baf, int nStart, int hStart)
/* Set up block formatter starting at nStart/hStart. */
{
if (baf->isTrans)
    nStart *= 3;
baf->nCurPos = nStart;
baf->hCurPos = hStart;
}

void bafStartLine(struct baf *baf)
/* Set up block formatter to start new line at current position. */
{
baf->nLineStart = baf->nCurPos;
baf->hLineStart = baf->hCurPos;
}

static int maxDigits(int x, int y)
{
int xDigits = digitsBaseTen(x);
int yDigits = digitsBaseTen(y);
return (xDigits > yDigits ? xDigits : yDigits);
}

void bafWriteLine(struct baf *baf)
/* Write out a line of an alignment (which takes up
 * three lines on the screen. */
{
int i;
int count = baf->cix;
int nStart = baf->nLineStart + 1 + baf->nNumOff;
int hStart = baf->hLineStart + 1 + baf->hNumOff;
int nEnd = baf->nCurPos + baf->nNumOff;
int hEnd = baf->hCurPos + baf->hNumOff;
int startDigits = maxDigits(nStart, hStart);
int endDigits = maxDigits(nEnd, hEnd);
int hStartNum, hEndNum;
int nStartNum, nEndNum;
static struct axtScoreScheme *ss = 0;  /* Scoring scheme. */
struct cfm cfm;
extern char blosumText[];
extern struct axtScoreScheme *axtScoreSchemeFromProteinText(char *text, char *fileName);
boolean revArrows = (baf->nCountDown ^ baf->hCountDown);
char arrowChar = (revArrows ? '<' : '>');

ZeroVar(&cfm);
cfm.out = baf->out;
if (ss == 0)
    ss = axtScoreSchemeFromProteinText(blosumText, "fake");

if (baf->nCountDown)
    {
    nStartNum = baf->nNumOff - baf->nLineStart;
    nEndNum = 1+baf->nNumOff - baf->nCurPos;
    }
else
    {
    nStartNum = 1+baf->nNumOff + baf->nLineStart;
    nEndNum = baf->nNumOff + baf->nCurPos;
    }
fprintf(baf->out, "%0*d ", startDigits, nStartNum);
for (i=0; i<count; ++i)
    fputc(baf->nChars[i], baf->out);
fprintf(baf->out, " %0*d\n", endDigits, nEndNum);

for (i=0; i<startDigits; ++i)
    fputc(arrowChar, baf->out);
fputc(' ', baf->out);
for (i=0; i<count; ++i)
    {
    char n,h,c =  ' ';

    n = baf->nChars[i];
    h = baf->hChars[i];
    if (baf->isTrans)
        {
	if (n != ' ')
	    {
	    DNA codon[4];
	    codon[0] = baf->hChars[i-1];
	    codon[1] = h;
	    codon[2] = baf->hChars[i+1];
	    codon[3] = 0;
	    tolowers(codon);
	    c  = lookupCodon(codon);
	    cfmPushFormat(&cfm);
	    if (toupper(n) == c)
		cfmOut(&cfm, '|', seqOutColorLookup[0]);
	    else
		{
		int color;

		if (c == 0)
		    c = 'X';
		if (ss->matrix[(int)toupper(n)][(int)c] > 0)
		    color = 5;
		else
		    color = 6;
		cfmOut(&cfm, c, seqOutColorLookup[color]);
		}
	    cfmPopFormat(&cfm);
	    }
	else
	    {
	    fputc(c, baf->out);
	    }
	}
    else
        {
	if (toupper(n) == toupper(h))
	     c = '|';
	fputc(c, baf->out);
	}
    }
fputc(' ', baf->out);
for (i=0; i<endDigits; ++i)
    fputc(arrowChar, baf->out);
fprintf(baf->out, "\n");

if (baf->hCountDown)
    {
    hStartNum = baf->hNumOff - baf->hLineStart;
    hEndNum = 1+baf->hNumOff - baf->hCurPos;
    }
else
    {
    hStartNum = 1+baf->hNumOff + baf->hLineStart;
    hEndNum = baf->hNumOff + baf->hCurPos;
    }
fprintf(baf->out, "%0*d ", startDigits, hStartNum);
for (i=0; i<count; ++i)
    fputc(baf->hChars[i], baf->out);
fprintf(baf->out, " %0*d\n\n", endDigits, hEndNum);
}

void bafOut(struct baf *baf, char n, char h)
/* Write a pair of character to block alignment. */
{
baf->nChars[baf->cix] = n;
baf->hChars[baf->cix] = h;
if (n != '.' && n != '-')
    baf->nCurPos += 1;
if (h != '.' && h != '-')
    baf->hCurPos += 1;
if (++(baf->cix) >= baf->lineSize)
    {
    bafWriteLine(baf);
    baf->cix = 0;
    baf->nLineStart = baf->nCurPos;
    baf->hLineStart = baf->hCurPos;
    }
}

void bafFlushLineNoHr(struct baf *baf)
/* Write out alignment line if it has any characters in it (no <HR>). */
{
if (baf->cix > 0)
    bafWriteLine(baf);
fflush(baf->out);
baf->cix = 0;
}

void bafFlushLine(struct baf *baf)
/* Write out alignment line if it has any characters in it, and an <HR>. */
{
bafFlushLineNoHr(baf);
fprintf(baf->out, "<HR ALIGN=\"CENTER\">");
}

