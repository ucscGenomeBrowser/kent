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

static char const rcsid[] = "$Id: wiggle.c,v 1.14 2004/09/03 22:16:26 hiram Exp $";

enum wigOutputType 
/*	type of output requested	*/
    {
    wigOutData, wigOutBed,
    };

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

static boolean checkWigDataFilter(char *db, char *table,
	char **constraint, double *ll, double *ul)
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

static void wigDataHeader(char *name, char *description, char *visibility)
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

static int wigOutRegion(char *table, struct sqlConnection *conn,
	struct region *region, int maxOut, enum wigOutputType wigOutType)
/* Write out wig data in region.  Write up to maxOut elements. 
 * Returns number of elements written. */
{
int linesOut = 0;
char splitTableOrFileName[256];
struct customTrack *ct;
boolean isCustom = FALSE;
struct wiggleDataStream *wds = NULL;
unsigned long long valuesMatched = 0;
int operations = wigFetchAscii;
char *dataConstraint;
double ll = 0.0;
double ul = 0.0;

switch (wigOutType)
    {
    case wigOutBed:
	operations = wigFetchBed;
	break;
    default:
    case wigOutData:
	operations = wigFetchAscii;
	break;
    };

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

wds = wiggleDataStreamNew();

wds->setMaxOutput(wds, maxOut);
wds->setChromConstraint(wds, region->chrom);
wds->setPositionConstraint(wds, region->start, region->end);

if (checkWigDataFilter(database, table, &dataConstraint, &ll, &ul))
    wds->setDataConstraint(wds, dataConstraint, ll, ul);

if (isCustom)
    {
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
	valuesMatched = wds->getData(wds, database,
	    splitTableOrFileName, operations);
	}
    }

switch (wigOutType)
    {
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
}

static void doOutWig(struct trackDb *track, struct sqlConnection *conn,
	enum wigOutputType wigOutType)
{
struct region *regionList = getRegions(), *region;
int maxOut = 100000, oneOut, curOut = 0;
char *name;
extern char *maxOutMenu[];
char *maxOutputStr = NULL;
char *maxOutput = NULL;

name = filterFieldVarName(database, curTable, "", filterMaxOutputVar);
maxOutputStr = cartOptionalString(cart, name);
/*	Don't modify(stripChar) the values sitting in the cart hash	*/
if (NULL == maxOutputStr)
    maxOutput = cloneString(maxOutMenu[0]);
else
    maxOutput = cloneString(maxOutputStr);

stripChar(maxOutput, ',');
maxOut = sqlUnsigned(maxOutput);
freeMem(maxOutput);

textOpen();

wigDataHeader(track->shortLabel, track->longLabel, NULL);

for (region = regionList; region != NULL; region = region->next)
    {
    oneOut = wigOutRegion(track->tableName, conn, region, maxOut - curOut,
	wigOutType);
    curOut += oneOut;
    if (curOut >= maxOut)
        break;
    }
if (curOut >= maxOut)
    warn("Reached output limit of %d data values, please make region smaller,\n\tor set a higher output line limit with the filter settings.", curOut);
}

void doOutWigData(struct trackDb *track, struct sqlConnection *conn)
/* Return wiggle data in variableStep format. */
{
doOutWig(track, conn, wigOutData);
}

void doOutWigBed(struct trackDb *track, struct sqlConnection *conn)
/* Return wiggle data in bed format. */
{
doOutWig(track, conn, wigOutBed);
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
struct wiggleDataStream *wds = NULL;
unsigned long long valuesMatched = 0;
int regionCount = 0;
unsigned span = 0;
char *dataConstraint;
double ll = 0.0;
double ul = 0.0;
boolean haveDataConstraint = FALSE;

startTime = clock1000();


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

wds = wiggleDataStreamNew();

if (checkWigDataFilter(database, table, &dataConstraint, &ll, &ul))
    {
    wds->setDataConstraint(wds, dataConstraint, ll, ul);
    haveDataConstraint = TRUE;
    }

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

    wds->setChromConstraint(wds, region->chrom);

    if (fullGenomeRegion())
	wds->setPositionConstraint(wds, 0, 0);
    else
	wds->setPositionConstraint(wds, region->start, region->end);

    if (haveDataConstraint)
	wds->setDataConstraint(wds, dataConstraint, ll, ul);

    /* depending on what is coming in on regionList, we may need to be
     * smart about how often we call getData for these custom tracks
     * since that is potentially a large file read each time.
     */
    if (isCustom)
	{
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
		region->start, region->end, cart);
	    wds->setSpanConstraint(wds, span);
	    valuesMatched = wds->getData(wds, database,
		splitTableOrFileName, operations);
	    }
	}
    }

if (1 == regionCount)
    statsPreamble(wds, regionList->chrom, regionList->start, regionList->end,
	span, valuesMatched);

/* first TRUE == sort results, second TRUE == html table output */
wds->statsOut(wds, "stdout", TRUE, TRUE);

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

    printHistoGram(histoGramResult);

    freeHistoGram(&histoGramResult);
    wds->freeAscii(wds);
    wds->freeArray(wds);
    }
#endif

wds->freeStats(wds);

wigFetchTime = clock1000() - startTime;
webNewSection("Region and Timing Statistics");
hTableStart();
stringStatRow("region", regionName);
numberStatRow("bases in region", regionSize);
numberStatRow("bases in gaps", gapTotal);
floatStatRow("load and calc time", 0.001*wigFetchTime);
hTableEnd();
htmlClose();
wiggleDataStreamFree(&wds);
}
