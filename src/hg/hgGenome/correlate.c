/* correlate - do correlation page. */

#include "common.h"
#include "web.h"
#include "jksql.h"
#include "chromGraph.h"
#include "correlate.h"
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
    uglyf("Procesing %s<BR>\n", chrom->name);
    chromGraphBinSeekToChrom(a, chrom->name);
    if (chromGraphBinSeekToChrom(b, chrom->name))
        {
	correlateChrom(a, b, c);
	}
    }
uglyf("N is %d<BR>\n", c->n);
r = correlateResult(c);
correlateFree(&c);
return r;
}

void correlateGraphs(struct genoGraph *aGg, struct genoGraph *bGg)
/* Do correlation between two graphs */
{
double r = chromGraphBinCorrelate(aGg->binFileName, bGg->binFileName);
hPrintf("<TR>");
hPrintf("<TD>%s</TD>", aGg->shortLabel);
hPrintf("<TD>%s</TD>", bGg->shortLabel);
hPrintf("<TD>%f</TD>", r);
hPrintf("<TD>%f</TD>", r*r);
hPrintf("</TR>");
}

void correlatePage(struct sqlConnection *conn)
/* Put up correlation page. */
{
cartWebStart(cart, "Theoretically correlating, please go back");
getGenoGraphs(conn);
struct slRef *ggRefList = ggAllVisible(conn);
struct slRef *aRef, *bRef;
hPrintf("<TABLE>\n");
hPrintf("<TR><TH>Graph A</TH><TH>Graph B</TH><TH>R</TH><TH>R-squared</TH></TR>");
for (aRef = ggRefList; aRef != NULL; aRef = aRef->next)
    {
    for (bRef = aRef->next; bRef != NULL; bRef = bRef->next)
        {
	correlateGraphs(aRef->val, bRef->val);
	}
    }
hPrintf("</TABLE>\n");
cartWebEnd();
}
