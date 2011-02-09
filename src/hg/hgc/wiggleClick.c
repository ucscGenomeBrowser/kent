/* Handle details pages for wiggle tracks. */

#include "common.h"
#include "wiggle.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "obscure.h"
#include "hmmstats.h"
#include "customTrack.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: wiggleClick.c,v 1.34 2010/05/11 01:43:29 kent Exp $";

void genericWiggleClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start)
/* Display details for Wiggle data tracks.
 *	conn may be NULL for custom tracks when from file */
{
char *chrom = cartString(cart, "c");
char table[64];
boolean hasBin;
unsigned span = 0;
struct wiggleDataStream *wds = wiggleDataStreamNew();
unsigned long long valuesMatched = 0;
struct histoResult *histoGramResult;
float *valuesArray = NULL;
size_t valueCount = 0;
struct customTrack *ct = NULL;
boolean isCustom = FALSE;
int operations = wigFetchStats;	/*	default operation */

if (startsWith("ct_", tdb->table))
    {
    ct = lookupCt(tdb->table);
    if (!ct)
        {
        warn("<P>wiggleClick: can not find custom wiggle track '%s'</P>", tdb->table);
        return;
        }
    if (! ct->wiggle)
        {
        warn("<P>wiggleClick: called to do stats on a custom track that isn't wiggle data ?</P>");
        return;
        }
    if (ct->dbTrack)
	{
	safef(table,ArraySize(table), "%s", ct->dbTableName);
	span = minSpan(conn, table, chrom, winStart, winEnd, cart, tdb);
	}
    else
	{
	safef(table,ArraySize(table), "%s", ct->wigFile);
	span = 0;	/*	cause all spans to be examined	*/
	}
    isCustom = TRUE;
    }
else
    {
    hFindSplitTable(database, seqName, tdb->table, table, &hasBin);
    /*span = spanInUse(conn, table, chrom, winStart, winEnd, cart);*/
    span = minSpan(conn, table, chrom, winStart, winEnd, cart, tdb);
    }

/*	if for some reason we don't have a chrom and win positions, this
 *	should be run in a loop that does one chrom at a time.  In the
 *	case of hgc, there seems to be a chrom and a position.
 */
wds->setSpanConstraint(wds, span);
wds->setChromConstraint(wds, chrom);
wds->setPositionConstraint(wds, winStart, winEnd);

/*	If our window is less than some number of points, we can do
 *	the histogram too.
 */
#define MAX_WINDOW_ALLOW_STATS	100000001
#define MAX_WINDOW_ALLOW_STRING	"100,000,000"
if ((winEnd - winStart) < MAX_WINDOW_ALLOW_STATS)
	operations |= wigFetchAscii;

/*	We want to also fetch the actual data values so we can run a
 *	histogram function on them.  You can't fetch the data in the
 *	form of the data array since the span information is then lost.
 *	We have to do the ascii data list format, and prepare that to
 *	send to the histogram function.
 */

if (isCustom)
    {
    if (ct->dbTrack)
	valuesMatched = wds->getData(wds, CUSTOM_TRASH, table, operations);
    else
	valuesMatched = wds->getData(wds, (char *)NULL, table, operations);
    }
else
    valuesMatched = wds->getData(wds, database, table, operations);

statsPreamble(wds, chrom, winStart, winEnd, span, valuesMatched, NULL);

/*	output statistics table
 *		(+sort, +html output, +with header, +close table)
 */
wds->statsOut(wds, database, "stdout", TRUE, TRUE, TRUE, FALSE);

if ((winEnd - winStart) < MAX_WINDOW_ALLOW_STATS)
    {
    char *words[16];
    int wordCount = 0;
    char *dupe = cloneString(tdb->type);
    double minY, maxY, tDbMinY, tDbMaxY;
    float hMin, hMax, hRange;

    wordCount = chopLine(dupe, words);

    wigFetchMinMaxY(tdb, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);
    hMin = min(minY,tDbMinY);
    hMax = max(maxY,tDbMaxY);
    hRange = hMax - hMin;

    /*	convert the ascii data listings to one giant float array 	*/
    valuesArray = wds->asciiToDataArray(wds, valuesMatched, &valueCount);

    /* let's see if we really want to use the range from the track type
     *	line, or the actual range in this data.  If there is a good
     *	actual range in the data, use that instead
     */
    if (hRange > 0.0) 
    	{
	if (wds->stats->dataRange != 0)
	    hRange = 0.0;
	}

    /*	If we have a valid range, use a specified 20 bin histogram
     *	NOTE: pass 21 as binCount to get a 20 bin histogram
     */
    if (hRange > 0.0)
	histoGramResult = histoGram(valuesArray, valueCount, (hRange/20.0),
	    (unsigned) 21, hMin, hMin, hMax, (struct histoResult *)NULL);
    else
	histoGramResult = histoGram(valuesArray, valueCount,
	    NAN, (unsigned) 0, NAN, (float) wds->stats->lowerLimit,
		(float) (wds->stats->lowerLimit + wds->stats->dataRange),
		    (struct histoResult *)NULL);

    /*	histoGram() may return NULL if it doesn't work, that's OK, the
     *	print out will indicate no results  (TRUE == html output)
     */
    printHistoGram(histoGramResult, TRUE);

    freeHistoGram(&histoGramResult);
    freeMem(valuesArray);
    }
else
    {
    printf("<P>(viewing windows of fewer than %s bases will also"
	" display a histogram)</P>\n", MAX_WINDOW_ALLOW_STRING);
    }

wiggleDataStreamFree(&wds);
}

