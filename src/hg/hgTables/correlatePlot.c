/* correlatePlot - produce correlation plot graphs */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "memgfx.h"
#include "vGfx.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "obscure.h"
#include "hgTables.h"
#include "correlate.h"
#include "histogram.h"
#include "trashDir.h"


#define CLIP(p,limit) if (p < 0) p = 0; if (p >= (limit)) p = (limit)-1;

static void verticalTextCentered(struct vGfx *vg, int x, int y,
    int width, int height, int colorIx, MgFont *font, char *string)
/* Draw a vertical line of text in middle of the box.
 *	The string will read from bottom to top
 */
{
/*	don't let this run wild	*/
CLIP(width, vg->width);
CLIP(height, vg->height);

if ((width > 0) && (height > 0))
    {
    struct vGfx *vgHoriz;
    int i, j;
    /*	reversed meanings of width and height here since this is going
     *	to rotate 90 degrees
    struct memGfx *mgHoriz;
    mgHoriz = mgNew(height, width);
    mgTextCentered(mgHoriz, 0, 0, height, width, colorIx, font, string);
     */
    vgHoriz = vgOpenPng(height, width, "/dev/null", FALSE);
    vgTextCentered(vgHoriz, 0, 0, height, width, colorIx, font, string);
    /*	now, blit from the horizontal to the vertical, rotate -90 (CCW) */
    for (i = 0; i < height; ++i)	/* xSrc -> yDest */
	{
	int yDest = height - i;
	for (j = 0; j < width; ++j)	/* ySrc -> xDest */
	    {
	    vgDot(vg,j+x,yDest+y, vgGetDot(vgHoriz, i, j));
	    /*vgDot(vg,j+x,yDest+y, ((int)mgGetDot(mgHoriz,i,j)));*/
	    }
	}
    vgClose(&vgHoriz);
    /*mgFree(&mgHoriz);*/
    }
}

#ifdef NOT_NEEDED
and should be corrected if it is needed ...
static void verticalText(struct vGfx *vg, int x, int y, int colorIx,
    MgFont *font, char *string)
/* Draw a vertical line of text with upper left corner x,y.
 *	The string ends at that upper coordinate y, thus the 
 *	string reads from bottom up.
 */
{
int textWidth = mgFontStringWidth(font, string);
int fontHeight = mgFontLineHeight(font);
int xClip = textWidth;
int yClip = fontHeight;
struct vGfx *vgHoriz;

CLIP(xClip,vg->width);
CLIP(yClip,vg->height);
/*	can't be larger than these either after rotating 90 degrees	*/
CLIP(xClip, vg->height);
CLIP(yClip, vg->width);

if ((yClip > 0) && (xClip > 0))
    {
    int i, j;
    int xx, yy;
    int dot;
    vgHoriz = vgOpenGif(xClip, yClip, "/dev/null", FALSE);
    vgText(vgHoriz, 0, 0, colorIx, font, string);
    for (i = 0; i < xClip; ++i)
	{
	yy = xClip - i;
	for (j = 0; j < yClip; ++j)
	    {
	    xx = j;
	    dot = vgGetDot(vgHoriz, i, j);
	    vgDot(vg,xx+x,yy+y, dot);
	    }
	}
    vgClose(&vgHoriz);
    }
}
#endif

