/* Details pages for barChart tracks */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "hvGfx.h"
#include "trashDir.h"
#include "hCommon.h"
#include "hui.h"
#include "asParse.h"
#include "hgc.h"
#include "trackHub.h"
#include "memgfx.h"
#include "hgColors.h"
#include "fieldedTable.h"

#include "barChartBed.h"
#include "barChartCategory.h"
#include "barChartData.h"
#include "barChartSample.h"
#include "barChartUi.h"
#include "hgConfig.h"

#define EXTRA_FIELDS_SIZE 256

struct barChartItemData
/* Measured value for a sample and the sample category at a locus.
 * Used for barChart track details (boxplot) */
    {
    struct barChartItemData *next;  /* Next in singly linked list. */
    char *sample;	/* Sample identifier */
    char *category;     /* Sample category (from barChartSample table  or barChartSampleUrl file) */
    double value;	/* Measured value (e.g. expression level) */
    };

static struct hash *getTrackCategories(struct trackDb *tdb)
/* Get list of categories from trackDb.  This may be a subset of those in matrix. 
 * (though maybe better to prune matrix for performance) */
{
char *categs = trackDbSetting(tdb, BAR_CHART_CATEGORY_LABELS);
char *words[BAR_CHART_MAX_CATEGORIES];
int wordCt;
wordCt = chopLine(cloneString(categs), words);
int i;
struct hash *categoryHash = hashNew(0);
for (i=0; i<wordCt; i++)
    {
    hashStore(categoryHash, words[i]);
    }
return categoryHash;
}

static struct barChartBed *getBarChartFromFile(struct trackDb *tdb, char *file, 
                                                char *item, char *chrom, int start, int end, 
                                                struct asObject **retAs, char **extraFieldsRet,
                                                int *extraFieldsCountRet)
/* Retrieve barChart BED item from big file */
{
boolean hasOffsets = TRUE;
struct bbiFile *bbi = bigBedFileOpen(file);
struct asObject *as = bigBedAsOrDefault(bbi);
if (retAs != NULL)
    *retAs = as;
hasOffsets = (
    asColumnFind(as, BARCHART_OFFSET_COLUMN) != NULL && 
    asColumnFind(as, BARCHART_LEN_COLUMN) != NULL);
struct lm *lm = lmInit(0);
struct bigBedInterval *bb, *bbList =  bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char *rest = cloneString(bb->rest);
    char startBuf[16], endBuf[16];
    char *row[32];
    bigBedIntervalToRow(bb, chrom, startBuf, endBuf, row, ArraySize(row));
    struct barChartBed *barChart = barChartBedLoadOptionalOffsets(row, hasOffsets);
    if (barChart == NULL)
        continue;
    if (sameString(barChart->name, item))
        {
        char *restFields[EXTRA_FIELDS_SIZE];
        int restCount = chopTabs(rest, restFields);
        int restBedFields = (6 + (hasOffsets ? 2 : 0));
        if (restCount > restBedFields)
            {
            int i;
            for (i = 0; i < restCount - restBedFields; i++)
                extraFieldsRet[i] = restFields[restBedFields + i];
            *extraFieldsCountRet = (restCount - restBedFields);
            }
        return barChart;
        }
    }
return NULL;
}

static struct barChartBed *getBarChartFromTable(struct trackDb *tdb, char *table, 
                                                char *item, char *chrom, int start, int end)
