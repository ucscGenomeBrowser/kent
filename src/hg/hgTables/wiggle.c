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
#include "botDelay.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: wiggle.c,v 1.35 2004/11/19 14:58:35 kent Exp $";

extern char *maxOutMenu[];

enum wigOutputType 
/*	type of output requested from static int wigOutRegion()	*/
    {
    wigOutData, wigOutBed, wigDataNoPrint,
    };


/*	a common set of stuff is accumulating in the various
 *	routines that call getData.  For the moment make it a macro,
 *	perhaps later it can turn into a routine of its own.
 */
#define WIG_INIT \
if (isCustomTrack(table)) \
    { \
    ct = lookupCt(table); \
    if (! ct->wiggle) \
	errAbort("called to work on a custom track '%s' that isn't wiggle data ?", table); \
 \
    safef(splitTableOrFileName,ArraySize(splitTableOrFileName), "%s", \
		ct->wigFile); \
    isCustom = TRUE; \
    hasConstraint = checkWigDataFilter("ct", table, &dataConstraint, &ll, &ul); \
    } \
else \
    hasConstraint = checkWigDataFilter(database, table, &dataConstraint, \
			&ll, &ul); \
\
wds = wiggleDataStreamNew(); \
\
if (anyIntersection()) \
    { \
    table2 = cartString(cart, hgtaIntersectTrack); \
    } \
\
if (hasConstraint)\
    wds->setDataConstraint(wds, dataConstraint, ll, ul);

static boolean checkWigDataFilter(char *db, char *table,
	char **constraint, double *ll, double *ul)
/*	check if filter exists, return its values, call with db="ct" for
 *	custom tracks	*/
{
char varPrefix[128];
struct hashEl *varList, *var;
char *pat = NULL;
char *cmp = NULL;

safef(varPrefix, sizeof(varPrefix), "%s%s.%s.", hgtaFilterVarPrefix, db, table);

varList = cartFindPrefix(cart, varPrefix);
if (varList == NULL)
    return FALSE;

/*	check varList, look for dataValue.pat and dataValue.cmp	*/

for (var = varList; var != NULL; var = var->next)
    {
    if (endsWith(var->name, ".pat"))
	{
	char *name;
	name = cloneString(var->name);
	tolowers(name);
	/*	make sure we are actually looking at datavalue	*/
	if (stringIn("datavalue", name))
	    {
	    pat = cloneString(var->val);
	    }
	freeMem(name);
	}
    if (endsWith(var->name, ".cmp"))
	{
	char *name;
	name = cloneString(var->name);
	tolowers(name);
	/*	make sure we are actually looking at datavalue	*/
	if (stringIn("datavalue", name))
	    {
	    cmp = cloneString(var->val);
	    tolowers(cmp);
	    if (stringIn("ignored", cmp))
		freez(&cmp);
	    }
	freeMem(name);
	}
    }

/*	Must get them both for this to work	*/
if (cmp && pat)
    {
    int wordCount = 0;
    char *words[2];
    char *dupe = cloneString(pat);

    wordCount = chopString(dupe, ", \t\n", words, ArraySize(words));
    switch (wordCount)
	{
	case 2: if (ul) *ul = sqlDouble(words[1]);
	case 1: if (ll) *ll = sqlDouble(words[0]);
		break;
	default:
	    warn("can not understand numbers input for dataValue filter");
	    errAbort(
	    "dataValue filter must be one or two numbers (two for 'in range')");
	}
    if (sameWord(cmp,"in range") && (wordCount != 2))
	errAbort("'in range' dataValue filter must have two numbers input\n");

    if (constraint)
	*constraint = cmp;

    return TRUE;
    }
else
    return FALSE;
}	/*	static boolean checkWigDataFilter()	*/

static void wigDataHeader(char *name, char *description, char *visibility,
	enum wigOutputType wigOutType)
/* Write out custom track header for this wiggle region. */
{
switch (wigOutType)
    {
    case wigOutBed:
	hPrintf("track ");
        break;
    default:
    case wigOutData:
	hPrintf("track type=wiggle_0");
        break;
    };

if (name != NULL)
    hPrintf(" name=\"%s\"", name);
if (description != NULL)
    hPrintf(" description=\"%s\"", description);
if (visibility != NULL)
    hPrintf(" visibility=%s", visibility);
hPrintf("\n");
}

