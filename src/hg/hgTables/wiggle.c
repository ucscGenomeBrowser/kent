/* wiggle - stuff to process wiggle tracks. 
 * Much of this is lifted from hgText/hgWigText.c */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "portable.h"
#include "obscure.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "cart.h"
#include "web.h"
#include "bed.h"
#include "hdb.h"
#include "hui.h"
#include "hgColors.h"
#include "trackDb.h"
#include "customTrack.h"
#include "wiggle.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: wiggle.c,v 1.4 2004/08/27 23:10:47 hiram Exp $";

boolean isWiggle(char *db, char *table)
/* Return TRUE if db.table is a wiggle. */
{
boolean typeWiggle = FALSE;

if (db != NULL && table != NULL)
    {
    if (isCustomTrack(table))
	{
	struct customTrack *ct = lookupCt(table);
	if (ct->wiggle)
	    typeWiggle = TRUE;
	}
    else
	{
	struct hTableInfo *hti = maybeGetHti(db, table);
	typeWiggle = (hti != NULL && HTI_IS_WIGGLE);
	}
    }
return(typeWiggle);
}

void wigDataHeader(char *name, char *description, char *visibility)
/* Write out custom track header for this wiggle region. */
{
hPrintf("track type=wiggle_0");
if (name != NULL)
    hPrintf(" name=\"%s\"", name);
if (description != NULL)
    hPrintf(" description=\"%s\"", description);
if (visibility != NULL)
    hPrintf(" visibility=%s", visibility);
hPrintf("\n");
}

int wigOutDataRegionDb(char *table, struct sqlConnection *conn,
	struct region *region, int maxOut)
/* Write out wig data in region.  Write up to maxOut elements. 
 * Returns number of elements written. */
{
struct wiggleData *wdList, *wd;
int tableId = 0;
char *constraints = NULL;
int outCount = 0;
boolean span = -1;

wdList = wigFetchData(database, table, 
	region->chrom, region->start, region->end,
	WIG_ALL_DATA, WIG_RETURN_DATA, 
	tableId, NULL, constraints, NULL, maxOut, NULL);
if (wdList == NULL)
    return 0;
for (wd = wdList; wd != NULL; wd = wd->next)
    {
    int i, count = wd->count;
    struct wiggleDatum *data = wd->data;
    if (wd->span != span)
	{
	span = wd->span;
	hPrintf("variableStep");
	hPrintf(" chrom=%s", region->chrom);
	hPrintf(" span=%u\n", span);
	}
    for (i=0; i<count; ++i)
        {
	unsigned start = data->chromStart;
	if (start >= region->start && start < region->end)
	    hPrintf("%u\t%g\n", start+1, data->value);
	++data;
	++outCount;
	}
    }
wiggleDataFreeList(&wdList);
return outCount;
}

int wigOutDataRegionCt(char *table, struct sqlConnection *conn,
	struct region *region, int maxOut)
/* Write out wig data from custom track in region.  Write up 
 * to maxOut elements.  Returns number of elements written. */
{
uglyAbort("Don't known how to get data points for custom wiggle tracks. "
         "Please ask Hiram Clawsom to implement me...."); return 0;
}

int wigOutDataRegion(char *table, struct sqlConnection *conn,
	struct region *region, int maxOut)
/* Write out wig data in region.  Write up to maxOut elements. 
 * Returns number of elements written. */
{
if (isCustomTrack(table))
    return wigOutDataRegionCt(table, conn, region, maxOut);
else
    return wigOutDataRegionDb(table, conn, region, maxOut);
}

void doOutWigData(struct trackDb *track, struct sqlConnection *conn)
/* Save as wiggle data. */
{
struct region *regionList = getRegions(), *region;
int maxOut = 100000, oneOut, curOut = 0;
textOpen();

wigDataHeader(track->shortLabel, track->longLabel, NULL);
for (region = regionList; region != NULL; region = region->next)
    {
    oneOut = wigOutDataRegion(track->tableName, conn, region, maxOut - curOut);
    curOut += oneOut;
    if (curOut >= maxOut)
        break;
    }
if (curOut >= maxOut)
    warn("Only fetching first %d data values, please make region smaller", curOut);
}


