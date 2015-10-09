/* Details pages for GTEx tracks */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "trashDir.h"
#include "hgc.h"

#include "rainbow.h"
#include "hvGfx.h"
#include "gtexUi.h"
#include "gtexGeneBed.h"
#include "gtexTissue.h"
#include "gtexSampleData.h"

/* Dimensions of image parts */
int pad = 2;
int margin = 1;
//int graphHeight = 400;
int graphHeight = 325;
//int barWidth = 3;
int barWidth = 12;
int titleHeight = 30;

int xAxisHeight = 100;
int yAxisWidth = 50;
int axisMargin = 5;

/* Dimensions of image overall and our portion within it. */
int imageWidth, imageHeight, innerWidth, innerHeight, innerXoff, innerYoff;
int plotWidth, plotHeight;      /* just the plot */

void setImageDims(int expCount)
/* Set up image according to count */
{
innerHeight = graphHeight + 2*pad;
innerWidth = pad + expCount*(pad + barWidth);
plotWidth = innerWidth + 2*margin;
plotHeight = innerHeight + 2*margin;
imageWidth = plotWidth + yAxisWidth;
imageHeight = plotHeight + xAxisHeight + titleHeight;
innerXoff = yAxisWidth + margin;
innerYoff = titleHeight + margin;
}

struct rgbColor rgbFromIntColor(int color)
{
return (struct rgbColor){.r=COLOR_32_BLUE(color), .g=COLOR_32_GREEN(color), .b=COLOR_32_RED(color)};
}

struct tissueSampleVals
/* RPKM expression values for multiple samples */
    {
    struct tissueSampleVals *next;
    char *description;  /* GTEx tissue description */
    int color;          /* GTEx tissue color */
    int count;          /* number of samples */
    double *vals;       /* RPKM values */
    double min, max;    /* minimum, maximum value */
    double q1, median, q3;      /* quartiles */
    struct slDouble *valList;   /* used to create val array */
    };

int valToY(double val, double maxVal, int height)
/* Convert a value from 0 to maxVal to 0 to height-1 */
{
double scaled = val/maxVal;
int y = scaled * (height-1);
return height - 1 - y;
}

void drawPointSmear(struct hvGfx *hvg, int xOff, int yOff, int height,
    struct tissueSampleVals *tsv, double maxExp, int colorIx, double midClipMin, double midClipMax)
/* Draw a point for each sample value. */
{
int i, size = tsv->count;
double *vals = tsv->vals;

// Min becomes max after valToY because graph flipped
int yMin = valToY(midClipMax, maxExp, height);
int yMax = valToY(midClipMin, maxExp, height);
for (i=0; i<size; ++i)
    {
    double exp = vals[i];
    int y = valToY(exp, maxExp, height);
    if (y < yMin || y > yMax)
        {
        y += yOff;
        hvGfxDot(hvg, xOff-1, y, colorIx);
        hvGfxDot(hvg, xOff+1, y, colorIx);
        hvGfxDot(hvg, xOff, y-1, colorIx);
        hvGfxDot(hvg, xOff, y+1, colorIx);
        }
    }
}

int cmpTissueSampleValsMedianScore(const void *va, const void *vb)
/* Compare RPKM scores */
{
const struct tissueSampleVals *a = *((struct tissueSampleVals **)va);
const struct tissueSampleVals *b = *((struct tissueSampleVals **)vb);
if (a->median == b->median)
    return 0;
return (a->median > b->median ? 1: -1);
}

void drawBoxAndWhiskers(struct hvGfx *hvg, int fillColorIx, int x, int y, 
                struct tissueSampleVals *tsv, double maxExp)
/* Draw a Tukey-type box plot. JK */
{
struct rgbColor lineColor = {.r=128, .g=128, .b=128};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);
int medianColorIx = hvGfxFindColorIx(hvg,0,0,0);
int whiskColorIx = hvGfxFindColorIx(hvg,192,192,192);
double xCen = x + barWidth/2;
if (tsv->count > 1)
    {
    /* Cache a few fields from tsv in local variables */
    double q1 = tsv->q1, q3 = tsv->q3, median = tsv->median;

    /* Figure out position of first quarter, median, and third quarter in screen Y coordinates */
    int yQ1 = valToY(q1, maxExp, graphHeight) + y;
    int yQ3 = valToY(q3, maxExp, graphHeight) + y;
    int yMedian = valToY(median, maxExp, graphHeight);

    /* Draw a filled box that covers the middle two quarters */
    int qHeight = yQ1 - yQ3 + 1;
    hvGfxOutlinedBox(hvg, x,  yQ3, barWidth, qHeight, fillColorIx, lineColorIx);

    /* Figure out whiskers as 1.5x distance from median to nearest quarter */
    double iq3 = q3 - median;
    double iq1 = median - q1;
    double whisk1 = q1 - 1.5 * iq1;
    double whisk2 = q3 + 1.5 * iq3;

    /* If whiskers extend past min or max point, clip them */
    if (whisk1 < tsv->min)
        whisk1 = tsv->min;
    if (whisk2 > tsv->max)
        whisk2 = tsv->max;

    /* Convert whiskers to screen coordinates and do some sanity checks */
    int yWhisk1 = valToY(whisk1, maxExp, graphHeight) + y;
    int yWhisk2 = valToY(whisk2, maxExp, graphHeight) + y;
    assert(yQ1 <= yWhisk1);
    assert(yQ3 >= yWhisk2);

    /* Draw horizontal lines of whiskers and median */
    hvGfxBox(hvg, x, yWhisk1, barWidth, 1, whiskColorIx);
    hvGfxBox(hvg, x, yWhisk2, barWidth, 1, whiskColorIx);

    /* Draw median, 2 thick */
    hvGfxBox(hvg, x, y + yMedian-1, barWidth, 2, medianColorIx);

    /* Draw lines from mid quarters to whiskers */
    hvGfxBox(hvg, xCen, yWhisk2, 1, yQ3 - yWhisk2, whiskColorIx);
    hvGfxBox(hvg, xCen, yQ1, 1, yWhisk1 - yQ1, whiskColorIx);

    /* Draw any outliers outside of whiskers */
    drawPointSmear(hvg, xCen, y, graphHeight, tsv, maxExp, lineColorIx, whisk1, whisk2);
    }
}

