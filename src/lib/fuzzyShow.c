/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
#include "common.h"
#include "dnautil.h"
#include "memgfx.h"
#include "fuzzyFind.h"
#include "htmshell.h"
#include "cda.h"
#include "seqOut.h"

int ffShAliPart(FILE *f, struct ffAli *aliList, 
    char *needleName, DNA *needle, int needleSize, int needleNumOffset,
    char *haystackName, DNA *haystack, int haySize, int hayNumOffset,
    int blockMaxGap, boolean rcNeedle, boolean rcHaystack,
    boolean showJumpTable, 
    boolean showNeedle, boolean showHaystack,
    boolean showSideBySide, boolean upcMatch)
/* Display parts of allignment on html page.  Returns number of blocks (after
 * merging blocks separated by blockMaxGap or less). */
{
long i;
struct ffAli *ali;
struct ffAli *lastAli;
struct ffAli *leftAli = aliList;
int startMatchIx;
int charsInLine;
struct baf baf;
int maxSize = (needleSize > haySize ? needleSize : haySize);
char *colorFlags = needMem(maxSize);
int anchorCount = 0;

if (showJumpTable)
    {
    fputs("<CENTER><P><TABLE BORDER=1 WIDTH=\"97%\"><TR>", f);
    fputs("<TD WIDTH=\"23%\"><P ALIGN=CENTER><A HREF=\"#cDNA\">cDNA Sequence</A></TD>", f);
    fputs("<TD WIDTH=\"27%\"><P ALIGN=\"CENTER\"><A HREF=\"#genomic\">Genomic Sequence</A></TD>", f);
    fputs("<TD WIDTH=\"29%\"><P ALIGN=\"CENTER\"><A HREF=\"#1\">cDNA in Genomic</A></TD>", f);
    fputs("<TD WIDTH=\"21%\"><P ALIGN=\"CENTER\"><A HREF=\"#ali\">Side by Side</A></TD>", f);
    fputs("</TR></TABLE>", f);
    }
fprintf(f, "Matching bases in cDNA and genomic sequences are colored blue%s. ", 
	(upcMatch ? " and capitalized" : ""));
fputs("Light blue bases mark the boundaries of gaps in either side of "
      "the alignment (often splice sites). ", f);
if (showJumpTable)
    fputs("</P></CENTER>", f);
htmHorizontalLine(f);

fprintf(f, "<H4><A NAME=cDNA></A>cDNA %s%s</H4>\n", needleName, (rcNeedle ? " (reverse complemented)" : ""));

if (rcHaystack) 
    hayNumOffset += haySize;

if (aliList == NULL)
    leftAli = NULL;
else
    {
    while (leftAli->left != NULL) leftAli = leftAli->left;
    }
if (rcNeedle)
    reverseComplement(needle, needleSize);

if (showNeedle)
    {
    struct cfm *cfm = cfmNew(10, 50, TRUE, FALSE, f, needleNumOffset);
    char *n = cloneMem(needle, needleSize);
    zeroBytes(colorFlags, needleSize);
    fprintf(f, "<TT><PRE>\n");
    for (ali = leftAli; ali != NULL; ali = ali->right)
	{
	int i;
	int off = ali->nStart-needle;
	int count = ali->nEnd - ali->nStart;
	for (i=0; i<count; ++i)
	    {
	    if (toupper(ali->hStart[i]) == toupper(ali->nStart[i]))
		{
		colorFlags[off+i] = ((i == 0 || i == count-1) ? socBrightBlue : socBlue);
		if (upcMatch)
		    n[off+i] = toupper(n[off+i]);
		}
	    }
	}
    for (i=0; i<needleSize; ++i)
	{
	cfmOut(cfm, n[i], seqOutColorLookup[colorFlags[i]]);
	}
    cfmFree(&cfm);
    freeMem(n);
    fprintf(f, "</TT></PRE>\n");
    htmHorizontalLine(f);
    }

if (showHaystack)
    {
    struct cfm *cfm = cfmNew(10, 50, TRUE, rcHaystack, f, hayNumOffset);
    char *h = cloneMem(haystack, haySize);
    fprintf(f, "<H4><A NAME=genomic></A>Genomic %s %s:</H4>\n", 
    	haystackName,
	(rcHaystack ? "(reverse strand)" : ""));
    fprintf(f, "<TT><PRE>\n");
    zeroBytes(colorFlags, haySize);
    for (ali = leftAli; ali != NULL; ali = ali->right)
	{
	int i;
	int off = ali->hStart-haystack;
	int count = ali->hEnd - ali->hStart;
	for (i=0; i<count; ++i)
	    {
	    if (toupper(ali->hStart[i]) == toupper(ali->nStart[i]))
		{
		colorFlags[off+i] = ((i == 0 || i == count-1) ? socBrightBlue : socBlue);
		if (upcMatch)
		    h[off+i] = toupper(h[off+i]);
		}
	    }
	}
    if (leftAli != NULL)
	startMatchIx = leftAli->hStart - haystack;
    else
	startMatchIx = 0;
    ali = leftAli;
    lastAli = NULL;
    for (i=0; i<haySize; ++i)
	{
	/* Put down "anchor" on first match position in haystack
	 * so user can hop here with a click on the needle. */
	if (ali != NULL &&  i == ali->hStart - haystack)
	    {
	    if (lastAli == NULL || ali->hStart - lastAli->hEnd > blockMaxGap)
		{
		fprintf(f, "<A NAME=%d></A>", ++anchorCount);
		}
	    lastAli = ali;
	    ali = ali->right;
	    }
	cfmOut(cfm, h[i], seqOutColorLookup[colorFlags[i]]);
	}
    cfmFree(&cfm);
    freeMem(h);
    fprintf(f, "</TT></PRE>\n");
    htmHorizontalLine(f);
    }

if (showSideBySide)
    {
    fprintf(f, "<H4><A NAME=ali></A>Side by Side Alignment</H4>\n");
    fprintf(f, "<TT><PRE>\n");
    lastAli = NULL;
    charsInLine = 0;
    bafInit(&baf, needle, 0, FALSE, 
    	haystack, hayNumOffset, rcHaystack, f, 50, FALSE);
    for (ali=leftAli; ali!=NULL; ali = ali->right)
	{
	boolean doBreak = TRUE;
	int aliLen;
	int i;

	/* Decide whether to put in a line break and/or blank characters */
	if (lastAli != NULL)
	    {
	    int nSkip = ali->nStart - lastAli->nEnd;
	    int hSkip = ali->hStart - lastAli->hEnd;
	    if (nSkip >= 0 && hSkip >= 0 && nSkip <= blockMaxGap && hSkip <= blockMaxGap)
		{
		if (nSkip > 0 && hSkip == 0)
		    {
		    for (i=0; i<nSkip; ++i)
			bafOut(&baf, lastAli->nEnd[i],'.');
		    doBreak = FALSE; 
		    }
		if (hSkip > 0 && nSkip == 0)
		    {
		    for (i=0; i<hSkip; ++i)
			bafOut(&baf, '.', lastAli->hEnd[i]);
		    doBreak = FALSE;
		    }
		if (hSkip == nSkip)
		    {
		    for (i=0; i<hSkip; ++i)
			bafOut(&baf, lastAli->nEnd[i], lastAli->hEnd[i]);
		    doBreak = FALSE;
		    }
		}
	    }
	else
	    {
	    doBreak = FALSE;
	    }
	if (doBreak)
	    bafFlushLine(&baf);
	bafSetAli(&baf, ali);
	if (doBreak || lastAli == NULL)
	    bafStartLine(&baf);
	aliLen = ali->nEnd - ali->nStart;
	for (i=0; i<aliLen; ++i)
	    {
	    bafOut(&baf, ali->nStart[i], ali->hStart[i]);
	    }
	lastAli = ali;
	}
    if (leftAli != NULL)
	bafFlushLine(&baf);
    fprintf(f, "</TT></PRE>\n");
    }
if (rcNeedle)
    reverseComplement(needle, needleSize);
return anchorCount;
}

