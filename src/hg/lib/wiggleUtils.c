/*	wiggleUtils - operations to fetch data from .wib files
 *	first used in hgText
 */
#include "common.h"
#include "jksql.h"
#include "dystring.h"
#include "hdb.h"
#include "wiggle.h"
#include "hCommon.h"
#include "obscure.h"

static char const rcsid[] = "$Id: wiggleUtils.c,v 1.21 2004/08/30 19:51:35 hiram Exp $";

void statsPreamble(struct wiggleDataStream *wDS, char *chrom,
    int winStart, int winEnd, unsigned span, unsigned long long valuesMatched)
{
char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */

sprintLongWithCommas(num1Buf, winStart + 1);
sprintLongWithCommas(num2Buf, winEnd);
printf("<P><B> Position: </B> %s:%s-%s</P>\n", chrom, num1Buf, num2Buf );
sprintLongWithCommas(num1Buf, winEnd - winStart);
printf("<P><B> Total Bases in view: </B> %s </P>\n", num1Buf);

/*	This printout is becoming common to what is already in
 *	hgc/wiggleClick.c, need to put this in one of the library files.
 */
if (valuesMatched == 0)
    {
    if ( span < (3 * (winEnd - winStart)))
	{
	printf("<P><B> XXX Viewpoint has too few bases to calculate statistics. </B></P>\n");
	printf("<P><B> Zoom out to at least %d bases to see statistics. </B></P>\n", 3 * span);
	}
    else
	printf("<P><B> No data found in this region. </B></P>\n");
    }
else
    {
    sprintLongWithCommas(num1Buf, wDS->stats->count * wDS->stats->span);
    printf(
	"<P><B> Statistics on: </B> %s <B> bases </B> (%% %.4f coverage)</P>\n",
	num1Buf,
	100.0*(wDS->stats->count * wDS->stats->span)/(winEnd - winStart));
    }
}

int spanInUse(struct sqlConnection *conn, char *table, char *chrom,
	int winStart, int winEnd, struct cart *cart)
/*	determine span used during hgTracks display,
 *	winEnd == 0 means whole chrom	*/
{
struct sqlResult *sr;
char query[256];
char **row;
float basesPerPixel = 0.0;
int spanInUse = 0;
struct hashCookie cookie;
int insideWidth;
int minSpan = BIGNUM;
int maxSpan = 0;
int spanCount = 0;
struct hash *spans = newHash(0);	/*	list of spans in this table */
struct hashEl *el;
int insideX = hgDefaultGfxBorder;
int pixWidth = atoi(cartUsualString(cart, "pix", "620" ));
boolean withLeftLabels = cartUsualBoolean(cart, "leftLabels", TRUE);

/*	winEnd less than 1 (i.e. == 0), we need to find this chrom size	*/
if (winEnd < 1)
    {
    safef(query, ArraySize(query),
	"SELECT size from chromInfo where chrom = '%s'", chrom);
    sr = sqlMustGetResult(conn,query);
    if ((row = sqlNextRow(sr)) == NULL)
	errAbort("spanInUse: query failed: '%s'\n", query);
    winEnd = sqlUnsigned(row[0]);
    sqlFreeResult(&sr);
    if (winEnd < 1)
	errAbort("spanInUse: failed to find valid chrom size via query: '%s'\n", query);
    }

/*	This is a time expensive query,
 *	~3 to 6 seconds on large chroms full of data	*/
safef(query, ArraySize(query),
    "SELECT span from %s where chrom = '%s' group by span", table, chrom);

sr = sqlMustGetResult(conn,query);
while ((row = sqlNextRow(sr)) != NULL)
    {   
    char spanName[128];
    unsigned span = sqlUnsigned(row[0]);

    safef(spanName, ArraySize(spanName), "%u", span);
    el = hashLookup(spans, spanName);
    if ( el == NULL)
	{
	if (span > maxSpan) maxSpan = span;
	if (span < minSpan) minSpan = span;
	++spanCount;
	hashAddInt(spans, spanName, span);
	}
    }
sqlFreeResult(&sr);

spanInUse = minSpan;

if (withLeftLabels)
	insideX += hgDefaultLeftLabelWidth + hgDefaultGfxBorder;

insideWidth = pixWidth - insideX - hgDefaultGfxBorder;

basesPerPixel = (winEnd - winStart) / insideWidth;
cookie = hashFirst(spans);

while ((el = hashNext(&cookie)) != NULL)
    {
    int span = sqlSigned(el->name);
    
    if ((float) span <= basesPerPixel) 
	spanInUse = span;
    }

return spanInUse;
}	/*	int spanInUse()	*/

