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
int boxWidth = 11;
int boxHeight = 200;
int boxPad = 2;
int margin = 50;

/* Dimensions of image overall and our portion within it. */
int imageWidth, imageHeight, innerWidth, innerHeight, innerXoff, innerYoff;

void setImageDims(int expCount)
/* Set up image according to count */
{
innerHeight = boxHeight + 2*boxPad;
innerWidth = boxPad + expCount*(boxPad + boxWidth);
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
   vals[i] = mean + r;
   }
return sv;
}

void drawPointSmear(struct hvGfx *hvg, int xOff, int yOff, int height, 
    struct sampleVals *sv, double maxExp, int colorIx)
/* Draw a point for each sample value. */
{
int i, size = sv->size;
double *vals = sv->vals;
for (i=0; i<size; ++i)
    {
    double exp = vals[i]/maxExp;
    int expHeight = exp * height;
    if (expHeight < 1)
        expHeight = 1;
    hvGfxBox(hvg, xOff-1, yOff + height - expHeight, 3, 1, colorIx);
    hvGfxDot(hvg, xOff, yOff + height - expHeight - 1, colorIx);
    hvGfxDot(hvg, xOff, yOff + height - expHeight + 1, colorIx);
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
    double exp = sampleValsMean(sv)/maxExp;
    int expHeight = boxHeight * exp;
    int fillColorIx = hvGfxFindColorIx(hvg, fillColor.r, fillColor.g, fillColor.b);
    if (sv->size > 1)
	drawOutlinedBox(hvg, x, y + boxHeight - expHeight, boxWidth, expHeight, 
		fillColorIx, lineColorIx);
    drawPointSmear(hvg, x+boxWidth/2, y, boxHeight, sv, maxExp, lineColorIx);
    x += boxWidth + boxPad;
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
float maxExp = maxFloat(expCount, expVals);
uglyf("Got %d values, max is %g\n", expCount, maxExp);

if (expCount > 50)
    expCount = 50;
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
    int rCount = rand() % 8 + 2;
    int rRad = rand() % 1000 + 1;
    sv = randomAround(expVals[i], rRad, rCount);
    slAddHead(&svList, sv);
    }
slReverse(&svList);
plotVals(hvg, innerXoff + boxPad, innerYoff + boxPad, svList, maxExp);

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
