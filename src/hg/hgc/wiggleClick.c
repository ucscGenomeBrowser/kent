/* Handle details pages for wiggle tracks. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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
#include "hdb.h"
#include "chromAlias.h"
#include "quickLift.h"
#include "htmshell.h"
#include "hubConnect.h"

static void liftedWiggleClick(struct trackDb *tdb);

void genericWiggleClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start)
/* Display details for Wiggle data tracks.
 *	conn may be NULL for custom tracks when from file */
{
char *chrom = cartString(cart, "c");
char table[HDB_MAX_TABLE_STRING];
unsigned span = 0;
struct wiggleDataStream *wds = wiggleDataStreamNew();
unsigned long long valuesMatched = 0;
struct histoResult *histoGramResult;
float *valuesArray = NULL;
size_t valueCount = 0;
struct customTrack *ct = NULL;
boolean isCustom = FALSE;
int operations = wigFetchStats;	/*	default operation */

if (quickLiftIsLifted(tdb) && quickLiftWigEnabled(cart))
    {
    liftedWiggleClick(tdb);
    return;
    }
// A wig in a quickLift hub written while browser.quickLiftWig was on, clicked with it off:
// the code below would report the table of the same name in this assembly.  hgTracks draws
// nothing for such a track.  A wigMaf's wiggles are not hub tracks and keep the path below.
if (quickLiftIsLifted(tdb) && isHubTrack(tdb->track))
    {
    printf("<P>Lifted wig tracks are not enabled on this server.</P>\n");
    return;
    }

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
    if (!hFindSplitTable(database, seqName, tdb->table, table, sizeof table, NULL))
	errAbort("track %s not found", tdb->table);
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

    // printWigHistogram is a copy of the histogram part of this block for a quickLifted wig
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

    if (valuesMatched < 25)
	{
	struct wigAsciiData *asciiData = NULL;
        unsigned long long valuesDone = 0;
	double worstCaseResolution = 0.0;

	struct dyString *tableData = dyStringNew(256);
	for (asciiData = wds->ascii; asciiData && (valuesDone < valuesMatched);
	    asciiData = asciiData->next)
	    {
	    if (asciiData->count)
		{
		double resolution = asciiData->dataRange / (MAX_WIG_VALUE+1);
		if (resolution > worstCaseResolution)
		    worstCaseResolution = resolution;
		struct asciiDatum *data = asciiData->data;
		unsigned i = 0;
		for (;(i < asciiData->count)&&(valuesDone < valuesMatched); ++i)
		    {
		    dyStringPrintf(tableData, "<tr>");
		    dyStringPrintf(tableData, "<td>%s</td>", asciiData->chrom);
		    dyStringPrintf(tableData, "<td>%d</td>", data->chromStart);
		    dyStringPrintf(tableData, "<td>%d</td>", data->chromStart+asciiData->span);
		    dyStringPrintf(tableData, "<td align=right>%.3f</td>", data->value);
		    dyStringPrintf(tableData, "</tr>\n");
		    ++data;
		    ++valuesDone;
		    }
		}
	    }
        printf("<table class='stdTbl'>\n");
        printf("<thead>\n");
	printf("<tr><th colspan=4>%llu data values in this window view</th></tr>\n", valuesMatched);
	printf("<tr><th>chromosome</th><th>chromStart</th><th>chromEnd</th><th>dataValue</th></tr>\n");
        printf("</thead>\n");
        printf("<tbody>\n");
	printf("%s", dyStringCannibalize(&tableData));
        printf("</tbody>\n");
        printf("<tfoot>\n");
	printf("<tr><td colspan=4>compressed data has resolution of + or - %.3f</td></tr>\n", worstCaseResolution);
        printf("</tfoot>\n");
        printf("</table>\n");
	}
    else
	{
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
	}
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
// intervalStatsPrint is a copy of the printing below for the quickLifted pages
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

static void printWigHistogram(struct trackDb *tdb, float *values, size_t count,
                              double dataLow, double dataRange)
/* Print a histogram of values.  It uses the range on the track's type line when the data
 * have no range of their own (all one value), else dataLow to dataLow+dataRange.  A copy of
 * the histogram block in genericWiggleClick, used only for a quickLifted wig so that block is
 * unchanged with browser.quickLiftWig off;  a fix to one belongs in the other.  When the gate
 * is retired, genericWiggleClick can call this. */
{
char *words[16];
char *dupe = cloneString(tdb->type);
int wordCount = chopLine(dupe, words);
double minY, maxY, tDbMinY, tDbMaxY;
wigFetchMinMaxY(tdb, &minY, &maxY, &tDbMinY, &tDbMaxY, wordCount, words);
float hMin = min(minY,tDbMinY);
float hMax = max(maxY,tDbMaxY);
float hRange = hMax - hMin;

/* let's see if we really want to use the range from the track type
 *	line, or the actual range in this data.  If there is a good
 *	actual range in the data, use that instead
 */
if ((hRange > 0.0) && (dataRange != 0))
    hRange = 0.0;

/*	If we have a valid range, use a specified 20 bin histogram
 *	NOTE: pass 21 as binCount to get a 20 bin histogram
 */
struct histoResult *histoGramResult;
if (hRange > 0.0)
    histoGramResult = histoGram(values, count, (hRange/20.0),
        (unsigned) 21, hMin, hMin, hMax, (struct histoResult *)NULL);
else
    histoGramResult = histoGram(values, count,
        NAN, (unsigned) 0, NAN, (float) dataLow,
            (float) (dataLow + dataRange),
                (struct histoResult *)NULL);

/*	histoGram() may return NULL if it doesn't work, that's OK, the
 *	print out will indicate no results  (TRUE == html output)
 */
printHistoGram(histoGramResult, TRUE);
freeHistoGram(&histoGramResult);
freeMem(dupe);
}

struct intervalStats
/* Running totals for the statistics report on a set of intervals. */
    {
    bits64 count;			/* Number of intervals. */
    bits64 totalSize;			/* Bases they cover. */
    bits32 biggestSize, smallestSize;	/* Longest and shortest. */
    double sum, sumSquares;		/* Of the values. */
    double minVal, maxVal;		/* Smallest and largest value. */
    };

static void intervalStatsInit(struct intervalStats *stats)
/* Start a set of running totals. */
{
ZeroVar(stats);
stats->smallestSize = BIGNUM;
}

static void intervalStatsAdd(struct intervalStats *stats, bits32 start, bits32 end, double val)
/* Add one interval to the running totals. */
{
if (stats->count == 0)
    stats->minVal = stats->maxVal = val;
stats->count += 1;
bits32 size = end - start;
stats->totalSize += size;
if (stats->biggestSize < size)
    stats->biggestSize = size;
if (stats->smallestSize > size)
    stats->smallestSize = size;
stats->sum += val;
stats->sumSquares += val * val;
if (stats->minVal > val)
    stats->minVal = val;
if (stats->maxVal < val)
    stats->maxVal = val;
}

static void intervalStatsPrint(struct intervalStats *stats, bits32 start, bits32 end)
/* Write out little statistical report in HTML from running totals with at least one
 * interval in them.  The printing half of bbiIntervalStatsReport, copied for the quickLifted
 * pages so that function is unchanged with browser.quickLiftWig off;  a fix to one belongs in
 * the other.  When the gate is retired, bbiIntervalStatsReport can add to intervalStats and
 * call this. */
{
char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */
sprintLongWithCommas(num1Buf, stats->count);
sprintLongWithCommas(num2Buf, stats->totalSize);
bits32 winSize = end-start;
printf("<B>Statistics on:</B> %s <B>items covering</B> %s bases (%4.2f%% coverage)<BR>\n",
	num1Buf, num2Buf, 100.0*stats->totalSize/winSize);
printf("<B>Average item spans</B> %4.2f <B>bases.</B> ",
       (double)stats->totalSize/stats->count);
if (stats->biggestSize != stats->smallestSize)
    {
    sprintLongWithCommas(num1Buf, stats->smallestSize);
    sprintLongWithCommas(num2Buf, stats->biggestSize);
    printf("<B>Minimum span</B> %s <B>maximum span</B> %s", num1Buf, num2Buf);
    }
printf("<BR>\n");

printf("<B>Average value</B> %g <B>min</B> %g <B>max</B> %g <B> standard deviation </B> %g<BR>\n",
	stats->sum/stats->count, stats->minVal, stats->maxVal,
	calcStdFromSums(stats->sum, stats->sumSquares, stats->count));
}

static int bbiIntervalCmpStart(const void *va, const void *vb)
/* Compare two bbiIntervals by start. */
{
const struct bbiInterval *a = *((struct bbiInterval **)va);
const struct bbiInterval *b = *((struct bbiInterval **)vb);
return (int)a->start - (int)b->start;
}

struct liftedValue
/* A value of a quickLifted track, in the coordinates of the assembly it came from. */
    {
    bits32 start, end;	/* Half open. */
    float val;		/* Wig and bigWig values are floats to begin with. */
    };

static int liftedValueCmp(const void *va, const void *vb)
/* Compare two liftedValues by start. */
{
const struct liftedValue *a = va;
const struct liftedValue *b = vb;
return (a->start > b->start) - (a->start < b->start);
}

typedef struct liftedValue *(*LiftedFetcher)(void *context, char *chrom, int start, int end,
                                             int *retCount);
/* Fetch the values in chrom:start-end of the assembly a track came from, as an array in
 * order that the caller frees.  A value may reach past start or end. */

#define LIFTED_FEW_VALUES 25	/* Fewer than this are listed rather than shown as a histogram */

struct liftedSink
/* What becomes of the values of a quickLifted track once they are on the reference. */
    {
    struct intervalStats stats;		/* Running totals for the report. */
    boolean keepValues;			/* Keep each value for a histogram. */
    float *values;			/* The values, if kept. */
    size_t valueCount, valueAlloc;	/* Used and allocated size of values. */
    struct bbiInterval *few;		/* The first LIFTED_FEW_VALUES, for a short list. */
    struct lm *lm;			/* Where few is allocated. */
    };

static void liftedSinkInit(struct liftedSink *sink, boolean keepValues)
/* Start an empty sink. */
{
ZeroVar(sink);
intervalStatsInit(&sink->stats);
sink->keepValues = keepValues;
sink->lm = lmInit(0);
}

static void liftedSinkFree(struct liftedSink *sink)
/* Free what a sink holds. */
{
freez(&sink->values);
lmCleanup(&sink->lm);
}

static void liftedSinkAdd(struct liftedSink *sink, bits32 start, bits32 end, float val)
/* Add a value that has landed on the reference. */
{
if (sink->stats.count < LIFTED_FEW_VALUES)
    {
    struct bbiInterval *iv;
    lmAllocVar(sink->lm, iv);
    iv->start = start;
    iv->end = end;
    iv->val = val;
    slAddHead(&sink->few, iv);
    }
intervalStatsAdd(&sink->stats, start, end, val);
if (sink->keepValues)
    {
    if (sink->valueCount == sink->valueAlloc)
        {
        size_t newAlloc = (sink->valueAlloc == 0) ? 1024 : 2 * sink->valueAlloc;
        sink->values = needLargeMemResize(sink->values, newAlloc * sizeof(float));
        sink->valueAlloc = newAlloc;
        }
    sink->values[sink->valueCount++] = val;
    }
}

static int liftedValueFirstEndingAfter(struct liftedValue *array, int count, int pos)
/* The index of the first value in array that ends after pos, or count if there is none.
 * The values are in order and do not overlap. */
{
int lo = 0, hi = count;
while (lo < hi)
    {
    int mid = (lo + hi) / 2;
    if ((int)array[mid].end <= pos)
        lo = mid + 1;
    else
        hi = mid;
    }
return lo;
}

static void liftValues(struct liftedValue *array, int count, struct quickLiftRange *range,
                       struct quickLiftRange *pieceList, int start, int end,
                       struct liftedSink *sink)
/* Add the values in array, which are in order, to sink where the pieces put them on the
 * reference, clipped to range in the source and to start-end on the reference.  A piece at
 * a time:  the values each piece holds are found by a binary search.  A value cut by a block
 * edge is added once for each piece. */
{
struct quickLiftRange *piece;
for (piece = pieceList; piece != NULL; piece = piece->next)
    {
    int srcStart = max(piece->sourceStart, range->start);
    int srcEnd = min(piece->sourceStart + (piece->end - piece->start), range->end);
    int i;
    for (i = liftedValueFirstEndingAfter(array, count, srcStart);
         (i < count) && ((int)array[i].start < srcEnd); i++)
        {
        int segStart = max((int)array[i].start, srcStart);
        int segEnd = min((int)array[i].end, srcEnd);
        int refStart, refEnd;
        quickLiftPieceToReference(piece, segStart, segEnd, &refStart, &refEnd);
        refStart = max(refStart, start);
        refEnd = min(refEnd, end);
        if (refStart < refEnd)
            liftedSinkAdd(sink, refStart, refEnd, array[i].val);
        }
    }
}

static void liftedValuesToSink(struct trackDb *tdb, char *chrom, int start, int end,
                               LiftedFetcher fetch, void *context, struct liftedSink *sink)
/* Add the values of a quickLifted track that land in chrom:start-end on the reference to
 * sink, each where the chain puts it.  fetch reads them from the assembly the track came
 * from, one source range at a time, so only one range's values are held at once. */
{
char *quickLiftFile = trackDbSetting(tdb, "quickLiftUrl");
struct hash *chainHash = newHash(8);
struct hash *blockCache = newHash(8);
struct quickLiftRange *range, *rangeList = quickLiftSourceRangesMerged(quickLiftFile, chrom,
                                                                       start, end, chainHash);
for (range = rangeList; range != NULL; range = range->next)
    {
    struct quickLiftRange *pieceList = quickLiftMapToReferenceIn(chainHash, blockCache,
                                    range->chrom, range->start, range->end, chrom, start, end);
    if (pieceList == NULL)
        continue;
    int count = 0;
    struct liftedValue *array = fetch(context, range->chrom, range->start, range->end, &count);
    // room for this range's values up front, rather than doubling past what is needed
    if (sink->keepValues && (sink->valueCount + count > sink->valueAlloc))
        {
        sink->valueAlloc = sink->valueCount + count;
        sink->values = needLargeMemResize(sink->values, sink->valueAlloc * sizeof(float));
        }
    liftValues(array, count, range, pieceList, start, end, sink);
    freeMem(array);
    quickLiftRangeListFree(&pieceList);
    }
quickLiftRangeListFree(&rangeList);
quickLiftBlockCacheFree(&blockCache);
}

struct liftedWigContext
/* What fetchWigValues needs to read a table wig in the assembly a track came from. */
    {
    struct trackDb *tdb;
    char *db;
    char *table;
    struct sqlConnection *conn;
    };

static struct liftedValue *fetchWigValues(void *context, char *chrom, int start, int end,
                                          int *retCount)
/* A LiftedFetcher for a table wig:  the values at its minimum span. */
{
struct liftedWigContext *wig = context;
*retCount = 0;
char splitTable[HDB_MAX_TABLE_STRING];
if (!hFindSplitTable(wig->db, chrom, wig->table, splitTable, sizeof splitTable, NULL))
    return NULL;
// minSpan remembers the table and chrom it was last given, so give it ones that last
int span = max(1, minSpan(wig->conn, cloneString(splitTable), cloneString(chrom), start, end, cart,
                          wig->tdb));
struct wiggleDataStream *wds = wiggleDataStreamNew();
wds->setSpanConstraint(wds, span);
wds->setChromConstraint(wds, chrom);
// getData keeps only the values that lie wholly inside its window, so widen it to take in
// the ones that cross the edge; liftValues clips them to the range
wds->setPositionConstraint(wds, max(0, start - span + 1), end + span - 1);
wds->getData(wds, wig->db, splitTable, wigFetchAscii);

slReverse(&wds->ascii);	// built head first, so the rows come back last first
struct wigAsciiData *ascii;
int count = 0;
for (ascii = wds->ascii; ascii != NULL; ascii = ascii->next)
    count += ascii->count;
struct liftedValue *array = NULL;
if (count > 0)
    {
    array = needLargeMem(count * sizeof(struct liftedValue));
    int i = 0;
    boolean inOrder = TRUE;
    for (ascii = wds->ascii; ascii != NULL; ascii = ascii->next)
        {
        unsigned j;
        for (j = 0; j < ascii->count; j++, i++)
            {
            array[i].start = ascii->data[j].chromStart;
            array[i].end = array[i].start + ascii->span;
            array[i].val = ascii->data[j].value;
            if ((i > 0) && (array[i].start < array[i-1].start))
                inOrder = FALSE;
            }
        freez(&ascii->data);	// copied, so drop it now rather than hold both at once
        }
    if (!inOrder)
        qsort(array, count, sizeof(struct liftedValue), liftedValueCmp);
    }
wiggleDataStreamFree(&wds);
*retCount = count;
return array;
}

static struct liftedValue *fetchBigWigValues(void *context, char *chrom, int start, int end,
                                             int *retCount)
/* A LiftedFetcher for a bigWig. */
{
struct lm *lm = lmInit(0);
struct bbiInterval *iv, *ivList = bigWigIntervalQuery((struct bbiFile *)context, chrom, start,
                                                      end, lm);
int count = slCount(ivList);
struct liftedValue *array = NULL;
if (count > 0)
    {
    array = needLargeMem(count * sizeof(struct liftedValue));
    int i = 0;
    for (iv = ivList; iv != NULL; iv = iv->next, i++)
        {
        array[i].start = iv->start;
        array[i].end = iv->end;
        array[i].val = iv->val;
        }
    }
lmCleanup(&lm);
*retCount = count;
return array;
}

static void printLiftedFrom(struct trackDb *tdb)
/* Say which assembly a quickLifted track's values came from. */
{
printf("<B>Lifted from: </B> %s<BR>\n", htmlEncode(trackDbSetting(tdb, "quickLiftDb")));
}

static void liftedWiggleClick(struct trackDb *tdb)
/* Display details for a quickLifted wig:  the values from the assembly it came from that
 * land in the window, where the chain puts them. */
{
char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */
sprintLongWithCommas(num1Buf, BASE_1(winStart));
sprintLongWithCommas(num2Buf, winEnd);
printf("<B>Position: </B> %s:%s-%s<BR>\n", seqName, num1Buf, num2Buf);
sprintLongWithCommas(num1Buf, winEnd-winStart);
printf("<B>Total Bases in view: </B> %s <BR>\n", num1Buf);
printLiftedFrom(tdb);

if ((winEnd - winStart) >= MAX_WINDOW_ALLOW_STATS)
    {
    printf("<P>Zoom in to a view less than %s bases to see data summary.</P>",
           MAX_WINDOW_ALLOW_STRING);
    return;
    }

struct liftedSink sink;
liftedSinkInit(&sink, TRUE);
struct liftedWigContext wig;
wig.tdb = tdb;
wig.db = trackDbSetting(tdb, "quickLiftDb");
quickLiftResolveTable(tdb, tdb->table, &wig.table, &wig.db);
if (quickLiftWigTableOk(wig.db, wig.table))
    {
    wig.conn = hAllocConn(wig.db);
    liftedValuesToSink(tdb, seqName, winStart, winEnd, fetchWigValues, &wig, &sink);
    hFreeConn(&wig.conn);
    }

if (sink.stats.count == 0)
    printf("<P>No data overlapping current position.</P>");
else
    {
    intervalStatsPrint(&sink.stats, winStart, winEnd);
    printf("<P>A value that the alignment splits is counted once for each part.</P>\n");
    if (sink.stats.count < LIFTED_FEW_VALUES)
        {
        struct bbiInterval *iv;
        slSort(&sink.few, bbiIntervalCmpStart);
        printf("<table class='stdTbl'>\n");
        printf("<thead>\n");
        printf("<tr><th colspan=4>%llu data values in this window view</th></tr>\n",
               (unsigned long long)sink.stats.count);
        printf("<tr><th>chromosome</th><th>chromStart</th><th>chromEnd</th>"
               "<th>dataValue</th></tr>\n");
        printf("</thead>\n");
        printf("<tbody>\n");
        for (iv = sink.few; iv != NULL; iv = iv->next)
            printf("<tr><td>%s</td><td>%d</td><td>%d</td><td align=right>%.3f</td></tr>\n",
                   seqName, (int)iv->start, (int)iv->end, iv->val);
        printf("</tbody>\n");
        printf("</table>\n");
        }
    else
        printWigHistogram(tdb, sink.values, sink.valueCount, sink.stats.minVal,
                          sink.stats.maxVal - sink.stats.minVal);
    }
liftedSinkFree(&sink);
}

static void liftedBigWigClick(struct trackDb *tdb, char *fileName)
/* Display details for a quickLifted bigWig:  the values from the assembly it came from that
 * land in the window, where the chain puts them.  Parallels bigWigClick, which keeps master's
 * code with browser.quickLiftWig off;  a change to the page belongs in both. */
{
char *maxWinToQuery = trackDbSettingClosestToHome(tdb, "maxWindowToQuery");
unsigned maxWTQ = 0;
if (isNotEmpty(maxWinToQuery))
    maxWTQ = sqlUnsigned(maxWinToQuery);

struct bbiFile *bbi = NULL;
struct liftedSink sink;
liftedSinkInit(&sink, FALSE);
if ((maxWinToQuery == NULL) || (maxWTQ > winEnd-winStart))
    {
    // the file is in the coordinates of the assembly it came from
    bbi = bigWigFileOpenAlias(fileName, chromAliasFindAliases);
    liftedValuesToSink(tdb, seqName, winStart, winEnd, fetchBigWigValues, bbi, &sink);
    }

char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */
sprintLongWithCommas(num1Buf, BASE_1(winStart));
sprintLongWithCommas(num2Buf, winEnd);
printf("<B>Position: </B> %s:%s-%s<BR>\n", seqName, num1Buf, num2Buf);
sprintLongWithCommas(num1Buf, winEnd-winStart);
printf("<B>Total Bases in view: </B> %s <BR>\n", num1Buf);
printLiftedFrom(tdb);

if (sink.stats.count > 0)
    {
    intervalStatsPrint(&sink.stats, winStart, winEnd);
    printf("<P>A value that the alignment splits is counted once for each part.</P>\n");
    }
else if (bbi == NULL)
    {
    sprintLongWithCommas(num1Buf, maxWTQ);
    printf("<P>Zoom in to a view less than %s bases to see data summary.</P>",num1Buf);
    }
else
    printf("<P>No data overlapping current position.</P>");

liftedSinkFree(&sink);
bbiFileClose(&bbi);
}

static void bigWigClick(struct trackDb *tdb, char *fileName)
/* Display details for BigWig data tracks. */
{
if (quickLiftIsLifted(tdb) && quickLiftWigEnabled(cart))
    {
    liftedBigWigClick(tdb, fileName);
    return;
    }

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
    // this needs to deal with quickLift
    bbi = bigWigFileOpenAlias(fileName, chromAliasFindAliases);
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
    sqlSafef(query, sizeof(query), "select fileName from %s", tdb->table);
    fileName = sqlQuickString(conn, query);
    if (fileName == NULL)
	errAbort("Missing fileName in %s table", tdb->table);
    }
bigWigClick(tdb, hReplaceGbdb(fileName)); // tiny memory leak
}

void bigWigCustomClick(struct trackDb *tdb)
/* Display details for BigWig custom tracks. */
{
bigWigClick(tdb, trackDbSetting(tdb, "bigDataUrl"));
}