void bbiIntervalStatsReport(struct bbiInterval *bbList, char *table, 
	char *chrom, bits32 start, bits32 end)
/* Write out little statistical report in HTML */
{
/* Loop through list and calculate some stats. */
bits64 iCount = 0;
bits64 iTotalSize = 0;
bits32 biggestSize = 0, smallestSize = BIGNUM;
struct bbiInterval *bb;
double sum = 0.0, sumSquares = 0.0;
double minVal = bbList->val, maxVal = bbList->val;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    iCount += 1;
    bits32 size = bb->end - bb->start;
    iTotalSize += size;
    if (biggestSize < size)
        biggestSize = size;
    if (smallestSize > size)
        smallestSize = size;
    double val = bb->val;
    sum += val;
    sumSquares += val * val;
    if (minVal > val)
        minVal = val;
    if (maxVal < val)
        maxVal = val;
    }

char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */
sprintLongWithCommas(num1Buf, iCount);
sprintLongWithCommas(num2Buf, iTotalSize);
bits32 winSize = end-start;
printf("<B>Statistics on:</B> %s <B>items covering</B> %s bases (%4.2f%% coverage)<BR>\n",
	num1Buf, num2Buf, 100.0*iTotalSize/winSize);
printf("<B>Average item spans</B> %4.2f <B>bases.</B> ", (double)iTotalSize/iCount);
if (biggestSize != smallestSize)
    {
    sprintLongWithCommas(num1Buf, smallestSize);
    sprintLongWithCommas(num2Buf, biggestSize);
    printf("<B>Minimum span</B> %s <B>maximum span</B> %s", num1Buf, num2Buf);
    }
printf("<BR>\n");

printf("<B>Average value</B> %g <B>min</B> %g <B>max</B> %g <B> standard deviation </B> %g<BR>\n",
	sum/iCount, minVal, maxVal, calcStdFromSums(sum, sumSquares, iCount));
}

static void bigWigClick(struct trackDb *tdb, char *fileName)
/* Display details for BigWig data tracks. */
{
char *chrom = cartString(cart, "c");

/* Open BigWig file and get interval list. */
struct bbiFile *bbi = NULL;
struct lm *lm = lmInit(0);
struct bbiInterval *bbList = NULL;
char *maxWinToQuery = trackDbSettingClosestToHome(tdb, "maxWindowToQuery");

unsigned maxWTQ = 0;
if (isNotEmpty(maxWinToQuery))
    maxWTQ = sqlUnsigned(maxWinToQuery);

if ((maxWinToQuery == NULL) || (maxWTQ > winEnd-winStart))
    {
    bbi = bigWigFileOpen(fileName);
    bbList = bigWigIntervalQuery(bbi, chrom, winStart, winEnd, lm);
    }

char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */
sprintLongWithCommas(num1Buf, BASE_1(winStart));
sprintLongWithCommas(num2Buf, winEnd);
printf("<B>Position: </B> %s:%s-%s<BR>\n", chrom, num1Buf, num2Buf );
sprintLongWithCommas(num1Buf, winEnd-winStart);
printf("<B>Total Bases in view: </B> %s <BR>\n", num1Buf);

if (bbList != NULL)
    {
    bbiIntervalStatsReport(bbList, tdb->table, chrom, winStart, winEnd);
    }
else if ((bbi == NULL) && (maxWTQ <= winEnd-winStart))
    {
    sprintLongWithCommas(num1Buf, maxWTQ);
    printf("<P>Zoom in to a view less than %s bases to see data summary.</P>",num1Buf);
    }
else
    {
    printf("<P>No data overlapping current position.</P>");
    }

lmCleanup(&lm);
bbiFileClose(&bbi);
}

void genericBigWigClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start)
/* Display details for BigWig built in tracks. */
{
char *fileName = trackDbSetting(tdb, "bigDataUrl");
if (fileName == NULL)
    {
    char query[256];
    safef(query, sizeof(query), "select fileName from %s", tdb->table);
    fileName = sqlQuickString(conn, query);
    if (fileName == NULL)
	errAbort("Missing fileName in %s table", tdb->table);
    }
bigWigClick(tdb, fileName);
}

void bigWigCustomClick(struct trackDb *tdb)
/* Display details for BigWig custom tracks. */
{
bigWigClick(tdb, trackDbSetting(tdb, "bigDataUrl"));
}
