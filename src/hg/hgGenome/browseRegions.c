#include "common.h"
#include "hash.h"
#include "bed.h"
#include "cart.h"
#include "web.h"
#include "chromGraph.h"
#include "hgGenome.h"

struct bed3 *bed3New(char *chrom, int start, int end)
/* Make new bed3. */
{
struct bed3 *bed;
AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = start;
bed->chromEnd = end;
return bed;
}

struct bed3 *chromGraphBinToBed3(char *fileName, boolean threshold)
/* Stream through making list of all places above threshold. */
{
struct bed3 *list = NULL, *bed = NULL;
struct chromGraphBin *cgb = chromGraphBinOpen(fileName);
double val, lastVal = 0;
int start, lastStart = 0;
while (chromGraphBinNextChrom(cgb))
    {
    /* Set up bed around first point in chromosome. */
    char *chrom = cgb->chrom;
    bed = NULL;
    if (!chromGraphBinNextVal(cgb))
        continue;
    lastVal = val = cgb->val;
    lastStart = start = cgb->chromStart;
    if (val >= threshold)
	{
	bed = bed3New(chrom, start, start+1);
	slAddHead(&list, bed);
	}

    /* Loop for next points, extending bed when get transition out, 
     * creating bed when getting transition in. */
    while (chromGraphBinNextVal(cgb))
        {
	val = cgb->val;
	start = cgb->chromStart;

	if (lastVal >= threshold && val < threshold)
	    {
	    double ratio = (lastVal - threshold)/(lastVal - val);
	    assert(bed != NULL);
	    bed->chromEnd = lastStart + (start-lastStart)*ratio;
	    bed = NULL;
	    }
	else if (lastVal < threshold && val >= threshold)
	    {
	    double ratio = (threshold - lastVal)/(val - lastVal);
	    int cross = lastStart + (start - lastStart)*ratio;
	    bed = bed3New(chrom, cross, cross+1);
	    slAddHead(&list, bed);
	    }

	lastVal = val;
	lastStart = start;
	}
    }
slReverse(&list);
return list;
}

void browseRegions(struct sqlConnection *conn)
/* Put up a frame with a list of links on the left and the
 * first link selected on the right. */
{
struct genoGraph *gg;
cartWebStart(cart, "Theoretically browsing, please go back");
getGenoGraphs(conn);
gg = ggFirstVisible();
if (gg != NULL)
    {
    double threshold = cartUsualDouble(cart, hggThreshold, defaultThreshold);
    struct bed3 *bed, *bedList;
    uglyf("Trying out %s - %s<BR>\n", gg->name, gg->longLabel);
    uglyf("Threshold %f<BR>\n", threshold);
    bedList = chromGraphBinToBed3(gg->binFileName, threshold);
    uglyf("Got %d beds<BR>\n", slCount(bedList));
    for (bed = bedList; bed != NULL; bed = bed->next)
        {
	hPrintf("<A HREF=\"../cgi-bin/hgTracks?db=%s&position=%s:%d-%d\">",
	    database, bed->chrom, bed->chromStart+1, bed->chromEnd);
	hPrintf("%s:%d-%d</A><BR>\n", bed->chrom, bed->chromStart+1, bed->chromEnd);
	}
    }
cartWebEnd();
}
