/* seqOut - stuff to output sequences and alignments in web 
 * or ascii viewable form. */

#include "common.h"
#include "obscure.h"
#include "dnautil.h"
#include "fuzzyFind.h"
#include "seqOut.h"
#include "htmshell.h"

void cfmInit(struct cfm *cfm, int wordLen, int lineLen, 
	boolean lineNumbers, boolean countDown, FILE *out, int numOff)
/* Set up colored sequence formatting for html. */
{
cfm->inWord = cfm->inLine = cfm->charCount = 0;
cfm->lastColor = 0;
cfm->wordLen = wordLen;
cfm->lineLen = lineLen;
cfm->lineNumbers = lineNumbers;
cfm->countDown = countDown;
cfm->out = out;
cfm->numOff = numOff;
cfm->bold = cfm->underline = cfm->italic = FALSE;
fprintf(cfm->out, "<FONT COLOR=\"#%06X\">", 0);
}

void cfmOutExt(struct cfm *cfm, char c, int color, boolean underline, boolean bold, boolean italic)
/* Write out a byte, and formatting extras  */
{
if (color != cfm->lastColor)
    {
    fprintf(cfm->out, "</FONT>");
    }
if (underline != cfm->underline)
    {
    if (!underline)
       fprintf(cfm->out, "</U>");
    }
if (italic != cfm->italic)
    {
    if (!italic)
       fprintf(cfm->out, "</I>");
    }
if (bold != cfm->bold)
    {
    if (!bold)
       fprintf(cfm->out, "</B>");
    }

if (bold != cfm->bold)
    {
    if (bold)
       fprintf(cfm->out, "<B>");
    cfm->bold = bold;
    }
if (italic != cfm->italic)
    {
    if (italic)
       fprintf(cfm->out, "<I>");
    cfm->italic = italic;
    }
if (underline != cfm->underline)
    {
    if (underline)
       fprintf(cfm->out, "<U>");
    cfm->underline = underline;
    }
if (color != cfm->lastColor)
    {
    fprintf(cfm->out, "<FONT COLOR=\"#%06X\">", color);
    cfm->lastColor = color;
    }


++cfm->charCount;
fputc(c, cfm->out);
if (cfm->wordLen)
    {
    if (++cfm->inWord >= cfm->wordLen)
	{
	fputc(' ', cfm->out);
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
	    fprintf(cfm->out, " %d", pos);
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

void cfmCleanup(struct cfm *cfm)
/* Finish up cfm formatting job. */
{
fprintf(cfm->out, "</FONT>\n");
}

int seqOutColorLookup[3] = 
    {
    0x000000,
    0x0033FF,
    0xaa22FF,
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
int div3 = (baf->isTrans ? 3 : 1);
int nStart = baf->nLineStart/div3 + 1 + baf->nNumOff;
int hStart = baf->hLineStart + 1 + baf->hNumOff;
int nEnd = baf->nCurPos/div3 + baf->nNumOff;
int hEnd = baf->hCurPos + baf->hNumOff;
int startDigits = maxDigits(nStart, hStart);
int endDigits = maxDigits(nEnd, hEnd);
int hStartNum, hEndNum;
int nStartNum, nEndNum;

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
    fputc('>', baf->out);
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
	    if (toupper(n) == lookupCodon(codon))
	        c = '|';
	    }
	}
    else 
        {
	if (toupper(n) == toupper(h))
	     c = '|';
	}
    fputc(c, baf->out);
    }
fputc(' ', baf->out);
for (i=0; i<endDigits; ++i)
    fputc('<', baf->out);
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
baf->nCurPos += 1;
baf->hCurPos += 1;
if (++(baf->cix) >= baf->lineSize)
    {
    bafWriteLine(baf);
    baf->cix = 0;
    baf->nLineStart = baf->nCurPos;
    baf->hLineStart = baf->hCurPos;
    }
}

void bafFlushLine(struct baf *baf)
/* Write out alignment line if it has any characters in it. */
{
bafWriteLine(baf);
htmHorizontalLine(baf->out);
baf->cix = 0;
}