static char *currentFile = (char *) NULL;	/* the binary file name */
static FILE *wibFH = (FILE *) NULL;		/* file handle to binary file */

static void openWibFile(char *fileName)
/*  smart open of the .wib file - only if it isn't already open */
{
if (currentFile)
    {
    if (differentString(currentFile,fileName))
	{
	if (wibFH != (FILE *) NULL)
	    {
	    fclose(wibFH);
	    wibFH = (FILE *) NULL;
	    freeMem(currentFile);
	    currentFile = (char *) NULL;
	    }
	currentFile = cloneString(fileName);
	if (differentString(currentFile,fileName))
	    errAbort("openWibFile: currentFile != fileName %s != %s",
		currentFile, fileName );
	wibFH = mustOpen(currentFile, "r");
	}
    }
else
    {
    currentFile = cloneString(fileName);
    if (differentString(currentFile,fileName))
	errAbort("openWibFile: currentFile != fileName %s != %s",
	    currentFile, fileName );
    wibFH = mustOpen(currentFile, "r");
    }
}

static void closeWibFile()
/*	smart close of .wib file - close only if open	*/
{
if (wibFH != (FILE *) NULL)
    {
    fclose(wibFH);
    wibFH = (FILE *) NULL;
    freeMem(currentFile);
    currentFile = (char *) NULL;
    }
}

static struct wiggleData * wigReadDataRow(struct wiggle *wiggle,
    int winStart, int winEnd, int tableId, boolean summaryOnly,
	boolean (*wiggleCompare)(int tableId, double value,
	    boolean summaryOnly, struct wiggle *wiggle))
/*  read one row of wiggle data, return data values between winStart, winEnd */
{
unsigned char *readData = (unsigned char *) NULL;
size_t itemsRead = 0;
int dataOffset = 0;
int noData = 0;
struct wiggleData *ret = (struct wiggleData *) NULL;
struct wiggleDatum *data = (struct wiggleDatum *)NULL;
struct wiggleDatum *dataPtr = (struct wiggleDatum *)NULL;
unsigned chromPosition = 0;
unsigned validCount = 0;
long int chromStart = -1;
unsigned chromEnd = 0;
double lowerLimit = 1.0e+300;
double upperLimit = -1.0e+300;
double sumData = 0.0;
double sumSquares = 0.0;

if (summaryOnly)
    {
    boolean takeIt = TRUE;
    /*	the 0.0 argument is unused in this case of
     *	summaryOnly in wiggleCompare
     */
    if (wiggleCompare)
	takeIt = (*wiggleCompare)(tableId, 0.0, summaryOnly, wiggle);
    if (takeIt)
	{ 
	upperLimit = wiggle->lowerLimit + wiggle->dataRange;
	lowerLimit = wiggle->lowerLimit;
	chromStart = wiggle->chromStart;
	chromEnd = wiggle->chromEnd;
	validCount = wiggle->validCount;
	sumData = wiggle->sumData;
	sumSquares = wiggle->sumSquares;
	} 
    }
else
    {
    openWibFile(wiggle->file);
    fseek(wibFH, wiggle->offset, SEEK_SET);
    readData = (unsigned char *) needMem((size_t) (wiggle->count + 1));


    itemsRead = fread(readData, (size_t) wiggle->count,
	    (size_t) sizeof(unsigned char), wibFH);
    if (itemsRead != sizeof(unsigned char))
	errAbort("wigReadDataRow: can not read %u bytes from %s at offset %u",
	    wiggle->count, wiggle->file, wiggle->offset);

    /*	need at most this amount, perhaps less
     *	this data area goes with the result, this is freed by wigFreeData,
     *	as well as the rest of the business that goes with it in the
     *	returned structure */
    data = (struct wiggleDatum *) needMem((size_t)
	(sizeof(struct wiggleDatum)*wiggle->validCount));

    dataPtr = data;
    chromPosition = wiggle->chromStart;

    for (dataOffset = 0; dataOffset < wiggle->count; ++dataOffset)
	{
	unsigned char datum = readData[dataOffset];
	if (datum == WIG_NO_DATA)
	    ++noData;
	else
	    {
	    if (chromPosition >= winStart && chromPosition < winEnd)
		{
		double value =
		    BIN_TO_VALUE(datum,wiggle->lowerLimit,wiggle->dataRange);
		boolean takeIt = TRUE;
		if (wiggleCompare)
		    takeIt = (*wiggleCompare)(tableId, value, summaryOnly,
			    wiggle);
		if (takeIt)
		    { 
		    dataPtr->chromStart = chromPosition;
		    dataPtr->value = value;
		    ++validCount;
		    if (validCount > wiggle->count)
		errAbort("wigReadDataRow: validCount > wiggle->count %u > %u",
				validCount, wiggle->count);

		    if (chromStart < 0)
			chromStart = chromPosition;
		    chromEnd = chromPosition + wiggle->span;
		    if (lowerLimit > dataPtr->value)
			lowerLimit = dataPtr->value;
		    if (upperLimit < dataPtr->value)
			upperLimit = dataPtr->value;
		    sumData += dataPtr->value;
		    sumSquares += dataPtr->value * dataPtr->value;
		    ++dataPtr;
		    }
		}
	    }
	chromPosition += wiggle->span;
	}

    freeMem(readData);
    }

if (validCount)
    {
    double dataRange = upperLimit - lowerLimit;
    AllocVar(ret);

     /*	this ret structure is freed by wigFreeData */
    ret->next = (struct wiggleData *) NULL;
    ret->chrom = wiggle->chrom;
    ret->chromStart = chromStart;
    ret->chromEnd = chromEnd;
    ret->span = wiggle->span;
    ret->count = validCount;
    ret->lowerLimit = lowerLimit;
    ret->dataRange = dataRange;
    ret->sumData = sumData;
    ret->sumSquares = sumSquares;
    ret->data = data;
    }
else
    {
    freeMem(data);
    }

return (ret);	/* may be null if validCount < 1	*/
}	/*	static struct wiggleData * wigReadDataRow()	*/