int ffShAli(FILE *f, struct ffAli *aliList, 
    char *needleName, DNA *needle, int needleSize, int needleNumOffset,
    char *haystackName, DNA *haystack, int haySize, int hayNumOffset,
    int blockMaxGap,
    boolean rcNeedle)
/* Display allignment on html page.  Returns number of blocks (after
 * merging blocks separated by blockMaxGap or less). */
{
return ffShAliPart(f, aliList, needleName, needle, needleSize, needleNumOffset,
    haystackName, haystack, haySize, hayNumOffset, blockMaxGap, rcNeedle, FALSE,
    TRUE, TRUE, TRUE, TRUE, FALSE);
}

void ffShowAli(struct ffAli *aliList, char *needleName, DNA *needle, int needleNumOffset,
    char *haystackName, DNA *haystack, int hayNumOffset, boolean rcNeedle)
/* Display allignment on html page. */
{
ffShAli(stdout, aliList, needleName, needle, strlen(needle), needleNumOffset,
    haystackName, haystack, strlen(haystack), hayNumOffset, 8, rcNeedle);
}

static struct cdaAli *makeBlocks(struct ffAli *aliList, 
    DNA *needle, int needleSize, DNA *hay, int haySize, boolean isRc)
/* Merge together blocks separated only by noise, and evaluate
 * left, right, and middle of block for alignment strength. */
{
struct cdaAli *ca = cdaAliFromFfAli(aliList, 
    needle, needleSize, hay, haySize, isRc);
cdaCoalesceBlocks(ca);
return ca;
}

