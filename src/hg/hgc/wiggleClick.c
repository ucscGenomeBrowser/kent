/* Handle details pages for wiggle tracks. */

#include "common.h"
#include "wiggle.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "obscure.h"
#include "histogram.h"

void genericWiggleClick(struct sqlConnection *conn, struct trackDb *tdb, 
	char *item, int start)
/* Display details for MAF tracks. */
{
char *chrom = cartString(cart, "c");
char table[64];
boolean hasBin;
int span = 0;
struct wiggleDataStream *wDS = newWigDataStream();
unsigned long long valuesMatched = 0;
struct histoResult *histoGramResult;
float *valuesArray = NULL;
float *fptr = NULL;
size_t valueCount = 0;
struct wigAsciiData *asciiData = NULL;
char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */

hFindSplitTable(seqName, tdb->tableName, table, &hasBin);

span = spanInUse(conn, table, chrom, winStart, winEnd, cart);

/*	if for some reason we don't have a chrom and win positions, this
 *	should be run in a loop that does one chrom at a time.  In the
 *	case of hgc, there seems to be a chrom and a position.
 */
wDS->setSpanConstraint(wDS, span);
wDS->setChromConstraint(wDS, chrom);
wDS->setPositionConstraint(wDS, winStart, winEnd);

/*	We want to also fetch the actual data values so we can run a
 *	histogram function on them.  You can't fetch the data in the
 *	form of the data array since the span information is then lost.
 *	We have to do the ascii data list format, and prepare that to
 *	send to the histogram function.
 */
valuesMatched = wDS->getData(wDS, database, table,
			wigFetchStats | wigFetchAscii );

sprintLongWithCommas(num1Buf, winStart + 1);
sprintLongWithCommas(num2Buf, winEnd);

printf ("<P><B> Position: </B> %s:%s-%s</P>\n", chrom, num1Buf, num2Buf );
sprintLongWithCommas(num1Buf, winEnd - winStart);
printf ("<P><B> Total Bases in view: </B> %s </P>\n", num1Buf);
if (valuesMatched == 0)
    {
    puts ("<P><B> no data found in this region </B></P>\n");
    }
else
    {
sprintLongWithCommas(num1Buf, wDS->stats->count * wDS->stats->span);
printf ("<P><B> Statistics on: </B> %s <B> bases </B> (%% %.4f coverage)</P>\n",
    num1Buf, 100.0*(wDS->stats->count * wDS->stats->span)/(winEnd - winStart));

/* For some reason BORDER=1 does not work in our web.c nested table
 * scheme.
 * So use web.c's trick of using an enclosing table to provide a border.  
 */
puts("<P><!--outer table is for border purposes-->" "\n"
     "<TABLE BGCOLOR=\"#"
	HG_COL_BORDER
	"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>");

/*	output statistics table	*/
wDS->statsOut(wDS, "stdout", TRUE, TRUE);
puts ("</TD></TR>\n");

/*	histoGram() may return NULL if it doesn't work	*/

AllocArray(valuesArray, valuesMatched);
fptr = valuesArray;

/*	the (valueCount <= valuesMatched) condition is for safety */
for (asciiData = wDS->ascii; asciiData && (valueCount <= valuesMatched);
	asciiData = asciiData->next)
    {
    if (asciiData->count)
	{
	struct asciiDatum *data;
	unsigned i;

	data = asciiData->data;
	for (i = 0; (i < asciiData->count)&&(valueCount <= valuesMatched); ++i)
	    {
	    *fptr++ = data->value;
	    ++data;
	    ++valueCount;
	    }
	}
    }

histoGramResult = histoGram(valuesArray, valueCount,
	    NAN, (unsigned) 0, NAN, (float) wDS->stats->lowerLimit,
		(float) (wDS->stats->lowerLimit + wDS->stats->dataRange),
		(struct histoResult *)NULL);

/*	global enclosing table row, add a blank line	*/
puts ("<TR><TD BGCOLOR=\""HG_COL_INSIDE"\"> &nbsp </TD></TR>\n");

/*	And then the row to enclose the histogram table	*/
puts ("<TR><TD BGCOLOR=\""HG_COL_INSIDE"\">\n");

puts ("<TABLE ALIGN=CENTER COLS=8 BGCOLOR=\""HG_COL_INSIDE"\" BORDER=1 HSPACE=0>\n");

printf ("<TR><TH ALIGN=CENTER COLSPAN=8> Histogram on %u values (zero count bins not shown)</TH></TR>\n",
	valueCount);
puts ("<TR><TH ALIGN=LEFT> bin </TH>\n");
puts ("    <TD COLSPAN=2 ALIGN=CENTER>\n");
puts ("      <TABLE WIDTH=100% ALIGN=CENTER COLS=2 BGCOLOR=\"");
puts (HG_COL_INSIDE"\" BORDER=0 HSPACE=0>\n");
puts ("        <TR><TH COLSPAN=2 ALIGN=CENTER> range </TH></TR>\n");
puts ("        <TR><TH ALIGN=LEFT> minimum </TH>\n");
puts ("              <TH ALIGN=RIGHT> maximum </TH></TR>\n");
puts ("      </TABLE>\n");
puts ("    </TD>\n");
puts ("    <TH ALIGN=CENTER> count </TH>\n");
puts ("    <TH ALIGN=CENTER> p Value </TH>\n");
puts ("    <TH ALIGN=CENTER> log2(p Value) </TH><TH ALIGN=CENTER> Cumulative <BR> Probability <BR> Distribution </TH>\n");
puts ("    <TH ALIGN=CENTER> 1.0 - CPD </TH></TR>\n");

if (histoGramResult)
    {
    boolean someDisplayed = FALSE;
    double cpd = 0.0;
    double log2_0 = log(2.0);
    int i;

    for (i=0; i < histoGramResult->binCount; ++i)
	{
	if (histoGramResult->binCounts[i] > 0)
	    {
	    double min, max, pValue;

	    min = ((double)i * histoGramResult->binSize) +
				histoGramResult->binZero;
	    max = min + histoGramResult->binSize;

	    printf ("<TR><TD ALIGN=LEFT> %d </TD>\n", i );
	    printf ("    <TD ALIGN=RIGHT> %g </TD><TD ALIGN=RIGHT> %g </TD>\n", min, max);
	    printf ("    <TD ALIGN=RIGHT> %u </TD>\n",
			    histoGramResult->binCounts[i] );
	    if (histoGramResult->binCounts[i] > 0)
		{
		pValue = (double) histoGramResult->binCounts[i] /
			    (double) histoGramResult->count;
		cpd += pValue;
		printf ("    <TD ALIGN=RIGHT> %g </TD>\n", pValue);
		printf ("    <TD ALIGN=RIGHT> %g </TD>\n", log(pValue)/log2_0);
		}
	    else
		{
		printf ("    <TD ALIGN=RIGHT> 0.0 </TD>\n");
		printf ("    <TD ALIGN=RIGHT> N/A </TD>\n");
		}

	    printf ("    <TD ALIGN=RIGHT> %g </TD>\n", cpd);
	    printf ("    <TD ALIGN=RIGHT> %g </TD></TR>\n", 1.0 - cpd);
	    someDisplayed = TRUE;
	    }
	}
    if (!someDisplayed)
	puts ("<TR><TD COLSPAN=8 ALIGN=CENTER> no data found for histogram </TD></TR>\n");
    }
else
    puts ("<TR><TD COLSPAN=8 ALIGN=CENTER> no data found for histogram </TD></TR>\n");

printf ("</TABLE>\n");

/*	finish out the global enclosing table	*/
puts ("</TD></TR></TABLE></P>\n");

freeHistoGram(&histoGramResult);
freeMem(valuesArray);
    }
destroyWigDataStream(&wDS);
}