static void wigFree(struct wiggleData **pEl)
/* Free a single dynamically allocated wiggle data item */
{
struct wiggleData *el;
if ((el = *pEl) == NULL) return;
freeMem(el->data);
freez(pEl);
/*	The el->chrom is not freed, it may belong to something else
 *	since it is not a copy
 */
}

void wigFreeData(struct wiggleData **pList)
/* free a list of dynamically allocated wiggle data items */
{
struct wiggleData *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    wigFree(&el);
    }
*pList = NULL;
}

static struct bed *bedElement(char *chrom, unsigned start, unsigned end,
	char *table, unsigned lineCount)
{
struct bed *bed;
char name[128];

AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = start;
bed->chromEnd = end;
snprintf(name, sizeof(name), "%s.%u",
    chrom, lineCount);
bed->name = cloneString(name);
return bed;
}

struct wigStatsAcc
/* statistic accumulators */
    {
    unsigned span;	/* span of this data */
    unsigned start;	/* beginning chrom position */
    unsigned end;	/* end chrom position */
    double lowerLimit;	/* lowest data value in this block */
    double upperLimit;	/* highest data value in this block */
    double sumData;	/* sum of data values in this block */
    double sumSquares;	/* sum of data values squared */
    unsigned count;	/* number of data values in this statistic */
    unsigned bedElStart;	/*	bed element start coord	*/
    unsigned bedElEnd;		/*	bed element end coord	*/
    unsigned bedElCount;	/*	bed element count	*/
    };

static struct wigStatsAcc wigStatsAcc;

static void resetStats(struct wigStatsAcc *wsa)
{
wsa->span = 0;
wsa->start = BIGNUM;
wsa->end = 0;
wsa->lowerLimit = 1.0e+300;
wsa->upperLimit = -1.0e+300;
wsa->count = 0;
wsa->sumData = 0.0;
wsa->sumSquares = 0.0;
wsa->bedElStart = 0;
wsa->bedElEnd = 0;
wsa->bedElCount = 0;
}

static void returnStats(struct wigStatsAcc *wsa, struct wiggleStats **wsList,
    char *chrom, unsigned start, unsigned end, int span)
{
if (wsa->count)
    {
    struct wiggleStats *wsEl = (struct wiggleStats *) NULL;
    double mean = wsa->sumData / (double)wsa->count;
    double variance = 0.0;
    double stddev = 0.0;
    if (wsa->count > 1)
	{
	variance = (wsa->sumSquares -
	    ((wsa->sumData*wsa->sumData)/(double)wsa->count)) /
		((double)(wsa->count-1));
	if (variance > 0.0)
	    stddev = sqrt(variance);
	}
    AllocVar(wsEl);
    wsEl->chrom = cloneString(chrom);
    wsEl->chromStart = start;
    wsEl->chromEnd = end;
    wsEl->span = span;
    wsEl->count = wsa->count;
    wsEl->lowerLimit = wsa->lowerLimit;
    wsEl->dataRange = wsa->upperLimit - wsa->lowerLimit;
    wsEl->mean = mean;
    wsEl->variance = variance;
    wsEl->stddev = stddev;
    slAddHead(wsList,wsEl);
    }
}


