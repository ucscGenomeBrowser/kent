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

static char const rcsid[] = "$Id: wiggle.c,v 1.3 2004/07/22 02:10:23 kent Exp $";

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
struct wiggleData *wdList, *wd;
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
long long regionSize = basesInRegion(regionList);
long long gapTotal = gapsInRegion(conn, regionList);
long long realSize = regionSize - gapTotal;
long startTime, wigFetchTime;
char *regionName = getRegionName();
struct wiggleData *wdList, *wd;
struct wiggleStats *wsList = NULL, *ws;
int wdCount = 0;
double sum = 0, minData = 0, maxData = 0;
boolean gotAny = FALSE;
long long dataCount = 0;
int span = 0;

htmlOpen("%s (%s) Wiggle Summary Statistics", track->shortLabel, table);

startTime = clock1000();
for (region = regionList; region != NULL; region = region->next)
    {
    wdList = wigFetchData(database, table, 
	region->chrom, region->start, region->end,
	WIG_ALL_DATA, WIG_DATA_NOT_RETURNED, 
	0, NULL, 0, NULL, 100000, &wsList);
    for (ws = wsList; ws != NULL; ws = ws->next)
	{
	double maxOne = ws->lowerLimit + ws->dataRange;
	if (dataCount)
	    {
	    if (ws->lowerLimit < minData) minData = ws->lowerLimit;
	    if (maxOne > maxData) maxData = maxOne;
	    }
	else
	    {
	    minData = ws->lowerLimit;
	    maxData = maxOne;
	    span = ws->span;
	    }
	dataCount += ws->count;
        sum += ws->mean * ws->count;
	}
    /* How to free wsList? */
    wiggleDataFreeList(&wdList);
    wsList = NULL;
    }
wigFetchTime = clock1000() - startTime;

hTableStart();
numberStatRow("# of data values", dataCount);
if (dataCount > 0)
    {
    floatStatRow("minimum value" , minData);
    floatStatRow("average value" , sum/dataCount);
    floatStatRow("maximum value" , maxData);
    numberStatRow("bases per value", span);
    }
hTableEnd();

webNewSection("Region and Timing Statistics");
hTableStart();
stringStatRow("region", regionName);
numberStatRow("bases in region", regionSize);
numberStatRow("bases in gaps", gapTotal);
floatStatRow("load and calc time", 0.001*wigFetchTime);
hTableEnd();
htmlClose();
}

