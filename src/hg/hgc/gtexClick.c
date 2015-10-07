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
int barWidth = 9;

/* Dimensions of image overall and our portion within it. */
int imageWidth, imageHeight, innerWidth, innerHeight, innerXoff, innerYoff;

void setImageDims(int expCount)
/* Set up image according to count */
{
innerHeight = graphHeight + 2*pad;
innerWidth = pad + expCount*(pad + barWidth);
imageWidth = innerWidth + 2*margin;
imageHeight = innerHeight + 2*margin;
innerXoff = margin;
innerYoff = margin;
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
void drawBoxAndWhiskers(struct hvGfx *hvg, int fillColorIx, int lineColorIx, int x, int y, 
                        struct tissueSampleVals *tsv, double maxExp)
/* Draw a Tukey-type box plot. JK */
{
int medianColorIx = lineColorIx;
int whiskColorIx = lineColorIx;
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
    printf("<b>Gene name:</b> %s<br>\n", gtexGene->name);
    printf("<b>Ensembl gene:</b> %s<br>\n", gtexGene->geneId);
    printf("<b>Ensembl transcript:</b> %s<br>\n", gtexGene->transcriptId);
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

slReverse(&tsList);
// Tissue list is now sorted by GTEx tissue ordering.  
// TODO: Option to sort by score descending.

// Initialize graphics for Box-and-whiskers plot
setImageDims(slCount(tsList));
struct tempName pngTn;
trashDirFile(&pngTn, "hgc", "gtexGene", ".png");
//uglyf("Trash file: %s\n", pngTn.forCgi);

struct hvGfx *hvg = hvGfxOpenPng(imageWidth, imageHeight, pngTn.forCgi, FALSE);
//hvGfxSetClip(hvg, 0, 0, imageWidth, imageHeight); // needed ?

struct rgbColor lineColor = {.r=0};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);

int x=innerXoff + pad, y = innerYoff + pad;
for (tsv = tsList; tsv != NULL; tsv = tsv->next)
    {
    struct rgbColor fillColor = rgbFromIntColor(tsv->color);
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    drawBoxAndWhiskers(hvg, fillColorIx, lineColorIx, x, y, tsv, maxVal);
    x += barWidth + pad;
    }

//hvGfxUnclip(hvg);
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
