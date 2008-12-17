/* Handle details page for ENCODE tracks. */

#include "common.h"
#include "cart.h"
#include "hgc.h"
#include "hCommon.h"
#include "hgColors.h"
#include "customTrack.h"
#include "web.h"
#include "encode/encodePeak.h"

void doEncodePeak(struct trackDb *tdb, struct customTrack *ct)
/*  details for encodePeak type tracks. */
{
struct sqlConnection *conn;
struct sqlResult *sr;
enum encodePeakType peakType;
char **row;
char *db;
char *table = tdb->tableName;
char *chrom = cgiString("c");
int start = cgiInt("o");
int end = cgiInt("t");
int rowOffset;
/* connect to DB */
if (ct)
    {
    db = CUSTOM_TRASH;
    table = ct->dbTableName;
    }
else 
    db = database;
conn = hAllocConn(db);
peakType = encodePeakInferTypeFromTable(db, table, tdb->type);
if (peakType == 0)
    errAbort("unrecognized peak type from table %s", tdb->tableName);
genericHeader(tdb, NULL);
sr = hOrderedRangeQuery(conn, table, chrom, start, end,
			NULL, &rowOffset);
row = sqlNextRow(sr);
if (row != NULL)
    {
    char **rowPastOffset = row + rowOffset;
    float signal = -1;
    float pValue = -1;
    float qValue = -1;
    /* Name */
    if (rowPastOffset[3][0] != '.')
	printf("<B>Name:</B> %s<BR>\n", rowPastOffset[3]);
    /* Position */
    printf("<B>Position:</B> "
       "<A HREF=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a><BR>\n",
       hgTracksPathAndSettings(), database, chrom, start+1, end, chrom, start+1, end);
    /* Print peak base */
    if ((peakType == narrowPeak) || (peakType == encodePeak))
	{
	int peak = sqlSigned(rowPastOffset[9]);
	if (peak > -1)
	    printf("<B>Peak base on chrom:</B> %d<BR>\n", start + peak);
	}
    /* Strand, score */
    if (rowPastOffset[5][0] != '.')
	    printf("<B>Strand:</B> %c<BR>\n", rowPastOffset[5][0]);
    printf("<B>Score:</B> %d<BR>\n", sqlUnsigned(rowPastOffset[4]));       
    /* signalVal, pVal */
    if (peakType != gappedPeak)
	{
	signal = sqlFloat(rowPastOffset[6]);
	pValue = sqlFloat(rowPastOffset[7]);
	qValue = sqlFloat(rowPastOffset[8]);
	}
    else
	{
	signal = sqlFloat(rowPastOffset[12]);
	pValue = sqlFloat(rowPastOffset[13]);
	qValue = sqlFloat(rowPastOffset[14]);
	}
    if (signal >= 0)
	printf("<B>Signal value:</B> %.3f<BR>\n", signal);
    if (pValue >= 0)
	printf("<B>P-value (-log10):</B> %.3f<BR>\n", pValue);
    if (qValue >= 0)
	printf("<B>Quality value: </B> %.3f<BR>\n", qValue);	
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
}