static void addLabels(struct vGfx *vg, MgFont *font, char *minXStr,
    char *maxXStr, char *minYStr, char *maxYStr, char *shortLabel,
	char *longLabel, int totalWidth, int totalHeight, int leftMargin,
	    int fontHeight, char *vertLabel)
{
int x1 = 0;
int y1 = 0;
int x2 = 0;
int y2 = 0;
int longLabelSize = 0;
int textWidth = 0;

/*	border around the whole thing	*/
vgLine(vg, 0, 0, totalWidth-1, 0, MG_MAGENTA);	/*	top	*/
vgLine(vg, 0, 0, 0, totalHeight-1, MG_MAGENTA);	/*	left	*/
vgLine(vg, totalWidth-1, 0, totalWidth-1, totalHeight-1, MG_MAGENTA); /* right */
vgLine(vg, 0, totalHeight-1, totalWidth-1, totalHeight-1, MG_MAGENTA);/* bottom */

/*	border just around the graph	*/
vgLine(vg, leftMargin-PLOT_MARGIN, PLOT_MARGIN, leftMargin-PLOT_MARGIN,
	PLOT_MARGIN+GRAPH_HEIGHT, MG_CYAN); /* left */
#ifdef NOT
vgLine(vg, leftMargin, PLOT_MARGIN, totalWidth-PLOT_MARGIN,
	PLOT_MARGIN, MG_BLACK); /* top */
vgLine(vg, totalWidth-PLOT_MARGIN, PLOT_MARGIN, /* right */
	totalWidth-PLOT_MARGIN, PLOT_MARGIN+GRAPH_HEIGHT, MG_BLACK);
#endif
vgLine(vg, leftMargin, PLOT_MARGIN+GRAPH_HEIGHT+1,
	totalWidth-PLOT_MARGIN, PLOT_MARGIN+GRAPH_HEIGHT+1, MG_CYAN);
						/*	bottom	*/

x1 = leftMargin;
y1 = totalHeight-PLOT_MARGIN-fontHeight;
x2 = totalWidth-PLOT_MARGIN;
y2 = totalHeight-PLOT_MARGIN;

if (longLabel)
    longLabelSize = mgFontStringWidth(font, longLabel);

if ((longLabel != NULL) && (longLabelSize < (x2-x1)))
    vgTextCentered(vg, x1, y1, x2-x1, y2-y1, MG_BLACK, font, longLabel);
else if (shortLabel != NULL)
    vgTextCentered(vg, x1, y1, x2-x1, y2-y1, MG_BLACK, font, shortLabel);

x1 = leftMargin;
y1 = totalHeight-PLOT_MARGIN-fontHeight-fontHeight+2;

vgText(vg, x1, y1, MG_BLACK, font, minXStr);

textWidth = mgFontStringWidth(font, maxXStr);
x1 = totalWidth - PLOT_MARGIN - textWidth;
vgText(vg, x1, y1, MG_BLACK, font, maxXStr);

x1 = 0;
x2 = leftMargin - PLOT_MARGIN;
y1 = PLOT_MARGIN;
y2 = PLOT_MARGIN + fontHeight;

if (maxYStr)
    vgTextRight(vg, x1, y1, x2-x1, y2-y1, MG_BLACK, font, maxYStr);

y1 = PLOT_MARGIN+GRAPH_HEIGHT-fontHeight;
y2 = PLOT_MARGIN+GRAPH_HEIGHT;
if (minYStr)
    vgTextRight(vg, x1, y1, x2-x1, y2-y1, MG_BLACK, font, minYStr);

x1 = PLOT_MARGIN;
y1 = PLOT_MARGIN + fontHeight;
x2 = leftMargin - PLOT_MARGIN - PLOT_MARGIN;
y2 = GRAPH_HEIGHT - fontHeight;
if (vertLabel)
    verticalTextCentered(vg, x1, y1, x2-x1, y2-y1, MG_BLACK, font, vertLabel);
}

static void ordinaryPlot(int **densityCounts, struct vGfx *vg,
    int totalHeight, int totalWidth, int leftMargin)
/* a simple point plot, not density, the inversion of the Y axis takes
 * place here.  leftMargin is what has been passed in, the top margin is
 * fixed at PLOT_MARGIN	*/
{
int i, j;

/*	the dots are going to overlap because of DOT_SIZE, but that is
 *	OK for this situation, they are all MG_BLACK
 */
for (j = 0; j < GRAPH_HEIGHT; ++j)
    for (i = 0; i < GRAPH_WIDTH; ++i)
	if (densityCounts[j][i])
	    {
	    vgBox(vg, i+leftMargin, PLOT_MARGIN+(GRAPH_HEIGHT-j),
		DOT_SIZE, DOT_SIZE, MG_BLACK);
	    }
	    /*	the (totalHeight-j) is the inversion of the Y axis */
}

static void densityPlot(int **densityCounts, struct vGfx *vg,
    int totalHeight, int totalWidth, int leftMargin)