static int wigOutRegion(char *table, struct sqlConnection *conn,
	struct region *region, int maxOut, enum wigOutputType wigOutType,
	struct wigAsciiData **data)
/* Write out wig data in region.  Write up to maxOut elements. 
 * Returns number of elements written. */
{
int linesOut = 0;
char splitTableOrFileName[256];
struct customTrack *ct;
boolean isCustom = FALSE;
boolean hasConstraint = FALSE;
struct wiggleDataStream *wds = NULL;
unsigned long long valuesMatched = 0;
int operations = wigFetchAscii;
char *dataConstraint;
double ll = 0.0;
double ul = 0.0;
char *table2 = NULL;
struct bed *intersectBedList = NULL;

switch (wigOutType)
    {
    case wigOutBed:
	operations = wigFetchBed;
	break;
    default:
    case wigDataNoPrint:
    case wigOutData:
	operations = wigFetchAscii;
	break;
    };

WIG_INIT;
if (hasConstraint)
    freeMem(dataConstraint);	/* been cloned into wds */

wds->setMaxOutput(wds, maxOut);
wds->setChromConstraint(wds, region->chrom);
wds->setPositionConstraint(wds, region->start, region->end);

if (table2)
    {
    /* Load up intersecting bedList2 (to intersect with) */
    struct trackDb *track2 = findTrack(table2, fullTrackList);
    struct lm *lm2 = lmInit(64*1024);
    intersectBedList = getFilteredBeds(conn, track2->tableName, region, lm2);
    }


if (isCustom)
    {
    if (table2 || intersectBedList)
	valuesMatched = wds->getDataViaBed(wds, NULL, splitTableOrFileName,
	    operations, &intersectBedList);
    else
	valuesMatched = wds->getData(wds, NULL,
	    splitTableOrFileName, operations);
    }
else
    {
    boolean hasBin;

    if (hFindSplitTable(region->chrom, table, splitTableOrFileName, &hasBin))
	{
	/* XXX TBD, watch for a span limit coming in as an SQL filter */
/*
	span = minSpan(conn, splitTableOrFileName, region->chrom,
	    region->start, region->end, cart);
	wds->setSpanConstraint(wds, span);
*/
	if (table2 || intersectBedList)
	    {
	    unsigned span;	
	    span = minSpan(conn, splitTableOrFileName, region->chrom,
		region->start, region->end, cart, curTrack);
	    wds->setSpanConstraint(wds, span);
	    valuesMatched = wds->getDataViaBed(wds, database,
		splitTableOrFileName, operations, &intersectBedList);
	    }
	else
	    {
	    valuesMatched = wds->getData(wds, database,
		splitTableOrFileName, operations);
	    }
	}
    }

switch (wigOutType)
    {
    case wigDataNoPrint:
	if (data)
	    {
	    if (*data != NULL)	/* no exercise of this function yet	*/
		{	/*	data not null, add to existing list	*/
		struct wigAsciiData *asciiData;
		struct wigAsciiData *next;
		for (asciiData = *data; asciiData; asciiData = next)
		    {
		    next = asciiData->next;
		    slAddHead(&wds->ascii, asciiData);
		    }
		}
	    wds->sortResults(wds);
	    *data = wds->ascii;	/* moving the list to *data */
	    wds->ascii = NULL;	/* gone as far as wds is concerned */
	    }
	    linesOut = valuesMatched;
	break;
    case wigOutBed:
	linesOut = wds->bedOut(wds, "stdout", TRUE);/* TRUE == sort output */
	break;
    default:
    case wigOutData:
	linesOut = wds->asciiOut(wds, "stdout", TRUE, FALSE);
	break;		/* TRUE == sort output, FALSE == not raw data out */
    };

wiggleDataStreamFree(&wds);

return linesOut;
}	/*	static int wigOutRegion()	*/