/* Retrieve barChart BED item from track table */
{
struct sqlConnection *conn = NULL;
struct customTrack *ct = lookupCt(tdb->track);
if (ct == NULL)
    conn = hAllocConnTrack(database, tdb);
else
    {
    conn = hAllocConn(CUSTOM_TRASH);
    table = ct->dbTableName;
    }
if (conn == NULL)
    return NULL;
struct barChartBed *barChart = NULL;
char **row;
char query[512];
struct sqlResult *sr;
if (sqlTableExists(conn, table))
    {
    boolean hasOffsets = (sqlColumnExists(conn, table, BARCHART_OFFSET_COLUMN) &&
                         sqlColumnExists(conn, table, BARCHART_LEN_COLUMN));
    sqlSafef(query, sizeof query, 
                "SELECT * FROM %s WHERE name='%s'"
                    "AND chrom='%s' AND chromStart=%d AND chromEnd=%d", 
                                table, item, chrom, start, end);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        barChart = barChartBedLoadOptionalOffsets(row, hasOffsets);
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return barChart;
}

static struct barChartBed *getBarChart(struct trackDb *tdb, char *item, char *chrom, int start, int end,
                                        struct asObject **retAs, char **extraFieldsReg, int *extraFieldsCountRet)
/* Retrieve barChart BED item from track */
{
struct barChartBed *barChart = NULL;
char *file = trackDbSetting(tdb, "bigDataUrl");
if (file != NULL)
    barChart = getBarChartFromFile(tdb, file, item, chrom, start, end, retAs, extraFieldsReg, extraFieldsCountRet);
else
    barChart = getBarChartFromTable(tdb, tdb->table, item, chrom, start, end);
return barChart;
}

static struct barChartItemData *getSampleValsFromFile(struct trackDb *tdb, 
                        struct hash *categoryHash, struct barChartBed *bed, 
                        char *dataFile, char *sampleFile)
/* Get all data values in a file for this item (locus) */
{
// Get sample categories from sample file
// Format: id, category, extras
struct lineFile *lf = udcWrapShortLineFile(sampleFile, NULL, 0);
struct hash *sampleHash = hashNew(0);
char *words[2];
int sampleCt = 0;
while (lineFileChopNext(lf, words, sizeof words))
    {
    hashAdd(sampleHash, words[0], words[1]);
    sampleCt++;
    }
lineFileClose(&lf);

// Open matrix file
struct udcFile *f = udcFileOpen(dataFile, NULL);

// Get header line with sample ids
char *header = udcReadLine(f);
int wordCt = sampleCt+1;        // initial field is label or empty 
char **samples;
AllocArray(samples, wordCt);
chopByWhite(header, samples, wordCt);

// Get data values
// Format: id, category, extras
bits64 offset = (bits64)bed->_dataOffset;
bits64 size = (bits64)bed->_dataLen;
udcSeek(f, offset);
bits64 seek = udcTell(f);
if (udcTell(f) != offset)
    warn("UDC seek mismatch: expecting %Lx, got %Lx. ", offset, seek);
char *buf = needMem(size+1);
bits64 count = udcRead(f, buf, size);
if (count != size)
    warn("UDC read mismatch: expecting %Ld bytes, got %Ld. ", size, count);

char **vals;
AllocArray(vals, wordCt);
buf[size]=0; // in case that size does not include the trailing newline
chopByWhite(buf, vals, wordCt);

udcFileClose(&f);

// Construct list of sample data with category
struct barChartItemData *sampleVals = NULL, *data = NULL;
int i;
for (i=1; i<wordCt && samples[i] != NULL; i++)
    {
    char *sample = samples[i];
    char *categ = (char *)hashFindVal(sampleHash, sample);
    if (categ == NULL)
        warn("barChart track %s: unknown category for sample %s", tdb->track, sample);
    else if (hashLookup(categoryHash, categ))
        {
        AllocVar(data);
        data->sample = cloneString(sample);
        data->category = cloneString(categ);
        data->value = sqlDouble(vals[i]);
        slAddHead(&sampleVals, data);
        }
    }
return sampleVals;
}

static struct sqlConnection *getConnectionAndTable(struct trackDb *tdb, char *suffix,
                                                         char **retTable)
/* Look for <table><suffix> in database or hgFixed and set up connection */
{
char table[256];
if (trackHubDatabase(database))
    return NULL;

assert(retTable);
safef(table, sizeof(table), "%s%s", tdb->table, suffix);
*retTable = cloneString(table);
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
return conn;
}

static struct barChartItemData *getSampleValsFromTable(struct trackDb *tdb, 
                                                        struct hash *categoryHash,
                                                        struct barChartBed *bed)
/* Get data values for this item (locus) from all samples */
{
char *table = NULL;
struct sqlConnection *conn = getConnectionAndTable(tdb, "Data", &table);
if (conn == NULL)
    return NULL;
struct barChartData *val, *vals = barChartDataLoadForLocus(conn, table, bed->name);

// Get category for samples 
conn = getConnectionAndTable(tdb, "Sample", &table);
char query[512];
sqlSafef(query, sizeof(query), "SELECT * FROM %s", table);
struct barChartSample *sample, *samples = barChartSampleLoadByQuery(conn, query);
hFreeConn(&conn);

struct hash *sampleHash = hashNew(0);
for (sample = samples; sample != NULL; sample = sample->next)
    {
    hashAdd(sampleHash, sample->sample, sample);
    }

// Construct list of sample data with category
struct barChartItemData *sampleVals = NULL, *data = NULL;
for (val = vals; val != NULL; val = val->next)
    {
    struct barChartSample *sample = hashFindVal(sampleHash, val->sample);
    if (sample == NULL)
        warn("barChart track %s: unknown category for sample %s", tdb->track, val->sample);
    else if (hashLookup(categoryHash, sample->category))
        {
        AllocVar(data);
        data->sample = cloneString(val->sample);
        data->category = cloneString(sample->category);
        data->value = val->value;
        slAddHead(&sampleVals, data);
        }
    }
return sampleVals;
}

static struct barChartItemData *getSampleVals(struct trackDb *tdb, struct barChartBed *chartItem,
                                                char **retMatrixUrl, char **retSampleUrl)
/* Get data values for this item (locus) from all samples */
{
struct barChartItemData *vals = NULL;
char *dataFile = trackDbSetting(tdb, "barChartMatrixUrl");
// for backwards compatibility during qa review
if (dataFile == NULL)
    dataFile = trackDbSetting(tdb, "barChartDataUrl");
// for backwards compatibility during qa review
struct hash *categoryHash = getTrackCategories(tdb);
if (dataFile != NULL)
    {
    char *sampleFile = trackDbSetting(tdb, "barChartSampleUrl");
    if (sampleFile == NULL)
        return NULL;
    if (retMatrixUrl != NULL)
        *retMatrixUrl = dataFile;
    if (retSampleUrl != NULL)
        *retSampleUrl = sampleFile;
    vals = getSampleValsFromFile(tdb, categoryHash, chartItem, dataFile, sampleFile);
    }
else
    vals = getSampleValsFromTable(tdb, categoryHash, chartItem);
return vals;
}

static char *makeDataFrame(char *track, struct barChartItemData *vals)
/* Create R data frame from sample data.  This is a tab-sep file, one row per sample.
   Return filename. */
{

// Create data frame with columns for sample, category, value */
struct tempName dfTn;
trashDirFile(&dfTn, "hgc", "barChart", ".df.txt");
FILE *f = fopen(dfTn.forCgi, "w");
if (f == NULL)
    errAbort("can't create temp file %s", dfTn.forCgi);
fprintf(f, "sample\tcategory\tvalue\n");
struct barChartItemData *val;
for (val = vals; val != NULL; val = val->next)
    {
    fprintf(f, "%s\t%s\t%0.3f\n", val->sample, val->category, val->value);
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

static void printBoxplot(char *df, char *item, char *name2, char *units, char *colorFile)
/* Plot data frame to image file and include in HTML */
{
struct tempName pngTn;
struct dyString *cmd = dyStringNew(0);
trashDirFile(&pngTn, "hgc", "barChart", ".png");

// to help with QAing the change, we add the "oldFonts" CGI parameter so QA can compare
// old and new fonts to make sure that things are still readible on mirrors and servers
// without the new fonts installed. This only needed during the QA phase
bool useOldFonts = cgiBoolean("oldFonts");

/* Exec R in quiet mode, without reading/saving environment or workspace */
dyStringPrintf(cmd, "Rscript --vanilla --slave hgcData/barChartBoxplot.R %s '%s' %s %s %s %s %d",
                                item, units, colorFile, df, pngTn.forHtml, isEmpty(name2) ? "n/a" : name2, useOldFonts);
int ret = system(cmd->string);
if (ret == 0)
    printf("<img src = \"%s\" border=1><br>\n", pngTn.forHtml);
else
    warn("Error creating boxplot from sample data with command: %s", cmd->string);
}

static double estimateStringWidth(char *s)
/* Get estimate of string width based on a memory font that is about the
 * same size as svg will be using.  After much research I don't think we
 * can get the size from the server, would have to be in Javascript to get
 * more precise */
{
MgFont *font = mgHelvetica14Font();
return mgFontStringWidth(font, s);
}

static double longestLabelSize(struct barChartCategory *categList)
/* Get estimate of longest label in pixels */
{
int longest = 0;
struct barChartCategory *categ;
for (categ = categList; categ != NULL; categ = categ->next)
    {
    int size = estimateStringWidth(categ->label);
    if (size > longest)
        longest = size;
    }
return longest * 1.02;
}

void deunderbarColumn(struct fieldedTable *ft, char *field)
/* Ununderbar all of a column inside table because space/underbar gets
 * so confusing */
{
int fieldIx = fieldedTableFindFieldIx(ft, field);
struct fieldedRow *row;
for (row = ft->rowList; row != NULL; row = row->next)
    replaceChar(row->row[fieldIx], '_', ' ');
}

static void printBarChart(struct barChartBed *chart, struct trackDb *tdb, double maxVal, char *metric)
/* Plot bar chart without quartiles or anything fancy just using SVG */
{
/* Load up input labels, color, and data */
struct barChartCategory *categs = barChartUiGetCategories(database, tdb);
int categCount = slCount(categs);
if (categCount != chart->expCount)
    {
    warn("Problem in %s barchart track. There are %d categories in trackDb and %d in data",
	tdb->track, categCount, chart->expCount);
    return;
    }

char *statsFile = trackDbSetting(tdb, "barChartStatsUrl");
struct hash *statsHash = NULL;
int countStatIx = 0;
double statsSize = 0.0;
if (statsFile != NULL)
    {
    char *required[] = {"cluster", "count", "total"};
    struct fieldedTable *ft = fieldedTableFromTabFile(
	statsFile, statsFile, required, ArraySize(required));
    deunderbarColumn(ft, "cluster");
    statsHash = fieldedTableIndex(ft, "cluster");
    countStatIx = fieldedTableFindFieldIx(ft, "count");
    statsSize = 8*(fieldedTableMaxColChars(ft, countStatIx)+1);
    }

/* Some constants that control layout */
double heightPer=18.0;
double totalWidth=1250.0;
double borderSize = 1.0;

double headerHeight = heightPer + 2*borderSize;
double innerHeight=heightPer-borderSize;
double labelWidth = longestLabelSize(categs) + 9;  // Add some because size is just estimate
if (labelWidth > totalWidth/2) labelWidth = totalWidth/2;  // Don't let labels take up more than half
double patchWidth = heightPer;
double labelOffset = patchWidth + 2*borderSize;
double statsOffset = labelOffset + labelWidth;
double barOffset = statsOffset + statsSize;
double statsRightOffset = barOffset - 9;
double barNumLabelWidth = estimateStringWidth(" 1234.000"); 
double barMaxWidth = totalWidth-barOffset -barNumLabelWidth ;
double totalHeight = headerHeight + heightPer * categCount + borderSize;

printf("<svg width=\"%g\" height=\"%g\">\n", totalWidth, totalHeight);

/* Draw header */
printf("<rect width=\"%g\" height=\"%g\" style=\"fill:#%s\"/>\n", totalWidth, headerHeight, HG_COL_HEADER);
char *sampleLabel = trackDbSettingOrDefault(tdb, "barChartLabel", "Sample");
printf("<text x=\"%g\" y=\"%g\" font-size=\"%g\">%s</text>\n", 
    labelOffset, innerHeight-1, innerHeight-1, sampleLabel);
if (statsSize > 0.0)
    printf("<text x=\"%g\" y=\"%g\" font-size=\"%g\" text-anchor=\"end\">%s</text>\n", 
	statsRightOffset, innerHeight-1, innerHeight-1, "N");
printf("<text x=\"%g\" y=\"%g\" font-size=\"%g\">%s %s</text>\n", 
    barOffset, innerHeight-1, innerHeight-1, metric, "Value");

/* Set up clipping path for the pesky labels, which may be too long */
printf("<clipPath id=\"labelClip\"><rect x=\"%g\" y=\"0\" width=\"%g\" height=\"%g\"/></clipPath>\n",
    labelOffset, barOffset-labelOffset, totalHeight);

double yPos = headerHeight;
struct barChartCategory *categ;
int i;
for (i=0, categ=categs; i<categCount; ++i , categ=categ->next, yPos += heightPer)
    {
    double score = chart->expScores[i];
    double barWidth = 0;
    if (maxVal > 0.0)
	barWidth = barMaxWidth * score/maxVal;
    char *deunder = cloneString(categ->label);
    replaceChar(deunder, '_', ' ');
    printf("<rect x=\"0\" y=\"%g\" width=\"15\" height=\"%g\" style=\"fill:#%06X\"/>\n",
	yPos, innerHeight, categ->color);
    printf("<rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" style=\"fill:#%06X\"/>\n",
	barOffset, yPos, barWidth, innerHeight, categ->color);
    if (i&1)  // every other time
	printf("<rect x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" style=\"fill:#%06X\"/>\n",
	    labelOffset, yPos, labelWidth+statsSize, innerHeight, 0xFFFFFF);
    printf("<text x=\"%g\" y=\"%g\" font-size=\"%g\" clip-path=\"url(#labelClip)\"\">%s</text>\n", 
 	labelOffset, yPos+innerHeight-1, innerHeight-1, deunder);
    if (statsSize > 0.0)
	{
	struct fieldedRow *fr = hashFindVal(statsHash, deunder);
	if (fr != NULL)
	    {
	    printf("<text x=\"%g\" y=\"%g\" font-size=\"%g\" text-anchor=\"end\">%s</text>\n", 
		statsRightOffset, yPos+innerHeight-1, innerHeight-1, fr->row[countStatIx]);
	    }
	}
    printf("<text x=\"%g\" y=\"%g\" font-size=\"%g\">%5.3f</text>\n", 
	barOffset+barWidth+2, yPos+innerHeight-1, innerHeight-1, score);
    }
printf("</svg>");
}


struct asColumn *asFindColByIx(struct asObject *as, int ix)
/* Find AS column by index */
{
struct asColumn *asCol;
int i;
for (i=0, asCol = as->columnList; asCol != NULL && i<ix; asCol = asCol->next, i++);
    return asCol;
}

void doBarChartDetails(struct trackDb *tdb, char *item)
/* Details of barChart item */
{
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct asObject *as = NULL;
char *extraFields[EXTRA_FIELDS_SIZE];
int extraFieldCount = 0;
int numColumns = 0;
struct barChartBed *chartItem = getBarChart(tdb, item, seqName, start, end, &as, extraFields, &extraFieldCount);
if (chartItem == NULL)
    errAbort("Can't find item %s in barChart table/file %s\n", item, tdb->table);

genericHeader(tdb, item);

// get name and name2 from trackDb, .as file, or use defaults
struct asColumn *nameCol = NULL, *name2Col = NULL;
//struct asColumn *name2Col;
char *nameLabel = NULL, *name2Label = NULL;
if (as != NULL)
    {
    numColumns = slCount(as->columnList);
    nameCol = asFindColByIx(as, BARCHART_NAME_COLUMN_IX);
    name2Col = asFindColByIx(as, BARCHART_NAME2_COLUMN_IX);
    }
nameLabel = trackDbSettingClosestToHomeOrDefault(tdb, "bedNameLabel", nameCol ? nameCol->comment : "Item");
if (trackDbSettingClosestToHomeOrDefault(tdb, "url", NULL) != NULL)
    printCustomUrl(tdb, item, TRUE);
else
    printf("<b>%s: </b>%s<br>\n", nameLabel, chartItem->name);
name2Label = name2Col ? name2Col->comment : "Alternative name";
if (differentString(chartItem->name2, "")) 
    {
    if (trackDbSettingClosestToHomeOrDefault(tdb, "url2", NULL) != NULL)
        printOtherCustomUrl(tdb, chartItem->name2, "url2", TRUE);
    else
        printf("<b>%s: </b> %s<br>\n", name2Label, chartItem->name2);
    }

int categId;
float highLevel = barChartMaxValue(chartItem, &categId);
char *units = trackDbSettingClosestToHomeOrDefault(tdb, BAR_CHART_UNIT, "units");
char *metric = trackDbSettingClosestToHomeOrDefault(tdb, BAR_CHART_METRIC, "");
printf("<b>Total all %s values: </b> %0.2f %s<br>\n", metric, barChartTotalValue(chartItem), units);
printf("<b>Maximum %s value: </b> %0.2f %s in %s<br>\n", 
                metric, highLevel, units, barChartUiGetCategoryLabelById(categId, database, tdb));
printf("<b>Score: </b> %d<br>\n", chartItem->score); 
printf("<b>Genomic position: "
                "</b>%s <a href='%s&db=%s&position=%s%%3A%d-%d'>%s:%d-%d</a><br>\n", 
                    database, hgTracksPathAndSettings(), database, 
                    chartItem->chrom, chartItem->chromStart+1, chartItem->chromEnd,
                    chartItem->chrom, chartItem->chromStart+1, chartItem->chromEnd);
printf("<b>Strand: </b> %s\n", chartItem->strand); 

// print any remaining extra fields
if (numColumns > 0)
    {
    extraFieldsPrint(tdb, NULL, extraFields, extraFieldCount);
    }

char *matrixUrl = NULL, *sampleUrl = NULL;
struct barChartItemData *vals = getSampleVals(tdb, chartItem, &matrixUrl, &sampleUrl);
puts("<p>");
if (vals != NULL)
    {
    // Print boxplot
    char *df = makeDataFrame(tdb->table, vals);
    char *colorFile = makeColorFile(tdb);
    printBoxplot(df, item, chartItem->name2, units, colorFile);
    printf("<br><a href='%s'>View all data points for %s%s%s%s</a>\n", df, 
                        chartItem->name, 
                        chartItem->name2 ? " (" : "",
                        chartItem->name2 ? chartItem->name2 : "",
                        chartItem->name2 ? ")" : "");
    }
else
    {
    if (cfgOptionBooleanDefault("svgBarChart", FALSE))
	printBarChart(chartItem, tdb, highLevel, metric);
    }
puts("<br>");
}

