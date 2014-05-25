/* Stuff to handle chromGraph tracks, especially custom ones. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "trackDb.h"
#include "chromGraph.h"
#include "customTrack.h"
#include "hgTables.h"

boolean isChromGraph(struct trackDb *track)
/* Return TRUE if it's a chromGraph track */
{
if (track && track->type)
    return startsWithWord("chromGraph", track->type);
else
    return FALSE;
}

void doOutChromGraphDataCt(struct trackDb *track, char *table)
/* Print out data points in region for chromGraph custom track. */
{
char *fileName = trackDbRequiredSetting(track, "binaryFile");
struct chromGraphBin *cgb = chromGraphBinOpen(fileName);
struct region *regionList = getRegions();
struct region *region;

textOpen();
for (region = regionList; region != NULL; region = region->next)
    {
    if (chromGraphBinSeekToChrom(cgb, region->chrom))
        {
	while (chromGraphBinNextVal(cgb))
	    {
	    int pos = cgb->chromStart;
	    if (pos >= region->end)
	        break;
	    if (pos >= region->start)
	        {
		hPrintf("%s\t%d\t%g\n", region->chrom, pos, cgb->val);
		}
	    }
	}
    }

chromGraphBinFree(&cgb);
}

static void doSummaryStatsChromGraphCt()
/* Put up page showing summary stats for chromGraph custom track. */
{
struct region *regionList = getRegions();
struct region *region;
htmlOpen("%s (%s) Summary Statistics", curTableLabel(), curTable);
char *fileName = trackDbRequiredSetting(curTrack, "binaryFile");
struct chromGraphBin *cgb = chromGraphBinOpen(fileName);
long long regionSize = 0;
long long dataCount = 0;
double total = 0, minVal = 0, maxVal = 0;
boolean first = TRUE;

for (region = regionList; region != NULL; region = region->next)
    {
    if (chromGraphBinSeekToChrom(cgb, region->chrom))
        {
	while (chromGraphBinNextVal(cgb))
	    {
	    int pos = cgb->chromStart;
	    if (pos > region->end)
	        break;
	    if (pos >= region->start)
	        {
		double val = cgb->val;
		if (first)
		    {
		    total = minVal = maxVal = val;
		    dataCount = 1;
		    first = FALSE;
		    }
		else
		    {
		    if (val < minVal) minVal = val;
		    if (val > maxVal) maxVal = val;
		    total += val;
		    ++dataCount;
		    }
		}
	    }
	}
    regionSize += region->end - region->start;
    }
chromGraphBinFree(&cgb);

hTableStart();
hPrintf("<TR><TD>bases in region</TD><TD>%lld</TD></TR>\n", regionSize);
hPrintf("<TR><TD>data points</TD><TD>%lld</TD></TR>\n", dataCount);
if (dataCount > 0)
    {
    hPrintf("<TR><TD>average value</TD><TD>%g</TD></TR>\n", total/dataCount);
    hPrintf("<TR><TD>minimum value</TD><TD>%g</TD></TR>\n", minVal);
    hPrintf("<TR><TD>maximum value</TD><TD>%g</TD></TR>\n", maxVal);
    }
hTableEnd();
htmlClose();
}

void doSummaryStatsChromGraph(struct sqlConnection *conn)
/* Put up page showing summary stats for chromGraph track. */
{
if (isCustomTrack(curTable))
    {
    doSummaryStatsChromGraphCt();
    }
else
    doSummaryStatsBed(conn);
}