static int wigMaxOutput(char *db, char *table)
/*	return maxOut value	*/
{
char *maxOutputStr = NULL;
char *name;
int maxOut;
char *maxOutput = NULL;

if (isCustomTrack(table))
    name = filterFieldVarName("ct", curTable, "", filterMaxOutputVar);
else
    name = filterFieldVarName(db, curTable, "", filterMaxOutputVar);

maxOutputStr = cartOptionalString(cart, name);
/*	Don't modify(stripChar) the values sitting in the cart hash	*/
if (NULL == maxOutputStr)
    maxOutput = cloneString(maxOutMenu[0]);
else
    maxOutput = cloneString(maxOutputStr);

stripChar(maxOutput, ',');
maxOut = sqlUnsigned(maxOutput);
freeMem(maxOutput);

return maxOut;
}

static void doOutWig(struct trackDb *track, char *table, struct sqlConnection *conn,
	enum wigOutputType wigOutType)
{
struct region *regionList = getRegions(), *region;
int maxOut = 0, outCount, curOut = 0;
char *shortLabel = table, *longLabel = table;

maxOut = wigMaxOutput(database, curTable);

textOpen();

if (track != NULL)
    {
    shortLabel = track->shortLabel;
    longLabel = track->longLabel;
    }
wigDataHeader(shortLabel, longLabel, NULL, wigOutType);

for (region = regionList; region != NULL; region = region->next)
    {
    outCount = wigOutRegion(table, conn, region, maxOut - curOut,
	wigOutType, NULL);
    curOut += outCount;
    if (curOut >= maxOut)
        break;
    }
if (curOut >= maxOut)
    warn("Reached output limit of %d data values, please make region smaller,\n\tor set a higher output line limit with the filter settings.", curOut);
}

/***********   PUBLIC ROUTINES  *********************************/

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

struct bed *getWiggleAsBed(
    char *db, char *table, 	/* Database and table. */
    struct region *region,	/* Region to get data for. */
    char *filter, 		/* Filter to add to SQL where clause if any. */
    struct hash *idHash, 	/* Restrict to id's in this hash if non-NULL. */
    struct lm *lm,		/* Where to allocate memory. */
    struct sqlConnection *conn)	/* SQL connection to work with */
/* Return a bed list of all items in the given range in table.
 * Cleanup result via lmCleanup(&lm) rather than bedFreeList.  */
/* filter, idHash and lm are currently unused, perhaps future use	*/
{
struct bed *bedList=NULL;
char splitTableOrFileName[256];
struct customTrack *ct;
boolean isCustom = FALSE;
boolean hasConstraint = FALSE;
struct wiggleDataStream *wds = NULL;
unsigned long long valuesMatched = 0;
int operations = wigFetchBed;
char *dataConstraint;
double ll = 0.0;
double ul = 0.0;
char *table2 = NULL;
struct bed *intersectBedList = NULL;
int maxOut;

WIG_INIT;
if (hasConstraint)
    freeMem(dataConstraint);	/* been cloned into wds */

maxOut = wigMaxOutput(database, curTable);

wds->setMaxOutput(wds, maxOut);

wds->setChromConstraint(wds, region->chrom);
wds->setPositionConstraint(wds, region->start, region->end);

if (table2)
    {
    /* Load up intersecting bedList2 (to intersect with) */
    struct trackDb *track2 = findTrack(table2, fullTrackList);
    struct lm *lm2 = lmInit(64*1024);
    intersectBedList = getFilteredBeds(conn, track2->tableName, region, lm2);
    }


if (isCustom)
    {
    if (table2 || intersectBedList)
	valuesMatched = wds->getDataViaBed(wds, NULL, splitTableOrFileName,
	    operations, &intersectBedList);
    else
	valuesMatched = wds->getData(wds, NULL,
		splitTableOrFileName, operations);
    }
else
    {
    boolean hasBin;

    if (conn == NULL)
	errAbort( "getWiggleAsBed: NULL conn given for database table");

    if (hFindSplitTable(region->chrom, table, splitTableOrFileName, &hasBin))
	{
	unsigned span = 0;

	/* XXX TBD, watch for a span limit coming in as an SQL filter */
	span = minSpan(conn, splitTableOrFileName, region->chrom,
	    region->start, region->end, cart, curTrack);
	wds->setSpanConstraint(wds, span);

	if (table2 || intersectBedList)
	    {
	    valuesMatched = wds->getDataViaBed(wds, database,
		splitTableOrFileName, operations, &intersectBedList);
	    }
	else
	    valuesMatched = wds->getData(wds, database,
		splitTableOrFileName, operations);
	}
    }

if (valuesMatched > 0)
    {
    struct bed *bed;

    wds->sortResults(wds);
    for (bed = wds->bed; bed != NULL; bed = bed->next)
	{
	struct bed *copy = lmCloneBed(bed, lm);
	slAddHead(&bedList, copy);
	}
    slReverse(&bedList);
    }

wiggleDataStreamFree(&wds);

return bedList;
}	/*	struct bed *getWiggleAsBed()	*/

