/* Handle details pages for wiggle tracks. */

#include "common.h"
#include "wiggle.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "obscure.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: wiggleClick.c,v 1.28 2008/09/03 19:19:09 markd Exp $";

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

if (startsWith("ct_", tdb->tableName))
    {
    ct = lookupCt(tdb->tableName);
    if (!ct)
        {
        warn("<P>wiggleClick: can not find custom wiggle track '%s'</P>", tdb->tableName);
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
    hFindSplitTable(database, seqName, tdb->tableName, table, &hasBin);
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

/*	If our window is less than 1000000 points, we can do
 *	the histogram too.
 */
if ((winEnd - winStart) < 1000001)
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


if ((winEnd - winStart) < 1000001)
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
    printf("<P>(viewing windows of fewer than 1,000,000 bases will also"
	" display a histogram)</P>\n");
    }

wiggleDataStreamFree(&wds);
}
