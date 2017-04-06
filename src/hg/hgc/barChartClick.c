/* Details pages for barChart tracks */

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
#include "barChartData.h"
#include "barChartSample.h"
#include "barChartUi.h"

// TODO: Consider moving these to lib/{barChartBed,bigBarChart}.c

static struct barChartBed *getBarChartFromFile(char *item, char *chrom, int start, int end, 
                                                        char *file)
/* Retrieve barChart BED item from big file */
{
struct bbiFile *bbi = bigBedFileOpen(file);
struct lm *lm = lmInit(0);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char startBuf[16], endBuf[16];
    char *bedRow[32];
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct barChartBed *barChart = barChartBedLoad(bedRow);
    if (sameString(barChart->name, item))
        return barChart;
    }
return NULL;
}

static struct barChartBed *getBarChartFromTable(char *item, char *chrom, int start, int end, 
                                                        char *table)
/* Retrieve barChart BED item from track table */
{
struct barChartBed *barChart = NULL;
struct sqlConnection *conn = hAllocConn(database);
char **row;
char query[512];
struct sqlResult *sr;
if (sqlTableExists(conn, table))
    {
    sqlSafef(query, sizeof query, 
                "SELECT * FROM %s WHERE name='%s'"
                    "AND chrom='%s' AND chromStart=%d AND chromEnd=%d", 
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

static struct barChartBed *getBarChart(char *item, char *chrom, int start, int end, 
                                                        struct trackDb *tdb)
/* Retrieve barChart BED item from track */
{
struct barChartBed *barChart = NULL;
char *file = trackDbSetting(tdb, "bigDataUrl");
if (file != NULL)
    barChart = getBarChartFromFile(item, chrom, start, end, file);
else
    barChart = getBarChartFromTable(item, chrom, start, end, tdb->table);
return barChart;
}

static struct barChartData *getSampleVals(char *track, char *item)
/* Get data values for this item (locus) from all samples */
{
char table[256];
safef(table, sizeof(table), "%s%s", track, "Data");
struct sqlConnection *conn = hAllocConn(database);
if (!sqlTableExists(conn, table))
    {
    hFreeConn(&conn);
    conn = hAllocConn("hgFixed");
    if (!sqlTableExists(conn, table))
        {
        hFreeConn(&conn);
        return NULL;
        }
    }
struct barChartData *vals = barChartDataLoadForLocus(conn, table, item);
hFreeConn(&conn);
return vals;
}

static char *makeDataFrame(char *track, struct barChartData *vals)
/* Create R data frame from sample data.  This is a tab-sep file, one row per sample.
   Return filename. */
{
// Get category for samples 
char table[256];
safef(table, sizeof(table), "%s%s", track, "Sample");
struct sqlConnection *conn = hAllocConn(database);
if (!sqlTableExists(conn, table))
    {
    hFreeConn(&conn);
    conn = hAllocConn("hgFixed");
    if (!sqlTableExists(conn, table))
        {
        hFreeConn(&conn);
        return NULL;
        }
}
char query[512];
sqlSafef(query, sizeof(query), "SELECT * FROM %s", table);
struct barChartSample *sample, *samples = barChartSampleLoadByQuery(conn, query);
hFreeConn(&conn);
struct hash *sampleHash = hashNew(0);
for (sample = samples; sample != NULL; sample = sample->next)
    {
    hashAdd(sampleHash, sample->sample, sample);
    }

// Create data frame with columns for sample, category, value */
struct tempName dfTn;
trashDirFile(&dfTn, "hgc", "barChart", ".df.txt");
FILE *f = fopen(dfTn.forCgi, "w");
if (f == NULL)
    errAbort("can't create temp file %s", dfTn.forCgi);
fprintf(f, "sample\tcategory\tvalue\n");
int i = 0;
struct barChartData *val;
for (val = vals; val != NULL; val = val->next)
    {
    struct barChartSample *sample = hashFindVal(sampleHash, val->sample);
    if (sample == NULL)
        warn("barChart track %s: unknown category for sample %s", track, val->sample);
    else
        fprintf(f, "%d\t%s\t%0.3f\n", i++, sample->category, val->value);
    }
fclose(f);
return cloneString(dfTn.forCgi);
}

char *makeColorFile(struct trackDb *tdb)
/* Make a file with category + color */
{
struct tempName colorTn;
trashDirFile(&colorTn, "hgc", "barChartColors", ".txt");
FILE *f = fopen(colorTn.forCgi, "w");
if (f == NULL)
    errAbort("can't create temp file %s", colorTn.forCgi);
struct barChartCategory *categs = barChartUiGetCategories(database, tdb);
struct barChartCategory *categ;
fprintf(f, "%s\t%s\n", "category", "color");
for (categ = categs; categ != NULL; categ = categ->next)
    {
    //fprintf(f, "%s\t#%06X\n", categ->label, categ->color);
    fprintf(f, "%s\t%d\n", categ->label, categ->color);
    }
fclose(f);
return cloneString(colorTn.forCgi);
}

static void printBoxplot(char *df, char *item, char *units, char *colorFile)
/* Plot data frame to image file and include in HTML */
{
struct tempName pngTn;
trashDirFile(&pngTn, "hgc", "barChart", ".png");

/* Exec R in quiet mode, without reading/saving environment or workspace */
char cmd[256];
safef(cmd, sizeof(cmd), "Rscript --vanilla --slave hgcData/barChartBoxplot.R %s %s %s %s %s",
                                item, units, colorFile, df, pngTn.forHtml);
int ret = system(cmd);
if (ret == 0)
    printf("<img src = \"%s\" border=1><br>\n", pngTn.forHtml);
}

void doBarChartDetails(struct trackDb *tdb, char *item)

/* Details of barChart item */
{
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct barChartBed *chartItem = getBarChart(item, seqName, start, end, tdb);
if (chartItem == NULL)
    errAbort("Can't find item %s in barChart table %s\n", item, tdb->table);

genericHeader(tdb, item);
int categId;
float highLevel = barChartHighestValue(chartItem, &categId);
char *units = trackDbSettingClosestToHomeOrDefault(tdb, BAR_CHART_UNIT, "");
printf("<b>Maximum value: </b> %0.2f %s in %s<br>\n", 
                highLevel, units, barChartUiGetCategoryLabelById(categId, database, tdb));
printf("<b>Total all values: </b> %0.2f<br>\n", barChartTotalValue(chartItem));
printf("<b>Score: </b> %d<br>\n", chartItem->score); 
printf("<b>Genomic position: "
                "</b>%s <a href='%s&db=%s&position=%s%%3A%d-%d'>%s:%d-%d</a><br>\n", 
                    database, hgTracksPathAndSettings(), database, 
                    chartItem->chrom, chartItem->chromStart+1, chartItem->chromEnd,
                    chartItem->chrom, chartItem->chromStart+1, chartItem->chromEnd);
struct barChartData *vals = getSampleVals(tdb->table, item);
if (vals != NULL)
    {
    // Print boxplot
    puts("<p>");
    char *df = makeDataFrame(tdb->table, vals);
    char *colorFile = makeColorFile(tdb);
    printBoxplot(df, item, units, colorFile);
    }
puts("<br>");
printTrackHtml(tdb);
}

