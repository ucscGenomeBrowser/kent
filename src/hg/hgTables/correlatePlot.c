/* scatterPlot - produce scatter plot graphs */

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

static char const rcsid[] = "$Id: correlatePlot.c,v 1.4 2005/07/01 23:35:23 hiram Exp $";

static void ordinaryPlot(int **densityCounts, struct vGfx *vg)
/* a simple point plot, not density	*/
{
int i, j;

for (j = 0; j < PLOT_HEIGHT; ++j)
    for (i = 0; i < PLOT_WIDTH; ++i)
	if (densityCounts[j][i])
	    {
	    vgBox(vg, i, j, DOT_SIZE, DOT_SIZE, MG_BLACK);
	    }
}

static void densityPlot(int **densityCounts, struct vGfx *vg)
/* density plot shading	*/
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
int densityRange = 0;

#define LOG2(x) (log(x)/log_2)

for (j = 0; j < PLOT_HEIGHT; ++j)
    for (i = 0; i < PLOT_WIDTH; ++i)
	if (densityCounts[j][i])
	    {
	    densityMin = min(densityMin,densityCounts[j][i]);
	    densityMax = max(densityMax,densityCounts[j][i]);
	    }
logMin = LOG2((double)densityMin);
logMax = LOG2((double)densityMax);
logRange = logMax - logMin;

for (i=0; i<=maxShade; ++i)
    {
    struct rgbColor rgb;
    int level = 255 - (255*i/maxShade);
    if (level < 0) level = 0;
    rgb.r = rgb.g = rgb.b = level;
    shadesOfGray[i] = vgFindColorIx(vg, rgb.r, rgb.g, rgb.b);
    }
shadesOfGray[maxShade+1] = MG_RED;

densityRange = densityMax - densityMin;

for (j = 0; j < PLOT_HEIGHT; ++j)
    for (i = 0; i < PLOT_WIDTH; ++i)
	if (densityCounts[j][i])
	    {
	    Color color;
	    int level =
		((LOG2((double)densityCounts[j][i]) - logMin)*maxShade)
			    / logRange;
	    if (level <= 0) level = 1;
	    if (level > maxShade) level = maxShade;
	    color = shadesOfGray[level];
	    vgBox(vg, i, j, DOT_SIZE, DOT_SIZE, color);
	    }
}


struct tempName *scatterPlot(struct trackTable *yTable,
    struct trackTable *xTable, struct dataVector *result)
/*	create scatter plot gif file in trash, return path name */
{
static struct tempName gifFileName;
char title[64];
MgFont *font = mgSmallFont();
int textWidth = 0;
struct dataVector *y = yTable->vSet;
struct dataVector *x = xTable->vSet;
double yMin = INFINITY;
double xMin = INFINITY;
double yMax = -INFINITY;
double xMax = -INFINITY;
double yRange = 0.0;
double xRange = 0.0;
int plotWidth = PLOT_WIDTH - (PLOT_MARGIN * 2);
int plotHeight = PLOT_HEIGHT - (PLOT_MARGIN * 2);
struct lm *lm = lmInit(PLOT_WIDTH);
int **densityCounts;	/*	densityCounts[PLOT_HEIGHT] [PLOT_WIDTH] */
int i, j;
struct vGfx *vg;
int pointsPlotted = 0;

/*	Initialize density plot count array	*/
/*	space for the row pointers, first	*/
lmAllocArray(lm, densityCounts, PLOT_HEIGHT);
/*	then space for each row	*/
for (j = 0; j < PLOT_HEIGHT; ++j)
    lmAllocArray(lm, densityCounts[j], PLOT_WIDTH);
for (j = 0; j < PLOT_HEIGHT; ++j)
    for (i = 0; i < PLOT_WIDTH; ++i)
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

makeTempName(&gifFileName, "hgtaPlot", ".gif");

vg = vgOpenGif(PLOT_WIDTH, PLOT_HEIGHT, gifFileName.forCgi);

/*	x,y, w,h	*/
vgSetClip(vg, 0, 0, PLOT_WIDTH, PLOT_HEIGHT);

safef(title,ArraySize(title), "ranges %g, %g", yRange, xRange);
textWidth = mgFontStringWidth(font, title);
//vgTextCentered(vg, 0, 0, PLOT_WIDTH, PLOT_HEIGHT, MG_BLACK, font, title);

/*	y data is the Y axis, x data is the X axis	*/
/*	Remember, the Y axis on the plot is 0 at top, yMax at bottom,
 *	the points get translated by the 'plotHeight - y' operation. */

for ( ; (y != NULL) && (x !=NULL); y = y->next, x=x->next)
    {
    int i;
    pointsPlotted += y->count;

    for( i = 0; i < y->count; ++i)
	{
	float yValue = y->value[i];
	float xValue = x->value[i];
	int x1 = PLOT_MARGIN +
		(((xValue - xMin)/xRange) * plotWidth);
	int y1 = PLOT_MARGIN +
		(plotHeight - (((yValue - yMin)/yRange) * plotHeight));

	densityCounts[y1][x1]++;
	}
    }

/*	more than 100,000 points, show them as density	*/
if (pointsPlotted > 100000)
    densityPlot(densityCounts, vg);
else
    ordinaryPlot(densityCounts, vg);

/*	draw regression line	*/
    {
    int x1 = PLOT_MARGIN + (((xMin - xMin)/xRange) * plotWidth);
    int x2 = PLOT_MARGIN + (((xMax - xMin)/xRange) * plotWidth);
    int y1 = plotHeight - PLOT_MARGIN;
    int y2 = PLOT_MARGIN;
    double y;
    y = (result->m * xMin) + result->b;
    y1 =  PLOT_MARGIN + (plotHeight - (((y - yMin)/yRange) * plotHeight));
    y = (result->m * xMax) + result->b;
    y2 =  PLOT_MARGIN + (plotHeight - (((y - yMin)/yRange) * plotHeight));
    vgLine(vg, x1, y1, x2, y2, MG_RED);
    }

vgUnclip(vg);
vgClose(&vg);

lmCleanup(&lm);

return &gifFileName;
}