/* density plot shading, the inversion of the Y axis takes place here.
 * leftMargin is what has been passed in, the top margin is
 * fixed at PLOT_MARGIN
 */
{
/* shade code borrowed from hgTracks	*/
int maxShade = 9;
Color shadesOfGray[10+1];
double logMin = 0.0;
double logMax = 0.0;
double logRange = 0.0;
double log_2 = log(2.0);
int i, j;
int densityMin = BIGNUM;
int densityMax = 0;

#define LOG2(x) (log(x)/log_2)

/*	the painted dots are twice as big as the data, so lower the
 *	resolution of this data by adding together adjacent counts
 */

for (j = 0; j < GRAPH_HEIGHT-1; j += 2)
    for (i = 0; i < GRAPH_WIDTH-1; i += 2)
	{
	int count = densityCounts[j][i];
	count += densityCounts[j+1][i];
	count += densityCounts[j][i+1];
	count += densityCounts[j+1][i+1];
	densityCounts[j][i] = count;
	densityCounts[j+1][i] = count;
	densityCounts[j][i+1] = count;
	densityCounts[j+1][i+1] = count;
	if (count > 0)
	    {
	    densityMin = min(densityMin,count);
	    densityMax = max(densityMax,count);
	    }
	}

//hPrintf("<P>residuals min,max: %d, %d</P>\n", densityMin, densityMax);
logMin = LOG2((double)densityMin);
logMax = LOG2((double)densityMax);
logRange = logMax - logMin;

//hPrintf("<P>residuals log2(min,max): %g, %g</P>\n", logMin, logMax);

/*	shades of gray borrowed from hgTracks.c	*/
for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    shadesOfGray[i] = vgFindColorIx(vg, rgb.r, rgb.g, rgb.b);
    }
shadesOfGray[maxShade+1] = MG_RED;

/* only need to draw every other one since they were reduced by two above */
for (j = 0; j < GRAPH_HEIGHT; ++j)
    for (i = 0; i < GRAPH_WIDTH; ++i)
	if (densityCounts[j][i])
	    {
	    Color color;
	    int level =
		((LOG2((double)densityCounts[j][i]) - logMin)*maxShade)
			    / logRange;
	    if (level <= 0) level = 1;
	    if (level > maxShade) level = maxShade;
	    color = shadesOfGray[level];
	    vgBox(vg, i+leftMargin, PLOT_MARGIN+(GRAPH_HEIGHT-j),
		DOT_SIZE, DOT_SIZE, color);
	    }
	    /*	the (GRAPH_HEIGHT-j) is the inversion of the Y axis */
}

static MgFont *fontSetup(int *height)
/* Select font from textSize in cart. */
{
#define textSizeVar "textSize"	/*	from hgTracks.h	*/
char *textSize = cartUsualString(cart, textSizeVar, "small");
MgFont *font = mgFontForSize(textSize);
if (height)
    *height = mgFontLineHeight(font);
return (font);
}

struct tempName *scatterPlot(struct trackTable *yTable,
    struct trackTable *xTable, struct dataVector *result, int *width,
	int *height)