static void accumStats(struct wigStatsAcc *ws, struct wiggleData *wdPtr,
    struct bed **bedList, unsigned maxBedElements, char *table)
{
boolean createBedList = FALSE;

if ((struct bed **)NULL != bedList)
    createBedList = TRUE;

ws->sumData += wdPtr->sumData;
ws->sumSquares += wdPtr->sumSquares;
ws->count += wdPtr->count;
if (wdPtr->lowerLimit < ws->lowerLimit)
    ws->lowerLimit = wdPtr->lowerLimit;
if (wdPtr->lowerLimit+wdPtr->dataRange > ws->upperLimit)
    ws->upperLimit = wdPtr->lowerLimit+wdPtr->dataRange;
if (wdPtr->chromStart < ws->start)
    ws->start = wdPtr->chromStart;
if (wdPtr->chromEnd > ws->end)
    ws->end = wdPtr->chromEnd;

if (createBedList && wdPtr->data)
    {
    struct wiggleDatum *wd = wdPtr->data;
    int i;
    for (i = 0; (i < wdPtr->count); ++i, ++wd)
	{
	if (wd->chromStart == ws->bedElEnd)
	    ws->bedElEnd += wdPtr->span;
	else if (wd->chromStart < ws->bedElEnd) /* do not start */
	    break;	/* over again (do not repeat for next span) */
	else if (ws->bedElCount > maxBedElements)
	    break;			/* too many */
	else
	    {
	    if (ws->bedElEnd > ws->bedElStart)
		{
		struct bed *bedEl;
		bedEl = bedElement(wdPtr->chrom, ws->bedElStart, ws->bedElEnd,
		    table, ++ws->bedElCount);
		slAddHead(bedList, bedEl);
		}
	    ws->bedElStart = wd->chromStart;
	    ws->bedElEnd = ws->bedElStart + wdPtr->span;
	    }
	}
    }

}	/*	static void accumStats()	*/

/*
 *	there are a variety of conditions that affect how FetchData is
 *	going to work.  This is an attempt to allow it to do as much
 *	as possible, but not get overloaded.
 *	summaryOnly is done when whole chrom summaries are requested
 *	for statistic purposes.  In those cases we do not need to go all
 *	the way to the data to get the averages, the SQL rows are good
 *	enough.  Although even on this level there is quite a bit of
 *	work to do on tracks such as Quality that have 180,000 rows on
 *	just chr1.
 *	a wiggleStats wsList is given when doing statistics, if it is
 *	purely a data fetch operation, there is no need to do
 *	wiggleStats and it will be a NULL pointer.
 *	a bedList pointer is given when a returned bed list is desired.
 *	In the case of processing a bedList, we honor the return limit
 *	of number of bed elements via the maxBedElements.
 *	If we are not returning a bedList and we are not doing a stats
 *	summary, then we have an honest data fetch operation, and in
 *	this case we honor the stated line limit of maxBedElements.
 *	When the caller is doing this data fetch operation and states
 *	that maxBedElements is zero, then we do all data that can be found.
 *	This would be the case for a stats operation when only one chrom
 *	is being worked on.
 */
struct wiggleData *wigFetchData(char *db, char *table, char *chromName,
    int winStart, int winEnd, boolean summaryOnly, boolean freeData,
	int tableId, boolean (*wiggleCompare)(int tableId, double value,
	    boolean summaryOnly, struct wiggle *wiggle),
		char *constraints, struct bed **bedList,
		    unsigned maxBedElements, struct wiggleStats **wsList)
/*  return linked list of wiggle data between winStart, winEnd */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
int rowOffset;
int rowCount = 0;
struct wiggle *wiggle;
struct hash *spans = NULL;      /* List of spans encountered during load */
char spanName[128];
char whereSpan[128];
char query[256];
struct hashEl *el;
int leastSpan = BIGNUM;
int mostSpan = 0;
int spanCount = 0;
int span = 0;
struct hashCookie cookie;
struct wiggleData *wigData = (struct wiggleData *) NULL;
struct wiggleData *wdList = (struct wiggleData *) NULL;
boolean bewareConstraints = FALSE;
boolean createBedList = FALSE;
boolean firstSpanDone = FALSE;
unsigned dataLimit = 0;
unsigned dataDone = 0;
boolean reachedDataLimit = FALSE;

/*	make sure table exists before we try to talk to it
 *	If it does not exist, we return a null result
 */
if (! sqlTableExists(conn, table))
    {
    hFreeConn(&conn);
    return((struct wiggleData *)NULL);
    }