struct tempName *residualPlot(struct trackTable *yTable,
    struct trackTable *xTable, struct dataVector *result, double *F_statistic,
	double *fitMin, double *fitMax)
/*	create residual plot gif file in trash, return path name */
{
static struct tempName gifFileName;
char title[64];
MgFont *font = mgSmallFont();
int textWidth = 0;
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
double yRange = 0.0;
double xRange = 0.0;
double fittedRange = 0.0;
double residualRange = 0.0;
register double m = result->m;	/*	slope m	*/
register double b = result->b;	/*	intercept m	*/
int plotWidth = PLOT_WIDTH - (PLOT_MARGIN * 2);
int plotHeight = PLOT_HEIGHT - (PLOT_MARGIN * 2);
struct vGfx *vg;
int resultIndex = 0;	/*	for reference within result	*/
boolean debugOn = FALSE;
double F = 0.0;
double MSR = 0.0;	/*	aka MSM	- mean square residual */
double SSE = 0.0;	/*	mean squared error	*/
double ySum = 0.0;
double yBar = 0.0;
struct lm *lm = lmInit(PLOT_WIDTH);
int **densityCounts;	/*	densityCounts[PLOT_HEIGHT] [PLOT_WIDTH] */
int i, j;
int pointsPlotted = 0;

if (result->count < 1300) debugOn = TRUE;

debugOn = FALSE;

if(debugOn)
    {
    hPrintf("<PRE>\n");
    hPrintf("#position, x, y, fitted, residual\n");
    }

/*	Initialize density plot count array	*/
/*	space for the row pointers, first	*/
lmAllocArray(lm, densityCounts, PLOT_HEIGHT);
/*	then space for each row	*/
for (j = 0; j < PLOT_HEIGHT; ++j)
    lmAllocArray(lm, densityCounts[j], PLOT_WIDTH);
for (j = 0; j < PLOT_HEIGHT; ++j)
    for (i = 0; i < PLOT_WIDTH; ++i)
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
yRange = yMax - yMin;
xRange = xMax - xMin;
fittedRange = fittedMax - fittedMin;
residualRange = residualMax - residualMin;

makeTempName(&gifFileName, "hgtaPlot", ".gif");

vg = vgOpenGif(PLOT_WIDTH, PLOT_HEIGHT, gifFileName.forCgi);

/*	x,y, w,h	*/
vgSetClip(vg, 0, 0, PLOT_WIDTH, PLOT_HEIGHT);

safef(title,ArraySize(title), "ranges %g, %g", yRange, xRange);
textWidth = mgFontStringWidth(font, title);
//vgTextCentered(vg, 0, 0, PLOT_WIDTH, PLOT_HEIGHT, MG_BLACK, font, title);


for (y = yTable->vSet; y != NULL; y = y->next)
    {
    ySum += y->sumData;
    }

yBar = ySum / result->count;

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
	int x1 = PLOT_MARGIN + (((fitted - fittedMin)/fittedRange) * plotWidth);
	int y1 = PLOT_MARGIN + (plotHeight -
		    (((residual - residualMin)/residualRange) * plotHeight));


if(debugOn)
    hPrintf("%d\t%g\t%g\t%g\t%g\n", x->position[i], x->value[i], y->value[i], fitted, residual);

	MSR += (fitted - yBar) * (fitted - yBar);
	SSE += (y->value[i] - fitted) * (y->value[i] - fitted);

	densityCounts[y1][x1]++;
	}
    }

/*	more than 100,000 points, show them as density	*/
if (pointsPlotted > 100000)
    densityPlot(densityCounts, vg);
else
    ordinaryPlot(densityCounts, vg);

if ((result->count - 2) > 0)
    F = MSR / (SSE / (result->count - 2));

/*	draw y = 0.0 line	*/
    {
    int x1 = PLOT_MARGIN;
    int x2 = plotWidth - PLOT_MARGIN;
    int y1 = PLOT_MARGIN + (plotHeight -
                    (((0.0 - residualMin)/residualRange) * plotHeight));
    int y2 = y1;
    vgLine(vg, x1, y1, x2, y2, MG_RED);
    }


vgUnclip(vg);
vgClose(&vg);

if(debugOn)
    hPrintf("</PRE>\n");

if (F_statistic)
    *F_statistic = F;

lmCleanup(&lm);

if (fitMin)
    *fitMin = fittedMin;
if (fitMax)
    *fitMax = fittedMax;
    
return &gifFileName;
}