/*	create scatter plot gif file in trash, return path name */
{
static struct tempName gifFileName;
MgFont *font = NULL;
struct dataVector *y = yTable->vSet;
struct dataVector *x = xTable->vSet;
double yMin = INFINITY;
double xMin = INFINITY;
double yMax = -INFINITY;
double xMax = -INFINITY;
double yRange = 0.0;
double xRange = 0.0;
struct lm *lm = lmInit(GRAPH_WIDTH);
int **densityCounts;	/*	densityCounts[GRAPH_HEIGHT] [GRAPH_WIDTH] */
int i, j;
struct vGfx *vg;
int pointsPlotted = 0;
int fontHeight = 0;
char minXStr[32];
char maxXStr[32];
char minYStr[32];
char maxYStr[32];
char bottomLabel[256];
int leftLabelSize = 0;
int bottomLabelSize = 0;
int leftMargin = 0;
int bottomMargin = 0;
int totalWidth = 0;
int totalHeight = 0;

font = fontSetup(&fontHeight);

/*	Initialize density plot count array	*/
/*	space for the row pointers, first	*/
lmAllocArray(lm, densityCounts, GRAPH_HEIGHT);
/*	then space for each row	*/
for (j = 0; j < GRAPH_HEIGHT; ++j)
    lmAllocArray(lm, densityCounts[j], GRAPH_WIDTH);
for (j = 0; j < GRAPH_HEIGHT; ++j)
    for (i = 0; i < GRAPH_WIDTH; ++i)
	densityCounts[j][i] = 0;

/*	find overall min and max	*/
for ( ; (y != NULL) && (x !=NULL); y = y->next, x=x->next)
    {
    yMin = min(yMin,y->min);
    xMin = min(xMin,x->min);
    yMax = max(yMax,y->max);
    xMax = max(xMax,x->max);
    }
y = yTable->vSet;
x = xTable->vSet;
yRange = yMax - yMin;
xRange = xMax - xMin;
if (xRange == 0.0)
	xRange = 1.0;
if (yRange == 0.0)
	yRange = 1.0;

safef(minXStr,ArraySize(minXStr), "%.4g", xMin);
safef(maxXStr,ArraySize(maxXStr), "%.4g", xMax);
safef(minYStr,ArraySize(minYStr), "%.4g", yMin);
safef(maxYStr,ArraySize(maxYStr), "%.4g", yMax);
/*	the axes labels will be horizontal, thus the widest establishes
 *	the left margin.
 */
leftLabelSize = max(mgFontStringWidth(font, minYStr),
    mgFontStringWidth(font, maxYStr));
/*	the vertical text may be even wider than the numbers	*/
leftLabelSize = max(leftLabelSize,fontHeight);
/*	The bottom label is one line for the axes labels, and a second
 *	line for the X axis table longLabel
 */
bottomLabelSize = fontHeight * 2;

/*	y data is the Y axis, x data is the X axis	*/
/*	Do not worry about the fact that the Y axis is 0 at the top
 *	here, that will get translated during the actual plot.
 */

for ( ; (y != NULL) && (x !=NULL); y = y->next, x=x->next)
    {
    int i;
    pointsPlotted += y->count;

    for( i = 0; i < y->count; ++i)
	{
	float yValue = y->value[i];
	float xValue = x->value[i];
	int x1 = ((xValue - xMin)/xRange) * GRAPH_WIDTH;
	int y1 = ((yValue - yMin)/yRange) * GRAPH_HEIGHT;

	CLIP(x1,GRAPH_WIDTH);
	CLIP(y1,GRAPH_HEIGHT);

	densityCounts[y1][x1]++;
	}
    }

leftMargin = PLOT_MARGIN + leftLabelSize + PLOT_MARGIN + PLOT_MARGIN;
bottomMargin = PLOT_MARGIN + bottomLabelSize + PLOT_MARGIN;
totalWidth = leftMargin + GRAPH_WIDTH + PLOT_MARGIN;
totalHeight = PLOT_MARGIN + GRAPH_HEIGHT + bottomMargin;

trashDirFile(&gifFileName, "hgtData", "hgtaScatter", ".gif");

vg = vgOpenPng(totalWidth, totalHeight, gifFileName.forCgi, FALSE);

/*	x,y, w,h, drawing area only	*/
vgSetClip(vg, leftMargin, PLOT_MARGIN, GRAPH_WIDTH, GRAPH_HEIGHT);

/*	more than 100,000 points, show them as density	*/
if (pointsPlotted > 100000)
    densityPlot(densityCounts, vg, totalWidth, totalHeight, leftMargin);
else
    ordinaryPlot(densityCounts, vg, totalWidth, totalHeight, leftMargin);


/*	draw regression line	*/
    {
    int x1 = leftMargin + (((xMin - xMin)/xRange) * GRAPH_WIDTH);
    int x2 = leftMargin + (((xMax - xMin)/xRange) * GRAPH_WIDTH);
    int y1 = 0;
    int y2 = 0;
    double y;
    y = (result->m * xMin) + result->b;
    y1 =  PLOT_MARGIN + (GRAPH_HEIGHT - (((y - yMin)/yRange) * GRAPH_HEIGHT));
    y = (result->m * xMax) + result->b;
    y2 =  PLOT_MARGIN + (GRAPH_HEIGHT - (((y - yMin)/yRange) * GRAPH_HEIGHT));
    if ((x1 != x2) || (y1 != y2))
	vgLine(vg, x1, y1, x2, y2, MG_RED);
    }

safef(bottomLabel,ArraySize(bottomLabel),"%s", xTable->shortLabel);

/*	allow the entire area to be drawn into for the labels	*/
vgSetClip(vg, 0, 0, totalWidth, totalHeight);

addLabels(vg, font, minXStr, maxXStr, minYStr, maxYStr,
    xTable->shortLabel, xTable->longLabel, totalWidth, totalHeight,
	leftMargin, fontHeight, yTable->shortLabel);

vgUnclip(vg);
vgClose(&vg);

lmCleanup(&lm);

if (width)
    *width = totalWidth;
if (height)
    *height = totalHeight;

return &gifFileName;
}

