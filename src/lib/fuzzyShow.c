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


static void ffShNeedle(FILE *f, DNA *needle, int needleSize,
		       int needleNumOffset, char *colorFlags,
		       struct ffAli *aliList, boolean upcMatch,
		       int cdsS, int cdsE,
		       boolean accentRange, int accentStart, int accentEnd)
/* Display the needle sequence with HTML highlighting. */
{
struct cfm *cfm = cfmNew(10, 50, TRUE, FALSE, f, needleNumOffset);
char *n = cloneMem(needle, needleSize);
char *accentFlags = needMem(needleSize);
struct ffAli *leftAli = aliList;
struct ffAli *ali;
long i;

zeroBytes(colorFlags, needleSize);
zeroBytes(accentFlags, needleSize);
fprintf(f, "<PRE><TT>\n");
if (aliList != NULL)
    {
    for (leftAli = aliList; leftAli->left != NULL; leftAli = leftAli->left)
	;
    }
for (ali = leftAli; ali != NULL; ali = ali->right)
    {
    boolean utr = FALSE;
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
	if (accentRange)
	    {
	    if (off+i >= accentStart && off+i < accentEnd)
		accentFlags[off+i] = TRUE;
	    }
	}
    }
for (i=0; i<needleSize; ++i)
    {
    if (accentRange && i == accentStart)
	fprintf(f, "<A NAME=cDNAStart></A>");
    cfmOutExt(cfm, n[i], seqOutColorLookup[(int)colorFlags[i]],
	      accentFlags[i], accentFlags[i], FALSE);
    }
cfmFree(&cfm);
freeMem(n);
freeMem(accentFlags);
fprintf(f, "</TT></PRE>\n");
htmHorizontalLine(f);
}

void ffShowSideBySide(FILE *f, struct ffAli *leftAli, DNA *needle, int needleNumOffset,
		      DNA *haystack, int hayNumOffset, int haySize, int hayOffStart, int hayOffEnd,
		      int blockMaxGap, boolean rcHaystack, boolean initialNewline)
/* Print HTML side-by-side alignment of needle and haystack (no title or labels) to f.
 * {hay,needle}NumOffset are the coords at which the DNA sequence begins.
 * hayOff{Start,End} are the range of coords *relative to hayNumOffset* to which the 
 * alignment display will be clipped -- pass in {0,haySize} for no clipping. */
{
fprintf(f, "<PRE><TT>%s", initialNewline ? "\n" : "");
struct ffAli *ali, *lastAli = NULL;
struct baf baf;
/* NOTE: if rcHaystack, hayNumOffset changes here into the end, not start! */
if (rcHaystack) 
    hayNumOffset += haySize;
bafInit(&baf, needle, needleNumOffset, FALSE, 
    	haystack, hayNumOffset, rcHaystack, f, 50, FALSE);
for (ali=leftAli; ali!=NULL; ali = ali->right)
    {
    int i;
    boolean doBreak = TRUE;
    if ((ali->hEnd - haystack) <= hayOffStart ||
	(ali->hStart - haystack) >= hayOffEnd)
	continue;

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
    int offset = max(0, (hayOffStart - (ali->hStart - haystack)));
    int nStart = offset + ali->nStart - needle;
    int hStart = offset + ali->hStart - haystack;
    bafSetPos(&baf, nStart, hStart);
    if (doBreak || lastAli == NULL)
	bafStartLine(&baf);
    int aliLen = ali->nEnd - ali->nStart;
    for (i=0; i<aliLen; ++i)
	{
	int hayOff = i + (ali->hStart - haystack);
	if (hayOff < hayOffStart)
	    continue;
	if (hayOff >= hayOffEnd)
	    break;
	bafOut(&baf, ali->nStart[i], ali->hStart[i]);
	}
    lastAli = ali;
    }
if (leftAli != NULL)
    bafFlushLineNoHr(&baf);
fprintf(f, "</TT></PRE>\n");
}