if ((struct bed **)NULL != bedList)
    createBedList = TRUE;

/*	if we are not doing a summary (== return all data) and
 *	we are not creating a bed list, then obey the limit requested
 *	It will be zero if they really want everything.
 */
if (!summaryOnly && !createBedList)
    dataLimit = maxBedElements;

spans = newHash(0);	/*	a listing of all spans found here	*/

resetStats(&wigStatsAcc);	/*	zero everything	*/


/*	Are the constraints going to interfere with our span search ? */
if (constraints)
    {
    char *c = cloneString(constraints);
    tolowers(c);
    if (stringIn("span",c))
	bewareConstraints = TRUE;
    }

if (bewareConstraints)
    snprintf(query, sizeof(query),
	"SELECT span from %s where chrom = '%s' AND %s group by span",
	table, chromName, constraints );
else
    snprintf(query, sizeof(query),
	"SELECT span from %s where chrom = '%s' group by span",
	table, chromName );

/*	Survey the spans to see what the story is here */

sr = sqlMustGetResult(conn,query);
while ((row = sqlNextRow(sr)) != NULL)
{
    unsigned span = sqlUnsigned(row[0]);

    ++rowCount;

    snprintf(spanName, sizeof(spanName), "%u", span);
    el = hashLookup(spans, spanName);
    if ( el == NULL)
	{
	if (span > mostSpan) mostSpan = span;
	if (span < leastSpan) leastSpan = span;
	++spanCount;
	hashAddInt(spans, spanName, span);
	}

    }
sqlFreeResult(&sr);

/*	Now, using that span list, go through each span, fetching data	*/
cookie = hashFirst(spans);
while ((! reachedDataLimit) && (el = hashNext(&cookie)) != NULL)
    {
    if ((struct wiggleStats **)NULL != wsList)
	returnStats(&wigStatsAcc,wsList,chromName,winStart,winEnd,span);

    resetStats(&wigStatsAcc);

    if (bewareConstraints)
	{
	snprintf(whereSpan, sizeof(whereSpan), "((span = %s) AND %s)", el->name,
	    constraints);
	}
    else
	snprintf(whereSpan, sizeof(whereSpan), "span = %s", el->name);

    span = atoi(el->name);

    sr = hOrderedRangeQuery(conn, table, chromName, winStart, winEnd,
        whereSpan, &rowOffset);

    rowCount = 0;
    while ((! reachedDataLimit) && (row = sqlNextRow(sr)) != NULL)
	{
	++rowCount;
	wiggle = wiggleLoad(row + rowOffset);
	if (wiggle->count > 0 && (! reachedDataLimit))
	    {
	    wigData = wigReadDataRow(wiggle, winStart, winEnd,
		    tableId, summaryOnly, wiggleCompare );
	    if (wigData)
		{
		if (firstSpanDone)
		    accumStats(&wigStatsAcc, wigData, (struct bed **)NULL,
			maxBedElements, table);
		else
		    accumStats(&wigStatsAcc, wigData, bedList,
			maxBedElements, table);
		dataDone += wigData->count;
		if (freeData)
		    {
		    freeMem(wigData->data); /* and mark it gone */
		    wigData->data = (struct wiggleDatum *)NULL;
		    }
		slAddHead(&wdList,wigData);
		if (!createBedList && dataLimit)
		    if (dataLimit < dataDone)
			reachedDataLimit = TRUE;
		if (createBedList && (wigStatsAcc.bedElCount > maxBedElements))
		    reachedDataLimit = TRUE;
		}
	    }
	}
	/*	perhaps last bed line	*/
    if (!firstSpanDone && createBedList &&
	(wigStatsAcc.bedElEnd > wigStatsAcc.bedElStart) && wigData)
	{
	struct bed *bedEl;
	bedEl = bedElement(wigData->chrom, wigStatsAcc.bedElStart,
	    wigStatsAcc.bedElEnd, table, ++wigStatsAcc.bedElCount);
	slAddHead(bedList, bedEl);
	}
    sqlFreeResult(&sr);
    firstSpanDone = TRUE;
    }
closeWibFile();

if (createBedList)
    slReverse(bedList);

/*	last stats calculation	*/
if ((struct wiggleStats **)NULL != wsList)
    returnStats(&wigStatsAcc,wsList,chromName,winStart,winEnd,span);

hFreeConn(&conn);

if (wdList != (struct wiggleData *)NULL)
	slReverse(&wdList);
/*	this wdList can be freed by wigFreeData */
return(wdList);
}	/*	struct wiggleData *wigFetchData()	*/