struct tempName *residualPlot(struct trackTable *yTable,
    struct trackTable *xTable, struct dataVector *result, double *F_statistic,
	double *fitMin, double *fitMax, int *width, int *height)
/*	create residual plot gif file in trash, return path name */
{
static struct tempName gifFileName;
struct dataVector *y = yTable->vSet;
struct dataVector *x = xTable->vSet;
double fittedMin = INFINITY;
double fittedMax = -INFINITY;
double yMin = INFINITY;
double xMin = INFINITY;
double yMax = -INFINITY;
double xMax = -INFINITY;
double residualMin = INFINITY;
double residualMax = -INFINITY;
double fittedRange = 0.0;
double residualRange = 0.0;
register double m = result->m;	/*	slope m	*/
register double b = result->b;	/*	intercept m	*/
struct vGfx *vg;
int resultIndex = 0;	/*	for reference within result	*/
boolean debugOn = FALSE;
double F = 0.0;
double MSR = 0.0;	/*	aka MSM	- mean square residual */
double SSE = 0.0;	/*	mean squared error	*/
double ySum = 0.0;
double yBar = 0.0;
struct lm *lm = lmInit(GRAPH_WIDTH);
int **densityCounts;	/*	densityCounts[GRAPH_HEIGHT] [GRAPH_WIDTH] */
int i, j;
int pointsPlotted = 0;
MgFont *font = NULL;
int fontHeight = 0;
char minXStr[32];
char maxXStr[32];
char minYStr[32];
char maxYStr[32];
int leftLabelSize = 0;
int bottomLabelSize = 0;
int leftMargin = 0;
int bottomMargin = 0;
int totalWidth = 0;
int totalHeight = 0;

font = fontSetup(&fontHeight);

if (result->count < 1300) debugOn = TRUE;

debugOn = FALSE;

if(debugOn)
    {
    hPrintf("<PRE>\n");
    hPrintf("#position, x, y, fitted, residual\n");
    }

/*	Initialize density plot count array	*/
/*	space for the row pointers, first	*/
lmAllocArray(lm, densityCounts, GRAPH_HEIGHT);
/*	then space for each row	*/
for (j = 0; j < GRAPH_HEIGHT; ++j)
    lmAllocArray(lm, densityCounts[j], GRAPH_WIDTH);
for (j = 0; j < GRAPH_HEIGHT; ++j)
    for (i = 0; i < GRAPH_WIDTH; ++i)
	densityCounts[j][i] = 0;

/*	find overall min and max for the "fitted" values	*/
for (y = yTable->vSet, x = xTable->vSet ;
	(y != NULL) && (x !=NULL); y = y->next, x=x->next)
    {
    yMin = min(yMin,y->min);
    xMin = min(xMin,x->min);
    yMax = max(yMax,y->max);
    xMax = max(xMax,x->max);
    }

fittedMin = (m * xMin) + b;
fittedMax = (m * xMax) + b;
residualMin = result->min;
residualMax = result->max;
fittedRange = fittedMax - fittedMin;
residualRange = residualMax - residualMin;

safef(minXStr,ArraySize(minXStr), "%.4g", fittedMin);
safef(maxXStr,ArraySize(maxXStr), "%.4g", fittedMax);
safef(minYStr,ArraySize(minYStr), "%.4g", residualMin);
safef(maxYStr,ArraySize(maxYStr), "%.4g", residualMax);
/*	the axes labels will be horizontal, thus the widest establishes
 *	the left margin.
 */
leftLabelSize = max(mgFontStringWidth(font, minYStr),
    mgFontStringWidth(font, maxYStr));
/*	the vertical text may be even wider than the numbers	*/
leftLabelSize = max(leftLabelSize,fontHeight);
/*	The bottom label is one line for the axes labels, and a second
 *	line for the X axis table longLabel
 */
bottomLabelSize = fontHeight * 2;

for (y = yTable->vSet; y != NULL; y = y->next)
    {
    ySum += y->sumData;
    }

yBar = ySum / result->count;

/*	Do not worry about the fact that the Y axis is 0 at the top
 *	here, that will get translated during the actual plot.
 */
for (y = yTable->vSet, x = xTable->vSet ; (y != NULL) && (x !=NULL);
	y=y->next, x=x->next)
    {
    int i;

    pointsPlotted += y->count;

    for( i = 0; i < x->count; ++i, ++resultIndex)
	{
	float residual = result->value[resultIndex];
	float xValue = x->value[i];
	float fitted = (m * xValue) + b;
	int x1 = ((fitted - fittedMin)/fittedRange) * GRAPH_WIDTH;
	int y1;
	if (residualRange != 0)
	    y1 = ((residual - residualMin)/residualRange) * GRAPH_HEIGHT;
	else
	    y1 = 0;

if(debugOn)
    hPrintf("%d\t%g\t%g\t%g\t%g\n", x->position[i], x->value[i], y->value[i], fitted, residual);

	MSR += (fitted - yBar) * (fitted - yBar);
	SSE += (y->value[i] - fitted) * (y->value[i] - fitted);

	CLIP(x1,GRAPH_WIDTH);
	CLIP(y1,GRAPH_HEIGHT);

	densityCounts[y1][x1]++;
	}
    }

leftMargin = PLOT_MARGIN + leftLabelSize + PLOT_MARGIN + PLOT_MARGIN;
bottomMargin = PLOT_MARGIN + bottomLabelSize + PLOT_MARGIN;
totalWidth = leftMargin + GRAPH_WIDTH + PLOT_MARGIN;
totalHeight = PLOT_MARGIN + GRAPH_HEIGHT + bottomMargin;

trashDirFile(&gifFileName, "hgtData", "hgtaResidual", ".gif");

vg = vgOpenPng(totalWidth, totalHeight, gifFileName.forCgi, FALSE);

/*	x,y, w,h, drawing area only	*/
vgSetClip(vg, leftMargin, PLOT_MARGIN, GRAPH_WIDTH, GRAPH_HEIGHT);

/*	more than 100,000 points, show them as density	*/
if (pointsPlotted > 100000)
    densityPlot(densityCounts, vg, totalWidth, totalHeight, leftMargin);
else
    ordinaryPlot(densityCounts, vg, totalWidth, totalHeight, leftMargin);


/*	draw y = 0.0 line	*/
    {
    int x1 = leftMargin;
    int x2 = leftMargin + GRAPH_WIDTH - PLOT_MARGIN;
    int y1, y2;
    if (residualRange != 0)
	y1 = PLOT_MARGIN + (GRAPH_HEIGHT -
                    (((0.0 - residualMin)/residualRange) * GRAPH_HEIGHT));
    else
	y1 = 0;
    y2 = y1;
    if ((x1 != x2) || (y1 != y2))
	vgLine(vg, x1, y1, x2, y2, MG_RED);
    }

/*	allow the entire area to be drawn into for the labels	*/
vgSetClip(vg, 0, 0, totalWidth, totalHeight);

addLabels(vg, font, minXStr, maxXStr, minYStr, maxYStr,
    "Fitted", NULL, totalWidth, totalHeight, leftMargin, fontHeight,
	"Residuals");

vgUnclip(vg);
vgClose(&vg);

if(debugOn)
    hPrintf("</PRE>\n");

if (F_statistic)
    {
    if ((result->count - 2) > 0)
	F = MSR / (SSE / (result->count - 2));
    else
	F = 0.0;
    *F_statistic = F;
    }

lmCleanup(&lm);

if (fitMin)
    *fitMin = fittedMin;
if (fitMax)
    *fitMax = fittedMax;
if (width)
    *width = totalWidth;
if (height)
    *height = totalHeight;
    
return &gifFileName;
}

