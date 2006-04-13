/* fuzzyShow - routines to show ffAli alignments in text
 * or html. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dnautil.h"
#include "memgfx.h"
#include "fuzzyFind.h"
#include "htmshell.h"
#include "cda.h"
#include "seqOut.h"

static char const rcsid[] = "$Id: fuzzyShow.c,v 1.18 2006/04/10 17:37:07 angie Exp $";

int ffShAliPart(FILE *f, struct ffAli *aliList, 
    char *needleName, DNA *needle, int needleSize, int needleNumOffset,
    char *haystackName, DNA *haystack, int haySize, int hayNumOffset,
    int blockMaxGap, boolean rcNeedle, boolean rcHaystack,
    boolean showJumpTable, 
    boolean showNeedle, boolean showHaystack,
    boolean showSideBySide, boolean upcMatch,
    int cdsS, int cdsE)
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
if (cdsE > 0) 
    {
    fprintf(f, "Matching bases in coding regions of cDNA and genomic sequences are colored blue%s. ", 
	    (upcMatch ? " and capitalized" : ""));
    fprintf(f, "Matching bases in UTR regions of cDNA and genomic sequences are colored red%s. ", 
	    (upcMatch ? " and capitalized" : ""));
    fputs("Light blue (coding) or orange (UTR) bases mark the boundaries of gaps in either sequence "
	  "(often splice sites). ", f);
    } 
else 
    {
    fprintf(f, "Matching bases in cDNA and genomic sequences are colored blue%s. ", 
	    (upcMatch ? " and capitalized" : ""));
    fputs("Light blue bases mark the boundaries of gaps in either sequence "
	  "(often splice sites). ", f);
    } 
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
	boolean utr = FALSE;
	int i;
	int off = ali->nStart-needle;
	int count = ali->nEnd - ali->nStart;
	if ((cdsE > 0) && ((cdsS-off-1) > 0)) 
	    utr = TRUE;
	for (i=0; i<count; ++i)
	    {
	    if (!utr && (i > (cdsE-off-1)) && (cdsE > 0))
		utr = TRUE;
	    if (utr && (i == (cdsS-off)))
		utr = FALSE;
	    if (toupper(ali->hStart[i]) == toupper(ali->nStart[i]))
		{
		if (utr)
		    colorFlags[off+i] = ((i == 0 || i == count-1) ? socOrange : socRed);
		else
		    colorFlags[off+i] = ((i == 0 || i == count-1) ? socBrightBlue : socBlue);
		if (upcMatch)
		    n[off+i] = toupper(n[off+i]);
		}
	    }
	}
    for (i=0; i<needleSize; ++i)
	{
	cfmOut(cfm, n[i], seqOutColorLookup[(int)colorFlags[i]]);
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
	boolean utr = FALSE;
	int i;
	int off = ali->hStart-haystack;
	int count = ali->hEnd - ali->hStart;
	int offn = ali->nStart-needle;
	if ((cdsE > 0) && ((cdsS-offn-1) > 0)) 
	    utr = TRUE;
	for (i=0; i<count; ++i)
	    {
	    if (!utr && (i > (cdsE-offn-1)) && (cdsE > 0))
		utr = TRUE;
	    if (utr && (i == (cdsS-offn)))
		utr = FALSE;
	    if (toupper(ali->hStart[i]) == toupper(ali->nStart[i]))
		{
		if (utr)
		    colorFlags[off+i] = ((i == 0 || i == count-1) ? socOrange : socRed);
		else
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
	cfmOut(cfm, h[i], seqOutColorLookup[(int)colorFlags[i]]);
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
    bafInit(&baf, needle, needleNumOffset, FALSE, 
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
	    if (nSkip > 0 && nSkip <= blockMaxGap && hSkip == 0)
		{
		for (i=0; i<nSkip; ++i)
		    bafOut(&baf, lastAli->nEnd[i],'.');
		doBreak = FALSE; 
		}
	    else if (hSkip > 0 && hSkip <= blockMaxGap && nSkip == 0)
		{
		for (i=0; i<hSkip; ++i)
		    bafOut(&baf, '.', lastAli->hEnd[i]);
		doBreak = FALSE;
		}
	    else if (hSkip == nSkip && hSkip <= blockMaxGap)
		{
		for (i=0; i<hSkip; ++i)
		    bafOut(&baf, lastAli->nEnd[i], lastAli->hEnd[i]);
		doBreak = FALSE;
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
    fprintf(f, "<EM>*Aligned Blocks with gaps &lt;= %d bases are merged for "
	    "this display when only one sequence has a gap, or when gaps in "
	    "both sequences are of the same size.</EM>\n", blockMaxGap);
    }
if (rcNeedle)
    reverseComplement(needle, needleSize);
return anchorCount;
}

int ffShAli(FILE *f, struct ffAli *aliList, 
    char *needleName, DNA *needle, int needleSize, int needleNumOffset,
    char *haystackName, DNA *haystack, int haySize, int hayNumOffset,
    int blockMaxGap, boolean rcNeedle)
/* Display allignment on html page.  Returns number of blocks (after
 * merging blocks separated by blockMaxGap or less). */
{
return ffShAliPart(f, aliList, needleName, needle, needleSize, needleNumOffset,
    haystackName, haystack, haySize, hayNumOffset, blockMaxGap, rcNeedle, FALSE,
    TRUE, TRUE, TRUE, TRUE, FALSE,0,0);
}

void ffShowAli(struct ffAli *aliList, char *needleName, DNA *needle, int needleNumOffset,
    char *haystackName, DNA *haystack, int hayNumOffset, boolean rcNeedle)
/* Display allignment on html page. */
{
ffShAli(stdout, aliList, needleName, needle, strlen(needle), needleNumOffset,
    haystackName, haystack, strlen(haystack), hayNumOffset, 8, rcNeedle);
}
#if 0 /* not used */
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
#endif
