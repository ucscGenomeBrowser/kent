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

static char const rcsid[] = "$Id: wiggle.c,v 1.7 2004/08/31 22:09:31 hiram Exp $";

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

int wigOutDataRegion(char *table, struct sqlConnection *conn,
	struct region *region, int maxOut)
/* Write out wig data in region.  Write up to maxOut elements. 
 * Returns number of elements written. */
{
char splitTableOrFileName[256];
struct customTrack *ct;
boolean isCustom = FALSE;
struct wiggleDataStream *wDS = NULL;
unsigned span = 0;
unsigned long long valuesMatched = 0;
int operations = wigFetchAscii;

if (isCustomTrack(table))
    {
    ct = lookupCt(table);
    if (! ct->wiggle)
	{
	warn("doSummaryStatsWiggle: called to do wiggle stats on a custom track that isn't wiggle data ?");
	htmlClose();
	return 0;
	}

    safef(splitTableOrFileName,ArraySize(splitTableOrFileName), "%s",
		ct->wigFile);
    isCustom = TRUE;
    }

wDS = newWigDataStream();

wDS->setMaxOutput(wDS, maxOut);
wDS->setChromConstraint(wDS, region->chrom);
wDS->setPositionConstraint(wDS, region->start, region->end);

if (isCustom)
    {
    valuesMatched = wDS->getData(wDS, NULL,
	    splitTableOrFileName, operations);
    /*  XXX We need to properly get the smallest span for custom tracks */
    /*	This is not necessarily the correct answer here	*/
    if (wDS->stats)
	span = wDS->stats->span;
    else
	span = 1;
    }
else
    {
    boolean hasBin;

    if (hFindSplitTable(region->chrom, table, splitTableOrFileName, &hasBin))
	{
	span = minSpan(conn, splitTableOrFileName, region->chrom,
	    region->start, region->end, cart);
	wDS->setSpanConstraint(wDS, span);
	valuesMatched = wDS->getData(wDS, database,
	    splitTableOrFileName, operations);
	}
    }

wDS->asciiOut(wDS, "stdout", TRUE, FALSE);

destroyWigDataStream(&wDS);

return valuesMatched;
}

void doOutWigData(struct trackDb *track, struct sqlConnection *conn)
/* Save as wiggle data. */
{
struct region *regionList = getRegions(), *region;
/*int maxOut = 100000, oneOut, curOut = 0;*/
int maxOut = 100, oneOut, curOut = 0;
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
int regionCount = 0;
unsigned span = 0;

/*	Count the regions, when only one, we can do a histogram	*/
for (region = regionList; region != NULL; region = region->next)
    ++regionCount;

htmlOpen("%s (%s) Wiggle Summary Statistics", track->shortLabel, table);

if (isCustomTrack(table))
    {
    ct = lookupCt(table);
    if (! ct->wiggle)
	{
	warn("doSummaryStatsWiggle: called to do wiggle stats on a custom track that isn't wiggle data ?");
	htmlClose();
	return;
	}

    safef(splitTableOrFileName,ArraySize(splitTableOrFileName), "%s",
		ct->wigFile);
    isCustom = TRUE;
    }

wDS = newWigDataStream();

startTime = clock1000();
for (region = regionList; region != NULL; region = region->next)
    {
    boolean hasBin;
    int operations;

    operations = wigFetchStats;
#if defined(NOT)
    /*	can't do the histogram now, that operation times out	*/
    if (1 == regionCount)
	operations |= wigFetchAscii;
#endif

    wDS->setChromConstraint(wDS, region->chrom);

    if (fullGenomeRegion())
	wDS->setPositionConstraint(wDS, 0, 0);
    else
	wDS->setPositionConstraint(wDS, region->start, region->end);

    /* depending on what is coming in on regionList, we may need to be
     * smart about how often we call getData for these custom tracks
     * since that is potentially a large file read each time.
     */
    if (isCustom)
	{
	valuesMatched = wDS->getData(wDS, NULL,
		splitTableOrFileName, operations);
	/*  XXX We need to properly get the smallest span for custom tracks */
	/*	This is not necessarily the correct answer here	*/
	if (wDS->stats)
	    span = wDS->stats->span;
	else
	    span = 1;
	}
    else
	{
	if (hFindSplitTable(region->chrom, table, splitTableOrFileName, &hasBin))
	    {
	    span = minSpan(conn, splitTableOrFileName, region->chrom,
		region->start, region->end, cart);
	    wDS->setSpanConstraint(wDS, span);
	    valuesMatched = wDS->getData(wDS, database,
		splitTableOrFileName, operations);
	    }
	}

    wDS->freeConstraints(wDS);
    }
wigFetchTime = clock1000() - startTime;

if (1 == regionCount)
    statsPreamble(wDS, regionList->chrom, regionList->start, regionList->end,
	span, valuesMatched);

/* first TRUE == sort results, second TRUE == html table output */
wDS->statsOut(wDS, "stdout", TRUE, TRUE);

#if defined(NOT)
/*	can't do the histogram now, that operation times out	*/
/*	Single region, we can do the histogram	*/
if ((valuesMatched > 1) && (1 == regionCount))
    {
    float *valuesArray = NULL;
    size_t valueCount = 0;
    struct histoResult *histoGramResult;

    /*	convert the ascii data listings to one giant float array 	*/
    valuesArray = wDS->asciiToDataArray(wDS, valuesMatched, &valueCount);

    /*	histoGram() may return NULL if it doesn't work	*/

    histoGramResult = histoGram(valuesArray, valueCount,
	    NAN, (unsigned) 0, NAN, (float) wDS->stats->lowerLimit,
		(float) (wDS->stats->lowerLimit + wDS->stats->dataRange),
		(struct histoResult *)NULL);

    printHistoGram(histoGramResult);

    freeHistoGram(&histoGramResult);
    wDS->freeAscii(wDS);
    wDS->freeArray(wDS);
    }
#endif

wDS->freeStats(wDS);

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