struct tempName *histogramPlot(struct trackTable *table,
    int *width, int *height)
/*	create histogram plot gif file in trash, return path name */
{
struct tempName *histoFileName;
struct vGfx *vg;
MgFont *font = NULL;
int totalWidth = 400;
int totalHeight = 300;
int fontHeight = 0;
int leftLabelSize = 0;
int leftMargin = 0;
int bottomMargin = 0;
int bottomLabelSize = 0;
char bottomLabelStr[128];
char minXStr[32];
char maxXStr[32];
char minYStr[32];
char maxYStr[32];
struct histoResult *histo = NULL;
struct dataVector *dv;
int totalCounts = 0;
unsigned maxBinCount = 0;
unsigned minBinCount = BIGNUM;
int binCountRange = 0;
int i;

for (dv = table->vSet; dv != NULL; dv = dv->next)
    {
    histo = histoGram(dv->value, (size_t) dv->count, NAN,
	(unsigned) GRAPH_WIDTH, NAN, table->min, table->max, histo);
    totalCounts += dv->count;
    }

if (histo == NULL)
    return NULL;

for (i = 0; i < histo->binCount; ++i)
    {
    maxBinCount = max(maxBinCount,histo->binCounts[i]);
    minBinCount = min(minBinCount,histo->binCounts[i]);
    }

binCountRange = maxBinCount - minBinCount;

if (histo->binCount != GRAPH_WIDTH)
    errAbort("histogram is not correctly sized ? %d vs %d",
	histo->binCount, GRAPH_WIDTH);

/*
struct histoResult *histoGram(float *values, size_t N, float binSize,
        unsigned binCount, float minValue, float min, float max,
        struct histoResult *accumHisto);
*/

AllocVar(histoFileName);
trashDirFile(histoFileName, "hgtData", "hgtaHisto", ".gif");

safef(bottomLabelStr,ArraySize(bottomLabelStr), "# of data values: %d",
	totalCounts);
safef(minYStr,ArraySize(minYStr), "%u", minBinCount);
safef(maxYStr,ArraySize(maxYStr), "%u", maxBinCount);
safef(minXStr,ArraySize(minXStr), "%.4g", table->min);
safef(maxXStr,ArraySize(maxXStr), "%.4g", table->max);

font = fontSetup(&fontHeight);
fontHeight = mgFontLineHeight(font);
/*	the vertical text size depends on the labels	*/
leftLabelSize = max(mgFontStringWidth(font, minYStr),
    mgFontStringWidth(font, maxYStr));
/*	or perhaps the fontHeight	*/
leftLabelSize = max(leftLabelSize,fontHeight);
/*	the horizontal text is two label height */
bottomLabelSize = fontHeight * 2;

leftMargin = PLOT_MARGIN + leftLabelSize + PLOT_MARGIN + PLOT_MARGIN;
bottomMargin = PLOT_MARGIN + bottomLabelSize + PLOT_MARGIN;
totalWidth = leftMargin + GRAPH_WIDTH + PLOT_MARGIN;
totalHeight = PLOT_MARGIN + GRAPH_HEIGHT + bottomMargin;

vg = vgOpenPng(totalWidth, totalHeight, histoFileName->forCgi, FALSE);

/*	x,y, w,h, drawing area only	*/
vgSetClip(vg, leftMargin, PLOT_MARGIN, GRAPH_WIDTH, GRAPH_HEIGHT);

if (binCountRange > 0)
    {
    int y1 = PLOT_MARGIN + GRAPH_HEIGHT;
    for (i = 0; i < histo->binCount; ++i)
	{
	int x1 = leftMargin + i;
	int y2 = PLOT_MARGIN + GRAPH_HEIGHT - (GRAPH_HEIGHT *
	    ((double)(histo->binCounts[i] - minBinCount) /
		(double)binCountRange));
	if (y1 != y2)
	    vgLine(vg, x1, y1, x1, y2, MG_BLACK);	/*	top	*/
	}
    }

/*	allow the entire area to be drawn into for the labels	*/
vgSetClip(vg, 0, 0, totalWidth, totalHeight);

addLabels(vg, font, minXStr, maxXStr, minYStr, maxYStr,
    bottomLabelStr, NULL, totalWidth, totalHeight, leftMargin, fontHeight,
	"counts in bin");

vgUnclip(vg);
vgClose(&vg);

freeHistoGram(&histo);

if (width)
    *width = totalWidth;
if (height)
    *height = totalHeight;

return histoFileName;
}
