/* seqOut - stuff to output sequences and alignments in web 
 * or ascii viewable form. */

#include "common.h"
#include "obscure.h"
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
fprintf(cfm->out, "<FONT COLOR=\"#00%X\">", 0);
}

void cfmOut(struct cfm *cfm, char c, int color)
/* Write out a byte, and depending on color formatting extras  */
{
if (color != cfm->lastColor)
    {
    fprintf(cfm->out, "</FONT><FONT COLOR=\"#00%X\">", color);
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
	        pos = -pos;
	    pos += cfm->numOff;
	    fprintf(cfm->out, " %ld", pos);
	    }
	fprintf(cfm->out, "<BR>\n");
	cfm->inLine = 0;
	}
    }
}

void cfmCleanup(struct cfm *cfm)
/* Finish up cfm formatting job. */
{
fprintf(cfm->out, "</FONT>\n");
}

void bafInit(struct baf *baf, DNA *needle, int nNumOff, DNA *haystack, int hNumOff, boolean hCountDown, FILE *out)
/* Initialize block alignment formatter. */
{
baf->cix = 0;
baf->needle = needle;
baf->haystack = haystack;
baf->nNumOff = nNumOff;
baf->hNumOff = hNumOff;
baf->hCountDown = hCountDown;
baf->out = out;
}

void bafSetAli(struct baf *baf, struct ffAli *ali)
/* Set up block formatter around an ffAli block. */
{
baf->nCurPos = ali->nStart - baf->needle;
baf->hCurPos = ali->hStart - baf->haystack;
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
int startDigits = maxDigits(baf->nLineStart+1+baf->nNumOff, baf->hLineStart+1+baf->hNumOff);
int endDigits = maxDigits(baf->nCurPos+baf->nNumOff, baf->hCurPos+baf->hNumOff);
int hStartNum, hEndNum;

fprintf(baf->out, "%0*d ", startDigits, 1+baf->nNumOff + baf->nLineStart);
for (i=0; i<count; ++i)
    fputc(baf->nChars[i], baf->out);
fprintf(baf->out, " %0*d<BR>\n", endDigits, baf->nNumOff + baf->nCurPos);

for (i=0; i<startDigits; ++i)
    fputc('>', baf->out);
fputc(' ', baf->out);
for (i=0; i<count; ++i)
    fputc( (toupper(baf->nChars[i]) == toupper(baf->hChars[i]) ? '|' : '.'), baf->out);
fputc(' ', baf->out);
for (i=0; i<endDigits; ++i)
    fputc('<', baf->out);
fprintf(baf->out, "<BR>\n");

if (baf->hCountDown)
    {
    hStartNum = 1+baf->hNumOff - baf->hLineStart;
    hEndNum = baf->hNumOff - baf->hCurPos;
    }
else
    {
    hStartNum = 1+baf->hNumOff + baf->hLineStart;
    hEndNum = baf->hNumOff + baf->hCurPos;
    }
fprintf(baf->out, "%0*d ", startDigits, hStartNum);
for (i=0; i<count; ++i)
    fputc(baf->hChars[i], baf->out);
fprintf(baf->out, " %0*d<BR>\n<BR>\n", endDigits, hEndNum);
}

void bafOut(struct baf *baf, char n, char h)
/* Write a pair of character to block alignment. */
{
baf->nChars[baf->cix] = n;
baf->hChars[baf->cix] = h;
baf->nCurPos += 1;
baf->hCurPos += 1;
if (++(baf->cix) >= sizeof(baf->nChars))
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

