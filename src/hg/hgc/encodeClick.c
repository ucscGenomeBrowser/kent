/* Handle details page for ENCODE tracks. */

#include "common.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "web.h"
#include "encode/encodePeak.h"

void doEncodePeak(struct trackDb *tdb)
/*  details for encodePeak type tracks. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
enum encodePeakType peakType = encodePeakInferType(database, tdb);
char **row;
char *chrom = cgiString("c");
int start = cgiInt("o");
int end = cgiInt("t");
int rowOffset;
if (peakType == 0)
    errAbort("unrecognized peak type from table %s", tdb->tableName);
genericHeader(tdb, NULL);
sr = hOrderedRangeQuery(conn, tdb->tableName, chrom, start, end,
			NULL, &rowOffset);
row = sqlNextRow(sr);
if (row != NULL)
    {
    char **rowPastOffset = row + rowOffset;
    int sigIx = 3, pValIx = 4;
    /* Position */
    printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a><BR>\n",
       hgTracksPathAndSettings(), database, chrom, start+1, end, chrom, start+1, end);
    /* Print peak base */
    if ((peakType == narrowPeak) || (peakType == encodePeak6) || (peakType == encodePeak9))
	{
	int peakIx = 5;
	if (peakType == narrowPeak)
	    peakIx = 8;
	int peak = sqlSigned(rowPastOffset[peakIx]);
	if (peak > -1)
	    printf("<B>Peak base on chrom:</B> %d<BR>\n", start + peak);
	}
    /* Strand, score */
    if ((peakType == narrowPeak) || (peakType == broadPeak) || (peakType == gappedPeak))
	{
	printf("<B>Strand:</B> %c<BR>\n", rowPastOffset[5][0]);
	printf("<B>Score:</B> %d<BR>\n", sqlUnsigned(rowPastOffset[4]));       
	}
    /* signalVal, pVal */
    if ((peakType == narrowPeak) || (peakType == broadPeak))
	{
	sigIx = 6;
	pValIx = 7;
	}
    else if (peakType == gappedPeak)
	{
	sigIx = 12;
	pValIx = 13;
	}
    printf("<B>Signal value:</B> %f<BR>\n", sqlFloat(rowPastOffset[sigIx]));
    printf("<B>P-value:</B> %f<BR>\n", sqlFloat(rowPastOffset[pValIx]));
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
printTrackHtml(tdb);
}