int ffShAliPart(FILE *f, struct ffAli *aliList, 
    char *needleName, DNA *needle, int needleSize, int needleNumOffset,
    char *haystackName, DNA *haystack, int haySize, int hayNumOffset,
    int blockMaxGap, boolean rcNeedle, boolean rcHaystack,
    boolean showJumpTable, 
    boolean showNeedle, boolean showHaystack,
    boolean showSideBySide, boolean upcMatch,
    int cdsS, int cdsE, int hayPartS, int hayPartE)
/* Display parts of alignment on html page.  If hayPartS..hayPartE is a 
 * smaller subrange of the alignment, highlight that part of the alignment 
 * in both needle and haystack with underline & bold, and show only that 
 * part of the haystack (plus padding).  Returns number of blocks (after
 * merging blocks separated by blockMaxGap or less). */
{
long i;
struct ffAli *ali;
struct ffAli *lastAli;
struct ffAli *leftAli = aliList;
struct ffAli *rightAli = aliList;
int maxSize = (needleSize > haySize ? needleSize : haySize);
char *colorFlags = needMem(maxSize);
int anchorCount = 0;
boolean restrictToWindow = FALSE;
int hayOffStart = 0, hayOffEnd = haySize;
int hayPaddedOffStart = 0, hayPaddedOffEnd = haySize;
int hayExtremity = rcHaystack ? (hayNumOffset + haySize) : hayNumOffset;
int nPartS=0, nPartE=0;

if (aliList != NULL)
    {
    while (leftAli->left != NULL) leftAli = leftAli->left;
    while (rightAli->right != NULL) rightAli = rightAli->right;
    }

/* If we are only showing part of the alignment, translate haystack window
 * coords to needle window coords and haystack-offset window coords: */
if (hayPartS > (hayNumOffset + (leftAli->hStart - haystack)) ||
    (hayPartE > 0 && hayPartE < (hayNumOffset + (rightAli->hEnd - haystack))))
    {
    DNA *haystackPartS;
    DNA *haystackPartE;
    restrictToWindow = TRUE;
    if (rcHaystack)
	{
	haystackPartS = haystack + (haySize - (hayPartE - hayNumOffset));
	haystackPartE = haystack + (haySize - (hayPartS - hayNumOffset));
	}
    else
	{
	haystackPartS = haystack + hayPartS - hayNumOffset;
	haystackPartE = haystack + hayPartE - hayNumOffset;
	}
    boolean foundStart = FALSE;
    hayOffStart = haystackPartS - haystack;
    hayOffEnd = haystackPartE - haystack;
    for (ali = leftAli;  ali != NULL;  ali = ali->right)
	{
	if (haystackPartS < ali->hEnd && !foundStart)
	    {
	    int offset = haystackPartS - ali->hStart;
	    if (offset < 0)
		offset = 0;
	    nPartS = offset + ali->nStart - needle;
	    hayOffStart = offset + ali->hStart - haystack;
	    foundStart = TRUE;
	    }
	if (haystackPartE > ali->hStart)
	    {
	    if (haystackPartE > ali->hEnd)
		{
		nPartE = ali->nEnd - needle;
		hayOffEnd = ali->hEnd - haystack;
		}
	    else
		{
		nPartE = haystackPartE - ali->hStart + ali->nStart - needle;
		hayOffEnd = haystackPartE - haystack;
		}
	    }
	}
    hayPaddedOffStart = max(0, (hayOffStart - 100));
    hayPaddedOffEnd = min(haySize, (hayOffEnd + 100));
    if (rcHaystack)
	hayExtremity = hayNumOffset + haySize - hayPaddedOffStart;
    else
	hayExtremity = hayNumOffset + hayPaddedOffStart;
    }

if (showJumpTable)
    {
    fputs("<CENTER><P><TABLE BORDER=1 WIDTH=\"97%\"><TR>", f);
    fputs("<TD WIDTH=\"23%\"><P ALIGN=CENTER><A HREF=\"#cDNA\">cDNA Sequence</A></TD>", f);
    if (restrictToWindow)
	fputs("<TD WIDTH=\"23%\"><P ALIGN=CENTER><A HREF=\"#cDNAStart\">cDNA Sequence in window</A></TD>", f);
    fputs("<TD WIDTH=\"27%\"><P ALIGN=\"CENTER\"><A HREF=\"#genomic\">Genomic Sequence</A></TD>", f);
    fputs("<TD WIDTH=\"29%\"><P ALIGN=\"CENTER\"><A HREF=\"#1\">cDNA in Genomic</A></TD>", f);
    fputs("<TD WIDTH=\"21%\"><P ALIGN=\"CENTER\"><A HREF=\"#ali\">Side by Side</A></TD>", f);
    fputs("</TR></TABLE>\n", f);
    }
if (cdsE > 0) 
    {
    fprintf(f, "Matching bases in coding regions of cDNA and genomic sequences are colored blue%s. ", 
	    (upcMatch ? " and capitalized" : ""));
    fprintf(f, "Matching bases in UTR regions of cDNA and genomic sequences are colored red%s. ", 
	    (upcMatch ? " and capitalized" : ""));
    fputs("Light blue (coding) or orange (UTR) bases mark the boundaries of gaps in either sequence "
	  "(often splice sites).\n", f);
    } 
else 
    {
    fprintf(f, "Matching bases in cDNA and genomic sequences are colored blue%s. ", 
	    (upcMatch ? " and capitalized" : ""));
    fputs("Light blue bases mark the boundaries of gaps in either sequence "
	  "(often splice sites).\n", f);
    } 
if (showNeedle && restrictToWindow)
    fputs("Bases that were in the selected browser region are shown in bold "
	  "and underlined, "
	  "and only the alignment for these bases is displayed in the "
	  "Genomic and Side by Side sections.\n", f);

if (showJumpTable)
    fputs("</P></CENTER>\n", f);
htmHorizontalLine(f);

fprintf(f, "<H4><A NAME=cDNA></A>cDNA %s%s</H4>\n", needleName, (rcNeedle ? " (reverse complemented)" : ""));

if (rcNeedle)
    reverseComplement(needle, needleSize);

if (showNeedle)
    {
    ffShNeedle(f, needle, needleSize, needleNumOffset, colorFlags,
	       aliList, upcMatch, cdsS, cdsE,
	       restrictToWindow, nPartS, nPartE);
    }

if (showHaystack)
    {
    struct cfm *cfm = cfmNew(10, 50, TRUE, rcHaystack, f, hayExtremity);
    char *h = cloneMem(haystack, haySize);
    char *accentFlags = needMem(haySize);
    zeroBytes(accentFlags, haySize);
    fprintf(f, "<H4><A NAME=genomic></A>Genomic %s %s:</H4>\n", 
    	haystackName,
	(rcHaystack ? "(reverse strand)" : ""));
    fprintf(f, "<PRE><TT>\n");
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
	    if (restrictToWindow && off+i >= hayOffStart && off+i < hayOffEnd)
		accentFlags[off+i] = TRUE;
	    }
	}
    ali = leftAli;
    lastAli = NULL;
    while (ali && (ali->hEnd - haystack) <= hayPaddedOffStart)
	ali = ali->right;
    for (i = hayPaddedOffStart; i < hayPaddedOffEnd; ++i)
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
	cfmOutExt(cfm, h[i], seqOutColorLookup[(int)colorFlags[i]],
		  accentFlags[i], accentFlags[i], FALSE);
	}
    cfmFree(&cfm);
    freeMem(h);
    fprintf(f, "</TT></PRE>\n");
    htmHorizontalLine(f);
    }

if (showSideBySide)
    {
    fprintf(f, "<H4><A NAME=ali></A>Side by Side Alignment</H4>\n");
    ffShowSideBySide(f, leftAli, needle, needleNumOffset, haystack, hayNumOffset, haySize,
		     hayOffStart, hayOffEnd, blockMaxGap, rcHaystack, TRUE);
    fprintf(f, "<HR ALIGN=\"CENTER\">");
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
    TRUE, TRUE, TRUE, TRUE, FALSE, 0, 0, 0, 0);
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