struct wigAsciiData *getWiggleAsData(struct sqlConnection *conn, char *table,
	struct region *region, struct lm *lm)
/*	return the wigAsciiData list	*/
{
int maxOut = 0;
struct wigAsciiData *data = NULL;
int outCount;

maxOut = wigMaxOutput(database, curTable);
outCount = wigOutRegion(table, conn, region, maxOut, wigDataNoPrint, &data);

return data;
}

void doOutWigData(struct trackDb *track, char *table, struct sqlConnection *conn)
/* Return wiggle data in variableStep format. */
{
hgBotDelay();
doOutWig(track, table, conn, wigOutData);
}

void doOutWigBed(struct trackDb *track, char *table, struct sqlConnection *conn)
/* Return wiggle data in bed format. */
{
hgBotDelay();
doOutWig(track, table, conn, wigOutBed);
}

void doSummaryStatsWiggle(struct sqlConnection *conn)
/* Put up page showing summary stats for wiggle track. */
{
struct trackDb *track = curTrack;
char *table = curTable;
struct region *region, *regionList = getRegions();
char *regionName = getRegionName();
long long regionSize = 0;
long long gapTotal = 0;
long startTime = 0, wigFetchTime = 0, freeTime = 0;
char splitTableOrFileName[256];
struct customTrack *ct;
boolean isCustom = FALSE;
struct wiggleDataStream *wds = NULL;
unsigned long long valuesMatched = 0;
int regionCount = 0;
int regionsDone = 0;
unsigned span = 0;
char *dataConstraint;
double ll = 0.0;
double ul = 0.0;
boolean hasConstraint = FALSE;
char *table2 = NULL;
boolean fullGenome = FALSE;
boolean statsHeaderDone = FALSE;
boolean gotSome = FALSE;
char *shortLabel = table;
long long statsItemCount = 0;	/*	global accumulators for overall */
int statsSpan = 0;		/*	stats summary on a multiple region */
double statsSumData = 0.0;	/*	output */
double statsSumSquares = 0.0;		/*	"  "	*/
double lowerLimit = INFINITY;		/*	"  "	*/
double upperLimit = -1.0 * INFINITY;	/*	"  "	*/

hgBotDelay();
startTime = clock1000();
if (track != NULL)
     shortLabel = track->shortLabel;

/*	Count the regions, when only one, we can do more stats */
for (region = regionList; region != NULL; region = region->next)
    ++regionCount;

htmlOpen("%s (%s) Wiggle Summary Statistics", shortLabel, table);

fullGenome = fullGenomeRegion();

WIG_INIT;

for (region = regionList; region != NULL; region = region->next)
    {
    struct bed *intersectBedList = NULL;
    boolean hasBin;
    int operations;

    ++regionsDone;

    if (table2)
	{
	/* Load up intersecting bedList2 (to intersect with) */
	struct trackDb *track2 = findTrack(table2, fullTrackList);
	struct lm *lm2 = lmInit(64*1024);
	intersectBedList = getFilteredBeds(conn, track2->tableName, region, lm2);
	}

    operations = wigFetchStats;
#if defined(NOT)
    /*	can't do the histogram now, that operation times out	*/
    if (1 == regionCount)
	operations |= wigFetchAscii;
#endif

    wds->setChromConstraint(wds, region->chrom);

    if (fullGenome)
	wds->setPositionConstraint(wds, 0, 0);
    else
	wds->setPositionConstraint(wds, region->start, region->end);

    if (hasConstraint)
	wds->setDataConstraint(wds, dataConstraint, ll, ul);

    /* depending on what is coming in on regionList, we may need to be
     * smart about how often we call getData for these custom tracks
     * since that is potentially a large file read each time.
     */
    if (isCustom)
	{
	if (table2 || intersectBedList)
	    valuesMatched = wds->getDataViaBed(wds, NULL, splitTableOrFileName,
		operations, &intersectBedList);
	else
	    valuesMatched = wds->getData(wds, NULL,
				splitTableOrFileName, operations);
	/*  XXX We need to properly get the smallest span for custom tracks */
	/*	This is not necessarily the correct answer here	*/
	if (wds->stats)
	    span = wds->stats->span;
	else
	    span = 1;
	}
    else
	{
	if (hFindSplitTable(region->chrom, table, splitTableOrFileName, &hasBin))
	    {
	    span = minSpan(conn, splitTableOrFileName, region->chrom,
		region->start, region->end, cart, curTrack);
	    wds->setSpanConstraint(wds, span);
	    if (table2 || intersectBedList)
		{
		valuesMatched = wds->getDataViaBed(wds, database,
		    splitTableOrFileName, operations, &intersectBedList);
		span = 1;
		}
	    else
		valuesMatched = wds->getData(wds, database,
		    splitTableOrFileName, operations);
	    }
	}
    /*	when doing multiple regions, we need to print out each result as
     *	it happens to keep the connection open to the browser and
     *	prevent any timeout since this could take a while.
     *	(worst case test is quality track on panTro1)
     */
    if (wds->stats)
	statsItemCount += wds->stats->count;
    if (wds->stats && (regionCount > 1) && (valuesMatched > 0))
	{
	double sumData = wds->stats->mean * wds->stats->count;
	double sumSquares;

	if (wds->stats->count > 1)
	    sumSquares = (wds->stats->variance * (wds->stats->count - 1)) +
		((sumData * sumData)/wds->stats->count);
	else
	    sumSquares = sumData * sumData;

	/*	global accumulators for overall summary	*/
	statsSpan = wds->stats->span;
	statsSumData += sumData;
	statsSumSquares += sumSquares;
	if (wds->stats->lowerLimit < lowerLimit)
	    lowerLimit = wds->stats->lowerLimit;
	if ((wds->stats->lowerLimit + wds->stats->dataRange) > upperLimit)
	    upperLimit = wds->stats->lowerLimit + wds->stats->dataRange;

	if (statsHeaderDone)
	    wds->statsOut(wds, "stdout", TRUE, TRUE, FALSE, TRUE);
	else
	    {
	    wds->statsOut(wds, "stdout", TRUE, TRUE, TRUE, TRUE);
	    statsHeaderDone = TRUE;
	    }
	wds->freeStats(wds);
	gotSome = TRUE;
	}
    if ((regionCount > MAX_REGION_DISPLAY) &&
		(regionsDone >= MAX_REGION_DISPLAY))
	{
	hPrintf("<TR><TH ALIGN=CENTER COLSPAN=12> Can not display more "
	    "than %d regions, <BR> would take too much time </TH></TR>\n",
		MAX_REGION_DISPLAY);
	break;	/*	exit this for loop	*/
	}
    }	/*for (region = regionList; region != NULL; region = region->next) */

if (hasConstraint)
    freeMem(dataConstraint);	/* been cloned into wds */

if (1 == regionCount)
    {
    statsPreamble(wds, regionList->chrom, regionList->start, regionList->end,
	span, valuesMatched, table2);
    /* 3 X TRUE = sort results, html table output, with header,
     *	the FALSE means close the table after printing, no more rows to
     *	come.  The case in the if() statement was already taken care of
     *	in the statsPreamble() printout.  No need to do that again.
     */

    if ( ! ((valuesMatched == 0) && table2) )
	wds->statsOut(wds, "stdout", TRUE, TRUE, TRUE, FALSE);
    regionSize = basesInRegion(regionList,0);
    gapTotal = gapsInRegion(conn, regionList,0);
    }
else
    {	/* this is a bit of a kludge here since these printouts are done in the
	 *	library source wigDataStream.c statsOut() function and
	 *	this is a clean up of that.  That function should be
	 *	pulled out of there and made independent and more
	 *	versatile.
	 */
    long long realSize;
    double variance;
    double stddev;

    /*	Too expensive to lookup the numbers for thousands of regions */
    if (regionsDone >= MAX_REGION_DISPLAY)
	{
	regionSize = basesInRegion(regionList,MAX_REGION_DISPLAY);
	gapTotal = gapsInRegion(conn, regionList,MAX_REGION_DISPLAY);
	}
    else
	{
	regionSize = basesInRegion(regionList,0);
	gapTotal = gapsInRegion(conn, regionList,0);
	}
    realSize = regionSize - gapTotal;

    /*	close the table which was left open in the loop above	*/
    if (!gotSome)
	hPrintf("<TR><TH ALIGN=CENTER COLSPAN=12> No data found matching this request </TH></TR>\n");

    hPrintf("<TR><TH ALIGN=LEFT> SUMMARY: </TH>\n");
    hPrintf("\t<TD> &nbsp; </TD>\n");	/*	chromStart	*/
    hPrintf("\t<TD> &nbsp; </TD>\n");	/*	chromEnd	*/
    hPrintf("\t<TD ALIGN=RIGHT> ");
    printLongWithCommas(stdout, statsItemCount);
    hPrintf(" </TD>\n" );
    hPrintf("\t<TD ALIGN=RIGHT> %d </TD>\n", statsSpan);
    hPrintf("\t<TD ALIGN=RIGHT> ");
    printLongWithCommas(stdout, statsItemCount*statsSpan);
    hPrintf("&nbsp;(%.2f%%) </TD>\n",
	100.0*(double)(statsItemCount*statsSpan)/(double)realSize);
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", lowerLimit);
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", upperLimit);
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", upperLimit - lowerLimit);
    if (statsItemCount > 0)
	hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", statsSumData/statsItemCount);
    else
	hPrintf("\t<TD ALIGN=RIGHT> 0.0 </TD>\n");
    stddev = 0.0;
    variance = 0.0;
    if (statsItemCount > 1)
	{
	variance = (statsSumSquares -
	    ((statsSumData * statsSumData)/(double) statsItemCount)) /
		(double) (statsItemCount - 1);
	if (variance > 0.0)
	    stddev = sqrt(variance);
	}
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", variance);
    hPrintf("\t<TD ALIGN=RIGHT> %g </TD>\n", stddev);
    hPrintf("</TR>\n");
    wigStatsTableHeading(stdout, TRUE);
    hPrintf("</TABLE></TD></TR></TABLE></P>\n");
    }


#if defined(NOT)
/*	can't do the histogram now, that operation times out	*/
/*	Single region, we can do the histogram	*/
if ((valuesMatched > 1) && (1 == regionCount))
    {
    float *valuesArray = NULL;
    size_t valueCount = 0;
    struct histoResult *histoGramResult;

    /*	convert the ascii data listings to one giant float array 	*/
    valuesArray = wds->asciiToDataArray(wds, valuesMatched, &valueCount);

    /*	histoGram() may return NULL if it doesn't work	*/

    histoGramResult = histoGram(valuesArray, valueCount,
	    NAN, (unsigned) 0, NAN, (float) wds->stats->lowerLimit,
		(float) (wds->stats->lowerLimit + wds->stats->dataRange),
		(struct histoResult *)NULL);

    printHistoGram(histoGramResult, TRUE);	/* TRUE == html output */

    freeHistoGram(&histoGramResult);
    wds->freeAscii(wds);
    wds->freeArray(wds);
    }
#endif

wds->freeStats(wds);
wiggleDataStreamFree(&wds);

wigFetchTime = clock1000() - startTime;
webNewSection("Region and Timing Statistics");
hTableStart();
stringStatRow("region", regionName);
numberStatRow("bases in region", regionSize);
numberStatRow("bases in gaps", gapTotal);
floatStatRow("load and calc time", 0.001*wigFetchTime);
stringStatRow("filter", (anyFilter() ? "on" : "off"));
stringStatRow("intersection", (anyIntersection() ? "on" : "off"));
hTableEnd();
htmlClose();
}	/*	void doSummaryStatsWiggle(struct sqlConnection *conn)	*/

void wigShowFilter(struct sqlConnection *conn)
/* print out wiggle data value filter */
{
double ll, ul;
char *constraint;

if (checkWigDataFilter(database, curTable, &constraint, &ll, &ul))
{
    if (constraint && sameWord(constraint, "in range"))
	{
	hPrintf("&nbsp;&nbsp;data value %s [%g : %g)\n", constraint, ll, ul);
	}
    else
	hPrintf("&nbsp;&nbsp;data value %s %g\n", constraint, ll);
    freeMem(constraint);
}
}
