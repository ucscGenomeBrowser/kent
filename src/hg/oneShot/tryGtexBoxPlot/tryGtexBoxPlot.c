/* tryGtexBoxPlot - Test program to make box plots for GTEx. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cart.h"
#include "hgExp.h"
#include "hvGfx.h"
#include "rainbow.h"
#include "math.h"

/* Dimensions of a image parts */
int tukWidth = 11;
int tukHeight = 400;
int tukPad = 2;
int margin = 50;

/* Dimensions of image overall and our portion within it. */
int imageWidth, imageHeight, innerWidth, innerHeight, innerXoff, innerYoff;

void setImageDims(int expCount)
/* Set up image according to count */
{
innerHeight = tukHeight + 2*tukPad;
innerWidth = tukPad + expCount*(tukPad + tukWidth);
imageWidth = innerWidth + 2*margin;
imageHeight = innerHeight + 2*margin;
innerXoff = margin;
innerYoff = margin;
}



void usage()
/* Explain usage and exit. */
{
errAbort(
  "tryGtexBoxPlot - Test program to make box plots for GTEx\n"
  "usage:\n"
  "   tryGtexBoxPlot input.txt output.png\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

float maxFloat(int count, float *array)
/* Return max value in array */
{
float val = array[0];
int i;
for (i=1; i<count; ++i)
    {
    float x = array[i];
    if (x > val)
        val = x;
    }
return val;
}

void drawOutlinedBox(struct hvGfx *hvg, int x, int y, int width, int height, 
    int fillColorIx, int lineColorIx)
/* Draw a box in fillColor outlined by lineColor */
{
/* If nothing to draw bail out early. */
if (width <= 0 || height <= 0)
    return;

/* Draw outline first, it may be all we need. We may not even need all 4 lines of it. */
hvGfxBox(hvg, x, y, width, 1, lineColorIx);
if (height == 1)
    return;
hvGfxBox(hvg, x, y+1, 1, height-1, lineColorIx);
if (width == 1)
    return;
hvGfxBox(hvg, x+width-1, y+1, 1, height-1, lineColorIx);
if (width == 2)
    return;
hvGfxBox(hvg, x+1, y+height-1, width-2, 1, lineColorIx);
if (height == 2)
    return;

/* Can draw fill with a single box. */
hvGfxBox(hvg, x+1, y+1, width-2, height-2, fillColorIx);
}

struct sampleVals
/* An array of double values. */
    {
    struct sampleVals *next;
    int size;	   /* # of values. */
    double *vals;  /* Sample values, alloced to be size big */
    };

double sampleValsMean(struct sampleVals *sv)
/* Return mean sample value */
{
double *vals = sv->vals;
double sum = 0.0;
int i, size = sv->size;
for (i=0; i<size; ++i)
    sum += vals[i];
return sum/size;
}

struct sampleVals *randomAround(double mean, double radius, int count)
/* Make count sample vals around given mean with offset no further than radius */
{
struct sampleVals *sv;
AllocVar(sv);
double *vals = AllocArray(sv->vals, count);
sv->size = count;
int i;
for (i=0; i<count; ++i)
   {
   double r = (radius * rand()) / RAND_MAX;
   if (i&1)
       r = -r;
   double x = mean + r;
   if (x < 0) x = 0;
   vals[i] = x;
   }
return sv;
}

int valToY(double val, double maxVal, int height)
/* Convert a value from 0 to maxVal to 0 to height-1 */
{
double scaled = val/maxVal;
int y = scaled * (height-1);
return height - 1 - y;
}

void drawPointSmear(struct hvGfx *hvg, int xOff, int yOff, int height, 
    struct sampleVals *sv, double maxExp, int colorIx, double midClipMin, double midClipMax)
/* Draw a point for each sample value. */
{
int i, size = sv->size;
double *vals = sv->vals;
for (i=0; i<size; ++i)
    {
    double exp = vals[i];
    if (exp < midClipMin || exp > midClipMax)
	{
	int y = valToY(exp, maxExp, height) + yOff;
	uglyf("%d ", round(exp));
	hvGfxDot(hvg, xOff-1, y, colorIx);
	hvGfxDot(hvg, xOff+1, y, colorIx);
	hvGfxDot(hvg, xOff, y-1, colorIx);
	hvGfxDot(hvg, xOff, y+1, colorIx);
	}
    }
uglyf("\n");
}

double sampleValsMax(struct sampleVals *sv)
/* Return max sample val */
{
double *array = sv->vals;
int count = sv->size;
double val = array[0];
int i;
for (i=1; i<count; ++i)
    {
    double x = array[i];
    if (x > val)
        val = x;
    }
return val;
}

double sampleValsListMax(struct sampleVals *svList)
/* Return maximum value on list */
{
double val = 0;
struct sampleVals *sv;
for (sv = svList; sv != NULL; sv = sv->next)
    {
    double x = sampleValsMax(sv);
    if (x > val)
        val = x;
    }
return val;
}

void svTukay(struct hvGfx *hvg, int fillColorIx, int lineColorIx, int x, int y, 
    struct sampleVals *sv, double maxExp)
/* Draw a Tukey-type box plot */
{
int medianColorIx = lineColorIx;
int whiskColorIx = lineColorIx;
double q1 = 0,q3 = 0,median = 0, minVal, maxVal;
doubleBoxWhiskerCalc(sv->size, sv->vals, &minVal, &q1, &median, &q3, &maxVal);
double xCen = x + tukWidth/2;
uglyf("N %d\tq1 %g\tmedian %g\tq3 %g\tsamples %d\n", sv->size, q1, median, q3, sv->size);
if (sv->size > 1)
    {
    int yQ1 = valToY(q1, maxExp, tukHeight) + y;
    int yQ3 = valToY(q3, maxExp, tukHeight) + y;
    int yMedian = valToY(median, maxExp, tukHeight);
    double iq3 = q3 - median;
    double iq1 = median - q1;
    int qHeight = yQ1 - yQ3 + 1;
    drawOutlinedBox(hvg, x,  yQ3, tukWidth, qHeight, fillColorIx, lineColorIx);
#ifdef SOON
#endif /* SOON */
    double whisk1 = q1 - 1.5 * iq1;
    double whisk2 = q3 + 1.5 * iq3;
    if (whisk1 < minVal)
        whisk1 = minVal;
    if (whisk2 > maxVal)
        whisk2 = maxVal;
    int yWhisk1 = valToY(whisk1, maxExp, tukHeight) + y;
    int yWhisk2 = valToY(whisk2, maxExp, tukHeight) + y;
    assert(yQ1 <= yWhisk1);
    assert(yQ3 >= yWhisk2);
    hvGfxBox(hvg, xCen, yWhisk2, 1, yQ3 - yWhisk2, whiskColorIx);
    hvGfxBox(hvg, xCen, yQ1, 1, yWhisk1 - yQ1, whiskColorIx);
#ifdef SOON
#endif /* SOON */
    hvGfxBox(hvg, x, yWhisk1, tukWidth, 1, whiskColorIx);
    hvGfxBox(hvg, x, yWhisk2, tukWidth, 1, whiskColorIx);

#ifdef OLD_BAR_GRAPH
    drawOutlinedBox(hvg, x, y + tukHeight - expHeight, tukWidth, expHeight, 
	    fillColorIx, lineColorIx);
#endif /* OLD_BAR_GRAPH */
    hvGfxBox(hvg, x, y + yMedian, tukWidth, 1, medianColorIx);
    drawPointSmear(hvg, xCen, y, tukHeight, sv, maxExp, lineColorIx, q1, q3);
    }
}

void plotVals(struct hvGfx *hvg, int xOff, int yOff, struct sampleVals *svList, double maxExp)
/* Plot rainbow bar graphs */
{
int x=xOff, y=yOff;
int i;
int expCount = slCount(svList);
double invExpCount = 1.0/expCount;
struct sampleVals *sv = svList;
struct rgbColor lineColor = {.r=0};
int lineColorIx = hvGfxFindColorIx(hvg, lineColor.r, lineColor.g, lineColor.b);
for (i=0; i<expCount; ++i, sv = sv->next)
    {
    double colPos = invExpCount * i;
    struct rgbColor fillColor = saturatedRainbowAtPos(colPos);
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    svTukay(hvg, fillColorIx, lineColorIx, x, y, sv, maxExp);
    x += tukWidth + tukPad;
    }
}

void tryGtexBoxPlot(char *inFile, char *outPng)
/* tryGtexBoxPlot - Test program to make box plots for GTEx. */
{
/* Get a data row */
struct sqlConnection *conn = sqlConnect("hgFixed");
int expCount = 0;
float *expVals = NULL;
if (!hgExpLoadVals(NULL, conn, NULL, "1007_s_at", "gnfHumanAtlas2Median", &expCount, &expVals))
    errAbort("Can't load data");

if (expCount > 20)
    expCount = 20;
setImageDims(expCount);

/* Open graphics and draw boxes for boundaries of total region and drawable. */
struct hvGfx *hvg  = hvGfxOpenPng(imageWidth, imageHeight, outPng, TRUE);
hvGfxBox(hvg, 0, 0, imageWidth, imageHeight, MG_BLUE);
hvGfxBox(hvg, innerXoff, innerYoff, innerWidth, innerHeight, MG_WHITE);

/* Fake data */
struct sampleVals *svList = NULL, *sv;
int i;
for (i=0; i<expCount; ++i)
    {
    int rCount = rand() % 7 + 2;
    int rRad = rand() % 2000 + 100;
    sv = randomAround(expVals[i], rRad, rCount);
    slAddHead(&svList, sv);
    }
slReverse(&svList);

double maxExp = sampleValsListMax(svList);
uglyf("Got %d values, max is %g\n", expCount, maxExp);
plotVals(hvg, innerXoff + tukPad, innerYoff + tukPad, svList, maxExp);

hvGfxClose(&hvg);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
tryGtexBoxPlot(argv[1], argv[2]);
return 0;
}