void doGtexGeneExpr(struct trackDb *tdb, char *item)
/* Details of GTEX gene expression item */
{
// Load item from table */

// TODO:  Get full details from Data table 
//char sampleTable[128];
//safef(sampleTable, sizeof(able), "%sSampleData", tdb->table);

struct sqlConnection *conn = hAllocConn(database);
char **row;
char query[512];
struct sqlResult *sr;
struct gtexGeneBed *gtexGene = NULL;
int expCount = 0;
if (sqlTableExists(conn, tdb->table))
    {
    sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", tdb->table, item);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        gtexGene = gtexGeneBedLoad(row);
        expCount = gtexGene->expCount;
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);

genericHeader(tdb, item);

if (gtexGene != NULL)
    {
    // TODO: link to UCSC gene
    printf("<b>Gene:</b> %s</a></br>", gtexGene->name);
    printf("<b>Ensembl ID:</b> %s<br>\n", gtexGene->geneId);
    printf("<a target='_blank' href='http://www.gtexportal.org/home/gene/%s'>View at GTEx portal</a><br>\n", gtexGene->geneId);
    puts("<p>");
    }

// Get full sample data for this gene
char *sampleDataTable = "gtexSampleData";
conn = hAllocConn("hgFixed");
assert(sqlTableExists(conn, sampleDataTable));
sqlSafef(query, sizeof(query), "select * from %s where geneId='%s'", 
                sampleDataTable, gtexGene->geneId);
sr = sqlGetResult(conn, query);
struct hash *tsHash = hashNew(0);
struct tissueSampleVals *tsv;
struct hashEl *hel;
struct slDouble *val;
double maxVal = 0;
struct gtexSampleData *sd = NULL;
while ((row = sqlNextRow(sr)) != NULL)
    {
    sd = gtexSampleDataLoad(row);
    if ((hel = hashLookup(tsHash, sd->tissue)) == NULL)
        {
        AllocVar(tsv);
        hashAdd(tsHash, sd->tissue, tsv);
        }
    else
        tsv = (struct tissueSampleVals *)hel->val;
    maxVal = max(maxVal, sd->score);
    val = slDoubleNew(sd->score);
    slAddHead(&tsv->valList, val);
    }

// Fill in tissue descriptions, fill values array and calculate stats for plotting
// Then make a list, suitable for sorting by tissue or score
struct gtexTissue *tis = NULL, *tissues = gtexGetTissues();
struct tissueSampleVals *tsList = NULL;
int i;
boolean doLogTransform = cartUsualBooleanClosestToHome(cart, tdb, FALSE, GTEX_LOG_TRANSFORM,
                                                GTEX_LOG_TRANSFORM_DEFAULT);
if (doLogTransform)
    maxVal = log10(maxVal+1.0);
for (tis = tissues; tis != NULL; tis = tis->next)
    {
    tsv = hashMustFindVal(tsHash, tis->name);
    tsv->description = tis->description;
    tsv->color = tis->color;
    int count = tsv->count = slCount(tsv->valList);
    double *vals = AllocArray(tsv->vals, count);
    for (i=0; i<count; i++)
        {
        val = slPopHead(&tsv->valList);
        if (doLogTransform)
            vals[i] = log10(val->val+1.0);
        else
            vals[i] = val->val;
        }
    doubleBoxWhiskerCalc(tsv->count, tsv->vals, 
                                &tsv->min, &tsv->q1, &tsv->median, &tsv->q3, &tsv->max);
    slAddHead(&tsList, tsv);
    }

// Tissue list is now sorted by GTEx tissue ordering.  
// Sort by score descending.
slSort(&tsList, cmpTissueSampleValsMedianScore);
slReverse(&tsList);

// Initialize graphics for Box-and-whiskers plot
setImageDims(slCount(tsList));
struct tempName pngTn;
trashDirFile(&pngTn, "hgc", "gtexGene", ".png");
//uglyf("Trash file: %s\n", pngTn.forCgi);

struct hvGfx *hvg = hvGfxOpenPng(imageWidth, imageHeight, pngTn.forCgi, FALSE);
//hvGfxSetClip(hvg, 0, 0, imageWidth, imageHeight); // needed ?

int x=innerXoff + pad, y = innerYoff + pad;
for (tsv = tsList; tsv != NULL; tsv = tsv->next)
    {
    struct rgbColor fillColor = rgbFromIntColor(tsv->color);
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    drawBoxAndWhiskers(hvg, fillColorIx, x, y, tsv, maxVal);
    x += barWidth + pad;
    }
//hvGfxUnclip(hvg);

/* Draw title */
int blackColorIx = hvGfxFindColorIx(hvg, 0,0,0);
char title[256];
// TODO: get tissue count and release from tables 
safef(title, sizeof(title), "%s Gene Expression in 53 Tissues from GTEx (V4 release, 2014) - %s", 
                gtexGene->name, doLogTransform ? "log10(RPKM+1)" : "(RPKM)");
hvGfxTextCentered(hvg, 0, innerYoff, imageWidth, titleHeight, blackColorIx, mgMediumFont(), title);

/* Draw axes */
x = innerXoff - margin;

// Y axis, tick marks, and labels
hvGfxLine(hvg, x, innerYoff, x, plotHeight, blackColorIx);

// for now, just mark the max value
char buf[16];
safef(buf, sizeof(buf), "%d -", round(maxVal));
hvGfxText(hvg, x-24, innerYoff-8, blackColorIx, mgMediumFont(), buf);

// X axis
y = innerYoff + plotHeight;
hvGfxLine(hvg, innerXoff, y, plotWidth, y, blackColorIx);

// Draw vertical labels on separate graphics object, rotate, then merge
/*
struct memGvx *mg = mgNew(imageHeight, imageWidth);
MgFont *font = mgMediumFont();
int lineHeight = mgFontLineHeight(font)-1;
y += lineHeight;
mgTextRight(mg, 0+1, y+1, width-1, lineHeight, MG_BLACK, font, labels[i]);
hvGfxMergeWithPng(hvg, mgRotate90(mg));
// or try using mg lib entirely, via mgSavePng.
*/

#ifdef TICKS
/* Figure tick intervals.  GTEx portal uses .5 for log, 2, 5, 10, 20, 50, 100  for linear.
 * With 7-12 ticks.  5-10 seems nicer to me. Here's how it's done for browser ruler */

static long figureTickSpan(long totalLength, int maxNumTicks)
/* Figure out whether ticks on ruler should be 1, 5, 10, 50, 100, 500, 
 * 1000, etc. units apart.  */
{
int roughTickLen = totalLength/maxNumTicks;
int i;
int tickLen = 1;

for (i=0; i<9; ++i)
    {
    if (roughTickLen < tickLen)
        return tickLen;
    tickLen *= 5;
    if (roughTickLen < tickLen)
        return tickLen;
    tickLen *= 2;
    }
return 1000000000;
}
#endif



hvGfxClose(&hvg);
printf("<IMG SRC = \"%s\" BORDER=1><BR>\n", pngTn.forHtml);
/*printf("<IMG SRC = \"%s\" BORDER=1 WIDTH=%d HEIGHT=%d><BR>\n",
                pngTn.forHtml, imageWidth, imageHeight);
*/
//cloneString(gifTn.forHtml);

// Track description
printTrackHtml(tdb);

// Print out tissue table with color assignments
#ifdef DEBUG
conn = hAllocConn("hgFixed");
char *tissueTable = "gtexTissue";
if (sqlTableExists(conn, tissueTable))
    {
    dyStringPrintf(dy, "<table>");
    dyStringPrintf(dy, "<tr><td><b>Color<b></td><td><b>Tissue<b></td></tr>\n");
    int i;
    double invExpCount = 1.0/expCount;
    char query[512];
    sqlSafef(query, sizeof(query), "select * from %s", tissueTable);
    struct sqlResult *sr = sqlGetResult(conn, query);
    for (i=0; i<expCount; i++)
        {
        row = sqlNextRow(sr);
        if (row == NULL)
            break;
        struct gtexTissue *tissue = gtexTissueLoad(row);
        double colPos = invExpCount * i;
        struct rgbColor color = saturatedRainbowAtPos(colPos);
        dyStringPrintf(dy, "<tr><td bgcolor='#%02X%02X%02X'></td><td>%s</td></tr>\n",
                    color.r, color.g, color.b, tissue->description);
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
dyStringPrintf(dy, "</table>");
puts(dy->string);

//cartWebStart(cart, database, "List of items assayed in %s", clusterTdb->shortLabel);

//genericClickHandlerPlus(tdb, item, item, dy->string);
dyStringFree(&dy);
#endif
}