void doSummaryStatsWiggle(struct sqlConnection *conn)
/* Put up page showing summary stats for wiggle track. */
{
struct trackDb *track = curTrack;
char *table = track->tableName;
struct region *region, *regionList = getRegionsWithChromEnds();
char *regionName = getRegionName();
long long regionSize = basesInRegion(regionList);
long long gapTotal = gapsInRegion(conn, regionList);
long startTime, wigFetchTime;
char splitTableOrFileName[256];
struct customTrack *ct;
boolean isCustom = FALSE;
struct wiggleDataStream *wDS = NULL;
unsigned long long valuesMatched = 0;
char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */

wDS = newWigDataStream();

htmlOpen("%s (%s) Wiggle Summary Statistics", track->shortLabel, table);

if (isCustomTrack(table))
    {
    ct = lookupCt(table);
    if (! ct->wiggle)
	{
	warn("doSummaryStatsWiggle: called to do wiggle stats on a custom track that isn't wiggle data ?");
	htmlClose();
	destroyWigDataStream(&wDS);
	return;
	}

    safef(splitTableOrFileName,ArraySize(splitTableOrFileName), "%s",
		ct->wigFile);
    isCustom = TRUE;
    }

startTime = clock1000();
for (region = regionList; region != NULL; region = region->next)
    {
    unsigned span = 0;
    boolean hasBin;

    sprintLongWithCommas(num1Buf, region->start + 1);
    sprintLongWithCommas(num2Buf, region->end);
    hPrintf("<P><B> Position: </B> %s:%s-%s</P>\n", region->chrom,
	    num1Buf, num2Buf );
    sprintLongWithCommas(num1Buf, region->end - region->start);
    hPrintf ("<P><B> Total Bases in view: </B> %s </P>\n", num1Buf);

    wDS->setChromConstraint(wDS, region->chrom);
    wDS->setPositionConstraint(wDS, region->start, region->end);
    /* depending on what is coming in on regionList, we may need to be
     * smart about how often we call getData for these custom tracks
     * since that is potentially a large file read each time.
     */
    if (isCustom)
	{
	valuesMatched = wDS->getData(wDS, NULL,
		splitTableOrFileName, wigFetchStats);
	}
    else
	{
	if (hFindSplitTable(region->chrom, table, splitTableOrFileName, &hasBin))
	    {
	    span = spanInUse(conn, splitTableOrFileName, region->chrom,
		region->start, region->end, cart);
	    wDS->setSpanConstraint(wDS, span);
	    valuesMatched = wDS->getData(wDS, database,
		splitTableOrFileName, wigFetchStats);
	    }
	}

    /*	This printout is becoming common to what is already in
     *	hgc/wiggleClick.c, need to put this in one of the library files.
     */
    if (valuesMatched == 0)
	{
	if ( span < (3 * (region->end - region->end)))
	    {
	    hPrintf("<P><B> Viewpoint has too few bases to calculate statistics. </B></P>\n");
	    hPrintf("<P><B> Zoom out to at least %d bases to see statistics. </B></P>\n", 3 * span);
	    }
	else
	    hPrintf("<P><B> No data found in this region. </B></P>\n");
	}
    else
	{
	sprintLongWithCommas(num1Buf, wDS->stats->count * wDS->stats->span);
	hPrintf("<P><B> Statistics on: </B> %s <B> bases </B> (%% %.4f coverage)</P>\n",
	    num1Buf, 100.0*(wDS->stats->count * wDS->stats->span)/(region->end - region->start));
	}

    hTableStart();
    hPrintf("<TR><TD>");

    /* first TRUE == sort results, second TRUE == html table output */
    wDS->statsOut(wDS, "stdout", TRUE, TRUE);

    hPrintf("</TD></TR>");

    /*	TBD: histogram printout here (lib call)	*/

    hTableEnd();
    wDS->freeStats(wDS);
    wDS->freeConstraints(wDS);
    }
wigFetchTime = clock1000() - startTime;

webNewSection("Region and Timing Statistics");
hTableStart();
stringStatRow("region", regionName);
numberStatRow("bases in region", regionSize);
numberStatRow("bases in gaps", gapTotal);
floatStatRow("load and calc time", 0.001*wigFetchTime);
hTableEnd();
htmlClose();
destroyWigDataStream(&wDS);
}
