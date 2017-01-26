/* Details pages for GTEx tracks */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "hvGfx.h"
#include "trashDir.h"
#include "hgc.h"
#include "hCommon.h"

#include "barChartBed.h"
#include "barChartCategory.h"
#include "barChartUi.h"

static struct barChartBed *getBarChart(char *item, char *chrom, int start, int end, char *table)
/* Retrieve barChart BED item from the main track table */
{
struct barChartBed *barChart = NULL;
struct sqlConnection *conn = hAllocConn(database);
char **row;
char query[512];
struct sqlResult *sr;
if (sqlTableExists(conn, table))
    {
    sqlSafef(query, sizeof query, 
                "SELECT * FROM %s WHERE name = '%s' "
                    "AND chrom = '%s' AND chromStart = %d AND chromEnd = %d", 
                                table, item, chrom, start, end);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        barChart = barChartBedLoad(row);
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return barChart;
}

void doBarChartDetails(struct trackDb *tdb, char *item)
/* Details of barChart item */
{
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct barChartBed *barChart = getBarChart(item, seqName, start, end, tdb->table);
if (barChart == NULL)
    errAbort("Can't find item %s in barChart table %s\n", item, tdb->table);

genericHeader(tdb, item);
int categId;
float highLevel = barChartHighestValue(barChart, &categId);
printf("<b>Highest value: </b> %0.2f in %s<br>\n", 
                highLevel, barChartGetCategoryLabel(categId, database, tdb->table));
printf("<b>Total all values: </b> %0.2f<br>\n", barChartTotalValue(barChart));
printf("<b>Score: </b> %d<br>\n", barChart->score); 
printf("<b>Genomic position: "
                "</b>%s <a href='%s&db=%s&position=%s%%3A%d-%d'>%s:%d-%d</a><br>\n", 
                    database, hgTracksPathAndSettings(), database, 
                    barChart->chrom, barChart->chromStart+1, barChart->chromEnd,
                    barChart->chrom, barChart->chromStart+1, barChart->chromEnd);
puts("<p>");

#ifdef BOXPLOT
struct tempName pngTn;
if (barChartBoxplot(barChart->name, &pngTn))
    printf("<img src = \"%s\" border=1><br>\n", pngTn.forHtml);
printf("<br>");
#endif
printTrackHtml(tdb);
}
