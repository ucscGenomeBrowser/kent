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
int pixWidth = atoi(cartUsualString(cart, "pix", "620" ));
char *itemName = NULL;
boolean withLeftLabels = cartUsualBoolean(cart, "leftLabels", TRUE);
int insideWidth;
int insideX = hgDefaultGfxBorder;
char **row;
struct sqlResult *sr;
char query[256];
int minSpan = BIGNUM;
int maxSpan = 0;
int spanCount = 0;
struct hash *spans = newHash(0);	/*	list of spans in this table */
struct hashEl *el;
struct hashCookie cookie;
float basesPerPixel = 0.0;
int spanInUse = 0;
struct wiggleDataStream *wDS = newWigDataStream();
unsigned long long valuesMatched = 0;
struct histoResult *histoGramResult;
float *valuesArray = NULL;
float *fptr = NULL;
size_t valueCount = 0;
struct wigAsciiData *asciiData = NULL;
char num1Buf[64], num2Buf[64]; /* big enough for 2^64 (and then some) */

hFindSplitTable(seqName, tdb->tableName, table, &hasBin);

/*	We need to take this span finding business into the library */
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

if (item)
	itemName = cloneString(item);
else
	itemName = cloneString("No item string");

basesPerPixel = (winEnd - winStart) / insideWidth;
cookie = hashFirst(spans);

while ((el = hashNext(&cookie)) != NULL)
    {
    int span = sqlSigned(el->name);
    
    if ((float) span <= basesPerPixel) 
	spanInUse = span;
    }
/*	if for some reason we don't have a chrom and win positions, this
 *	should be run in a loop that does one chrom at a time.  In the
 *	case of hgc, there seems to be a chrom and a position.
 */
wDS->setSpanConstraint(wDS, spanInUse);
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
sprintLongWithCommas(num1Buf, wDS->stats->count * wDS->stats->span);
printf ("<P><B> Statistics on: </B> %s <B> bases </B> (%% %.4f coverage)</P>\n",
    num1Buf, 100.0*(wDS->stats->count * wDS->stats->span)/(winEnd - winStart));

/* For some reason BORDER=1 does not work in our web.c nested table
 * scheme.
 * So use web.c's trick of using an enclosing table to provide a border.  
 */
puts("<P><!--outer table is for border purposes-->" "\n"
     "<TABLE BGCOLOR=\"#"HG_COL_BORDER"\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>");

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
		(float) (wDS->stats->lowerLimit + wDS->stats->dataRange));

/*	global enclosing table row, add a blank line	*/
puts ("<TR><TD BGCOLOR=\""HG_COL_INSIDE"\"> &nbsp </TD></TR><TR><TD>\n");

puts ("<TABLE ALIGN=CENTER COLS=7 BGCOLOR=\""HG_COL_INSIDE"\" BORDER=1 HSPACE=0>\n");
printf ("<TR><TH ALIGN=CENTER COLSPAN=7> Histogram on %u values (zero count bins not shown)</TH><TR>\n",
	valueCount);
puts ("<TR><TH ALIGN=CENTER> bin </TH>\n");
puts ("    <TH ALIGN=CENTER> bin range <BR> [min:max) </TH>\n");
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

	    printf ("<TR><TD ALIGN=RIGHT> %d </TD>\n", i );
	    printf ("    <TD ALIGN=CENTER> %g : %g </TD>\n", min, max);
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
	    printf ("    <TD ALIGN=RIGHT> %g </TD>\n", 1.0 - cpd);
	    someDisplayed = TRUE;
	    }
	}
    }
else
    puts ("<TR><TD COLSPAN=7 ALIGN=CENTER> no data found for histogram </TD></TR>\n");

printf ("</TABLE>\n");

/*	finish out the global enclosing table	*/
puts ("</TD></TR></TABLE></P>\n");

freeHistoGram(&histoGramResult);
freeMem(valuesArray);
destroyWigDataStream(&wDS);
}
