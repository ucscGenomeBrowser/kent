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

boolean sortVal = FALSE;
int barWidth = 13;
int graphHeight = 400;
int margin = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tryGtexBoxPlot - Test program to make box plots for GTEx\n"
  "usage:\n"
  "   tryGtexBoxPlot style geneNumber output.png\n"
  "Where style is either 'Tukay' or 'Bar' or 'barDown' and geneNumber is between 0 and 20000\n"
  "options:\n"
  "   -sortVal - if TRUE sort values first\n"
  "   -barWidth=N - set width of bar in pixels, default %d\n"
  "   -height=N - set height of graph in pixels, default %d\n"
  "   -margin=N - blank margin area around graph, default %d\n"
  , barWidth, graphHeight, margin
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"sortVal", OPTION_BOOLEAN},
   {"barWidth", OPTION_INT},
   {"height", OPTION_INT},
   {"margin", OPTION_INT},
   {NULL, 0},
};


/* Dimensions of a image parts */
int tukPad = 2;

/* Dimensions of image overall and our portion within it. */
int imageWidth, imageHeight, innerWidth, innerHeight, innerXoff, innerYoff;

void setImageDims(int expCount)
/* Set up image according to count */
{
innerHeight = graphHeight + 2*tukPad;
innerWidth = tukPad + expCount*(tukPad + barWidth);
imageWidth = innerWidth + 2*margin;
imageHeight = innerHeight + 2*margin;
innerXoff = margin;
innerYoff = margin;
}

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

struct sampleVals
/* An array of double values. */
    {
    struct sampleVals *next;
    int size;	   /* # of values. */
    double *vals;  /* Sample values, alloced to be size big */
    double q1,q3,median, minVal, maxVal;   /* Stats we gather */
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

void svCalcStats(struct sampleVals *sv)
/* Fill in stats portion of sv. */
{
doubleBoxWhiskerCalc(sv->size, sv->vals, &sv->minVal, &sv->q1, &sv->median, &sv->q3, &sv->maxVal);
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
   if ((rand() & 0x3f) == 0)
        x += 10*r;
       
   if (x < 0) x = 0;
   vals[i] = x;
   }
svCalcStats(sv);
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

void svBar(struct hvGfx *hvg, int fillColorIx, int lineColorIx, int x, int y, 
    struct sampleVals *sv, double maxExp)
/* Draw a bar graph bar up to median value. */
{
int yMedian = valToY(sv->median, maxExp, graphHeight) + y;
int yZero = valToY(0, maxExp, graphHeight) + y;
hvGfxOutlinedBox(hvg, x, yMedian, barWidth, yZero - yMedian, 
	fillColorIx, lineColorIx);
}

void svBarDown(struct hvGfx *hvg, int fillColorIx, int lineColorIx, int x, int y, 
    struct sampleVals *sv, double maxExp)
/* Draw a bar graph bar up to median value. */
{
double scaled = sv->median/maxExp;
int yMedian = scaled * (graphHeight-1);
hvGfxOutlinedBox(hvg, x, y, barWidth, yMedian, 
	fillColorIx, lineColorIx);
}

int svCmpMedianRev(const void *va, const void *vb)
/* Compare two slNames, ignore case. Big medians end up first*/
{
const struct sampleVals *a = *((struct sampleVals **)va);
const struct sampleVals *b = *((struct sampleVals **)vb);
double diff = b->median - a->median;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else
    return 0;
}


void svTukay(struct hvGfx *hvg, int fillColorIx, int lineColorIx, int x, int y, 
    struct sampleVals *sv, double maxExp)
/* Draw a Tukey-type box plot */
{
int medianColorIx = lineColorIx;
int whiskColorIx = lineColorIx;
double xCen = x + barWidth/2;
if (sv->size > 1)
    {
    /* Cache a few fields from sv in local variables */
    double q1 = sv->q1, q3 = sv->q3,  median = sv->median;

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
    if (whisk1 < sv->minVal)
        whisk1 = sv->minVal;
    if (whisk2 > sv->maxVal)
        whisk2 = sv->maxVal;

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
    drawPointSmear(hvg, xCen, y, graphHeight, sv, maxExp, lineColorIx, whisk1, whisk2);
    }
}

void plotVals(struct hvGfx *hvg, int xOff, int yOff, char *style,
    struct sampleVals *svList, double maxExp)
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
    if (sameWord(style, "tukay")) 
	svTukay(hvg, fillColorIx, lineColorIx, x, y, sv, maxExp);
    else if (sameWord(style, "bar"))
	svBar(hvg, fillColorIx, lineColorIx, x, y, sv, maxExp);
    else if (sameWord(style, "barDown"))
	svBarDown(hvg, fillColorIx, lineColorIx, x, y, sv, maxExp);
    else
        errAbort("Unrecognized style %s", style);
    x += barWidth + tukPad;
    }
}

void tryGtexBoxPlot(char *style, char *geneNumberString, char *outPng)
/* tryGtexBoxPlot - Test program to make box plots for GTEx. */
{
/* Get gene name - either directly or by lookup */
struct sqlConnection *conn = sqlConnect("hgFixed");
char *geneName = geneNumberString;
if (!endsWith(geneName, "_at"))
    {
    int geneNumber = sqlUnsigned(geneNumberString);
    /* Convert gene number to gene name */
    char query[512];
    sqlSafef(query, sizeof(query), "select name from gnfHumanAtlas2Median limit 1 offset %d", 
	geneNumber);
    uglyf("query %s\n", query);
    geneName = sqlQuickString(conn, query);
    uglyf("GeneNumber %d, geneName %s\n", geneNumber, geneName);
    }


/* Get a data row */
int expCount = 0;
float *expVals = NULL;
if (!hgExpLoadVals(NULL, conn, NULL, geneName, "gnfHumanAtlas2Median", &expCount, &expVals))
    errAbort("Can't load data");

if (expCount > 45)
    expCount = 45;
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
    int rCount = rand() % 8 + 3;
    int rRad = rand() % 2000 + 100;
    sv = randomAround(expVals[i], rRad, rCount);
    svCalcStats(sv);
    slAddHead(&svList, sv);
    }
slReverse(&svList);
if (sortVal)
    slSort(&svList, svCmpMedianRev);

double maxExp = sampleValsListMax(svList);
uglyf("Got %d values, max is %g\n", expCount, maxExp);
plotVals(hvg, innerXoff + tukPad, innerYoff + tukPad, style, svList, maxExp);

hvGfxClose(&hvg);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
sortVal = optionExists("sortVal");
barWidth = optionInt("barWidth", barWidth);
graphHeight = optionInt("height", graphHeight);
margin = optionInt("margin", margin);
tryGtexBoxPlot(argv[1], argv[2], argv[3]);
return 0;
}
