/* correlate - do correlation page. */

#include "common.h"
#include "web.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "chromGraph.h"
#include "correlate.h"
#include "hPrint.h"
#include "hgGenome.h"

static void correlateChrom(struct chromGraphBin *a, struct chromGraphBin *b,
	struct correlate *c)
/* Add a sample point to correlation for each data point in each graph.
 * The value for the other graph will be linearly interpolated. 
 * In most cases the very first and very last data points won't be
 * included in the correlation since the interpolation there isn't
 * generally possible. Both chromGraphBins should be positioned at the
 * start of the same chromosome. */
{
int aStart, bStart, aLastStart = 0, bLastStart = 0;
double aLastVal = 0, bLastVal = 0;
boolean gotA = FALSE, gotB = FALSE;

chromGraphBinNextVal(a);
chromGraphBinNextVal(b);
for (;;)
    {
    aStart = a->chromStart;
    bStart = b->chromStart;
    if (aStart == bStart)
        {
	/* Correlate twice since matching both points. */
        correlateNext(c, a->val, b->val);
        correlateNext(c, a->val, b->val);
	aLastStart = aStart;
	aLastVal = a->val;
	bLastStart = bStart;
	bLastVal = b->val;
	gotA = gotB = TRUE;
	if (!chromGraphBinNextVal(a))
	    break;
	if (!chromGraphBinNextVal(b))
	    break;
	}
    else if (aStart < bStart)
        {
	if (gotB)
	    {
	    double ratio = (aStart - bLastStart)/(bStart - bLastStart);
	    double bInterVal = bLastVal + ratio * (b->val - bLastVal);
	    correlateNext(c, a->val, bInterVal);
	    }
	aLastStart = aStart;
	aLastVal = a->val;
	gotA = TRUE;
	if (!chromGraphBinNextVal(a))
	    break;
	}
    else
        {
	if (gotA)
	    {
	    double ratio = (bStart - aLastStart)/(aStart - aLastStart);
	    double aInterVal = aLastVal + ratio * (a->val - aLastVal);
	    correlateNext(c, aInterVal, b->val);
	    }
	bLastStart = bStart;
	bLastVal = b->val;
	gotB = TRUE;
	if (!chromGraphBinNextVal(b))
	    break;
	}
    }
}

double chromGraphBinCorrelate(char *aFile, char *bFile)
/* Do correlation between two graphs.  */
{
struct chromGraphBin *a = chromGraphBinOpen(aFile);
struct chromGraphBin *b = chromGraphBinOpen(bFile);
struct cgbChrom *chrom;
struct correlate *c = correlateNew();
double r;

for (chrom = a->chromList; chrom != NULL; chrom = chrom->next)
    {
    chromGraphBinSeekToChrom(a, chrom->name);
    if (chromGraphBinSeekToChrom(b, chrom->name))
        {
	correlateChrom(a, b, c);
	}
    }
r = correlateResult(c);
correlateFree(&c);
return r;
}

void correlateGraphs(struct genoGraph *aGg, struct genoGraph *bGg)
/* Do correlation between two graphs */
{
double r = chromGraphBinCorrelate(aGg->binFileName, bGg->binFileName);

webPrintLinkCell(aGg->shortLabel);
webPrintLinkCell(bGg->shortLabel);
webPrintLinkCellStart();
hPrintf("%f", r);
webPrintLinkCellEnd();
webPrintLinkCellStart();
hPrintf("%f", r*r);
webPrintLinkCellEnd();
}

void correlatePage(struct sqlConnection *conn)
/* Put up correlation page. */
{
cartWebStart(cart, database, "Correlations of all pairs of graphs");
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" METHOD=GET>\n");
cartSaveSession(cart);
struct slRef *ggRefList = ggAllVisible(conn);
struct slRef *aRef, *bRef;
hPrintf("<TABLE><TR><TD>");
webPrintLinkTableStart();
webPrintLabelCell("Graph A");
webPrintLabelCell("Graph B");
webPrintLabelCell("R");
webPrintLabelCell("R-Squared");
for (aRef = ggRefList; aRef != NULL; aRef = aRef->next)
    {
    for (bRef = aRef->next; bRef != NULL; bRef = bRef->next)
        {
	hPrintf("</TR><TR>");
	correlateGraphs(aRef->val, bRef->val);
	}
    }
webPrintLinkTableEnd();
hPrintf("</TD><TD>");
hPrintf("<CENTER>");
cgiMakeButton("submit", "return to graphs");
hPrintf("</CENTER>");
hPrintf("</TD></TR></TABLE>");

hPrintf("<P>R, also known as Pearson's correlation coefficient, is a measure ");
hPrintf("of the extent that two graphs move together.  The value of R ranges ");
hPrintf("between -1 and 1.  A positive R indicates that the graphs tend to ");
hPrintf("move in the same direction, while a negative R indicates that they ");
hPrintf("tend to move in opposite directions.</P>\n");
hPrintf("<P>R-Squared (which is indeed just R*R) measures how much of the ");
hPrintf("variation in one graph can be explained by a linear dependence on ");
hPrintf("the other graph. R-Squared ranges between 0 when the two graphs are ");
hPrintf("independent to 1 when the graphs are completely dependent.</P>");
hPrintf("</FORM>");
cartWebEnd();
}
