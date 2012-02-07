/* gapCalc - Stuff to calculate complex (but linear) gap costs quickly,
 * and read specifications from a file. */

#include "common.h"
#include "linefile.h"
#include "gapCalc.h"


struct gapCalc
/* A structure that bundles together stuff to help us
 * calculate gap costs quickly. */
    {
    int smallSize; /* Size of tables for doing quick lookup of small gaps. */
    int *qSmall;   /* Table for small gaps in q; */
    int *tSmall;   /* Table for small gaps in t. */
    int *bSmall;   /* Table for small gaps in either. */
    int *longPos;/* Table of positions to interpolate between for larger gaps. */
    double *qLong; /* Values to interpolate between for larger gaps in q. */
    double *tLong; /* Values to interpolate between for larger gaps in t. */
    double *bLong; /* Values to interpolate between for larger gaps in both. */
    int longCount;	/* Number of long positions overall in longPos. */
    int qPosCount;	/* Number of long positions in q. */
    int tPosCount;	/* Number of long positions in t. */
    int bPosCount;	/* Number of long positions in b. */
    int qLastPos;	/* Maximum position we have data on in q. */
    int tLastPos;	/* Maximum position we have data on in t. */
    int bLastPos;	/* Maximum position we have data on in b. */
    double qLastPosVal;	/* Value at max pos. */
    double tLastPosVal;	/* Value at max pos. */
    double bLastPosVal;	/* Value at max pos. */
    double qLastSlope;	/* What to add for each base after last. */
    double tLastSlope;	/* What to add for each base after last. */
    double bLastSlope;	/* What to add for each base after last. */
    };

/* These are the gap costs used in the Evolution's Cauldron paper. */
static char *originalGapCosts = 
    "tableSize 11\n"
    "smallSize 111\n"
    "position 1 2 3 11 111 2111 12111 32111 72111 152111 252111\n"
    "qGap 350 425 450 600 900 2900 22900 57900 117900 217900 317900\n"
    "tGap 350 425 450 600 900 2900 22900 57900 117900 217900 317900\n"
    "bothGap 750 825 850 1000 1300 3300 23300 58300 118300 218300 318300\n";

/* These gap costs work well at chicken/human distances, and seem
 * to do ok closer as well, so they are now the default. */
static char *defaultGapCosts =
"tablesize       11\n"
"smallSize       111\n"
"position        1       2       3       11      111     2111    12111   32111   72111   152111  252111\n"
"qGap    325     360     400     450     600     1100    3600    7600    15600   31600   56600\n"
"tGap    325     360     400     450     600     1100    3600    7600    15600   31600   56600\n"
"bothGap 625     660     700     750     900     1400    4000    8000    16000   32000   57000\n";

/* These gap costs are for query=mRNA, target=DNA. */
static char *rnaDnaGapCosts = 
"tablesize       12\n"
"smallSize       111\n"
"position        1       2       3       11     31   111   2111    12111   32111   72111   152111  252111\n"
       "qGap    325     360     400     450     600  800   1100    3600    7600    15600   31600   56600\n"
       "tGap    200     210     220     250     300  400   500      600     800    1200     2000   4000\n"
"       bothGap 625     660     700     750     900  1100  1400    4000    8000    16000   32000   57000\n";

static char *cheapGapCosts = 
    "tableSize 3\n"
    "smallSize 100\n"
    "position 1 100 1000\n"
    "qGap 0 30 300\n"
    "tGap 0 30 300\n"
    "bothGap 0 30 300\n";


char *gapCalcSampleFileContents()
/* Return contents of a sample linear gap file. */
{
return defaultGapCosts;
}

static int interpolate(int x, int *s, double *v, int sCount)
/* Find closest value to x in s, and then lookup corresponding
 * value in v.  Interpolate where necessary. */
{
int i, ds, ss;
double dv;
for (i=0; i<sCount; ++i)
    {
    ss = s[i];
    if (x == ss)
        return v[i];
    else if (x < ss)
        {
	ds = ss - s[i-1];
	dv = v[i] - v[i-1];
	return v[i-1] + dv * (x - s[i-1]) / ds;
	}
    }
/* If get to here extrapolate from last two values */
ds = s[sCount-1] - s[sCount-2];
dv = v[sCount-1] - v[sCount-2];
return v[sCount-2] + dv * (x - s[sCount-2]) / ds;
}

static double calcSlope(double y2, double y1, double x2, double x1)
/* Calculate slope of line from x1/y1 to x2/y2 */
{
return (y2-y1)/(x2-x1);
}

static void readTaggedNumLine(struct lineFile *lf, char *tag, 
	int count, int *intOut,  double *floatOut)
/* Read in a line that starts with tag and then has count numbers.
 * Complain and die if tag is unexpected or other problem occurs. 
 * Put output as integers and/or floating point into intOut and 
 * floatOut. */
{
char *line;
int i = 0;
char *word;
if (!lineFileNextReal(lf, &line))
   lineFileUnexpectedEnd(lf);
word = nextWord(&line);
if (!sameWord(tag, word))
    errAbort("Expecting %s got %s line %d of %s",
             tag, word, lf->lineIx, lf->fileName);
for (i = 0; i < count; ++i)
    {
    word = nextWord(&line);
    if (word == NULL)
        errAbort("Not enough numbers line %d of %s", lf->lineIx, lf->fileName);
    if (!isdigit(word[0]))
        errAbort("Expecting number got %s line %d of %s",
	         word, lf->lineIx, lf->fileName);
    if (intOut)
	intOut[i] = atoi(word);
    if (floatOut)
        floatOut[i] = atof(word);
    }
word = nextWord(&line);
if (word != NULL)
        errAbort("Too many numbers line %d of %s", lf->lineIx, lf->fileName);
}

struct gapCalc *gapCalcRead(struct lineFile *lf)
/* Create gapCalc from open file. */
{
int i, tableSize, startLong = -1;
struct gapCalc *gapCalc;
int *gapInitPos;  
double *gapInitQGap;  
double *gapInitTGap;  
double *gapInitBothGap;

AllocVar(gapCalc);

/* Parse file. */
readTaggedNumLine(lf, "tableSize", 1, &tableSize, NULL);
readTaggedNumLine(lf, "smallSize", 1, &gapCalc->smallSize, NULL);
AllocArray(gapInitPos,tableSize);
AllocArray(gapInitQGap,tableSize);
AllocArray(gapInitTGap,tableSize);
AllocArray(gapInitBothGap,tableSize);
readTaggedNumLine(lf, "position", tableSize, gapInitPos, NULL);
readTaggedNumLine(lf, "qGap", tableSize, NULL, gapInitQGap);
readTaggedNumLine(lf, "tGap", tableSize, NULL, gapInitTGap);
readTaggedNumLine(lf, "bothGap", tableSize, NULL, gapInitBothGap);

/* Set up precomputed interpolations for small gaps. */
AllocArray(gapCalc->qSmall, gapCalc->smallSize);
AllocArray(gapCalc->tSmall, gapCalc->smallSize);
AllocArray(gapCalc->bSmall, gapCalc->smallSize);
for (i=1; i<gapCalc->smallSize; ++i)
    {
    gapCalc->qSmall[i] = 
	interpolate(i, gapInitPos, gapInitQGap, tableSize);
    gapCalc->tSmall[i] = 
	interpolate(i, gapInitPos, gapInitTGap, tableSize);
    gapCalc->bSmall[i] = interpolate(i, gapInitPos, 
	gapInitBothGap, tableSize);
    }

/* Set up to handle intermediate values. */
for (i=0; i<tableSize; ++i)
    {
    if (gapCalc->smallSize == gapInitPos[i])
	{
	startLong = i;
	break;
	}
    }
if (startLong < 0)
    errAbort("No position %d in gapCalcRead()\n", gapCalc->smallSize);
gapCalc->longCount = tableSize - startLong;
gapCalc->qPosCount = tableSize - startLong;
gapCalc->tPosCount = tableSize - startLong;
gapCalc->bPosCount = tableSize - startLong;
gapCalc->longPos = cloneMem(gapInitPos + startLong, gapCalc->longCount * sizeof(int));
gapCalc->qLong = cloneMem(gapInitQGap + startLong, gapCalc->qPosCount * sizeof(double));
gapCalc->tLong = cloneMem(gapInitTGap + startLong, gapCalc->tPosCount * sizeof(double));
gapCalc->bLong = cloneMem(gapInitBothGap + startLong, gapCalc->bPosCount * sizeof(double));

/* Set up to handle huge values. */
gapCalc->qLastPos = gapCalc->longPos[gapCalc->qPosCount-1];
gapCalc->tLastPos = gapCalc->longPos[gapCalc->tPosCount-1];
gapCalc->bLastPos = gapCalc->longPos[gapCalc->bPosCount-1];
gapCalc->qLastPosVal = gapCalc->qLong[gapCalc->qPosCount-1];
gapCalc->tLastPosVal = gapCalc->tLong[gapCalc->tPosCount-1];
gapCalc->bLastPosVal = gapCalc->bLong[gapCalc->bPosCount-1];
gapCalc->qLastSlope = calcSlope(gapCalc->qLastPosVal, gapCalc->qLong[gapCalc->qPosCount-2],
			   gapCalc->qLastPos, gapCalc->longPos[gapCalc->qPosCount-2]);
gapCalc->tLastSlope = calcSlope(gapCalc->tLastPosVal, gapCalc->tLong[gapCalc->tPosCount-2],
			   gapCalc->tLastPos, gapCalc->longPos[gapCalc->tPosCount-2]);
gapCalc->bLastSlope = calcSlope(gapCalc->bLastPosVal, gapCalc->bLong[gapCalc->bPosCount-2],
			   gapCalc->bLastPos, gapCalc->longPos[gapCalc->bPosCount-2]);
freez(&gapInitPos);
freez(&gapInitQGap);
freez(&gapInitTGap);
freez(&gapInitBothGap);
return gapCalc;
}

struct gapCalc *gapCalcFromString(char *s)
/* Return gapCalc from description string. */
{
struct lineFile *lf = lineFileOnString("string", TRUE, cloneString(s));
struct gapCalc *gapCalc = gapCalcRead(lf);
lineFileClose(&lf);
return gapCalc;
}

struct gapCalc *gapCalcFromFile(char *fileName)
/* Return gapCalc from file. */
{
struct gapCalc *gapCalc = NULL;

if (sameString(fileName, "loose"))
    {
    verbose(2, "using loose linear gap costs (chicken/human)\n");
    gapCalc = gapCalcFromString(defaultGapCosts);
    }
else if (sameString(fileName, "medium"))
    {
    verbose(2, "using medium (original) linear gap costs (mouse/human)\n");
    gapCalc = gapCalcFromString(originalGapCosts);
    }
else
    {
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    gapCalc = gapCalcRead(lf);
    lineFileClose(&lf);
    }
return gapCalc;
}

struct gapCalc *gapCalcDefault()
/* Return default gapCalc. */
{
return gapCalcFromString(defaultGapCosts);
}

struct gapCalc *gapCalcRnaDna()
/* Return gaps suitable for RNA queries vs. DNA targets */
{
return gapCalcFromString(rnaDnaGapCosts);
}

struct gapCalc *gapCalcCheap()
/* Return cheap gap costs. */
{
return gapCalcFromString(cheapGapCosts);
}

struct gapCalc *gapCalcOriginal()
/* Return gap costs from original paper. */
{
return gapCalcFromString(originalGapCosts);
}

void gapCalcFree(struct gapCalc **pGapCalc)
/* Free up resources associated with gapCalc. */
{
struct gapCalc *gapCalc = *pGapCalc;
if (gapCalc != NULL)
    {
    freeMem(gapCalc->qSmall);
    freeMem(gapCalc->tSmall);
    freeMem(gapCalc->bSmall);
    freeMem(gapCalc->longPos);
    freeMem(gapCalc->qLong);
    freeMem(gapCalc->tLong);
    freeMem(gapCalc->bLong);
    freez(pGapCalc);
    }
}

int gapCalcCost(struct gapCalc *gapCalc, int dq, int dt)
/* Figure out gap costs. */
{
if (dt < 0) dt = 0;
if (dq < 0) dq = 0;
if (dt == 0)
    { 
    if (dq < gapCalc->smallSize)
        return gapCalc->qSmall[dq];
    else if (dq >= gapCalc->qLastPos)
        return gapCalc->qLastPosVal + gapCalc->qLastSlope * (dq-gapCalc->qLastPos);
    else
        return interpolate(dq, gapCalc->longPos, gapCalc->qLong, gapCalc->qPosCount);
    }
else if (dq == 0)
    {
    if (dt < gapCalc->smallSize)
        return gapCalc->tSmall[dt];
    else if (dt >= gapCalc->tLastPos)
        return gapCalc->tLastPosVal + gapCalc->tLastSlope * (dt-gapCalc->tLastPos);
    else
        return interpolate(dt, gapCalc->longPos, gapCalc->tLong, gapCalc->tPosCount);
    }
else
    {
    int both = dq + dt;
    if (both < gapCalc->smallSize)
        return gapCalc->bSmall[both];
    else if (both >= gapCalc->bLastPos)
        return gapCalc->bLastPosVal + gapCalc->bLastSlope * (both-gapCalc->bLastPos);
    else
        return interpolate(both, gapCalc->longPos, gapCalc->bLong, gapCalc->bPosCount);
    }
}

void gapCalcTest(struct gapCalc *gapCalc)
/* Print out gap cost info. */
{
int i;
for (i=1; i<=10; i++)
   {
   verbose(1, "%d: %d %d %d\n", i, gapCalcCost(gapCalc, i, 0), 
           gapCalcCost(gapCalc, 0, i), gapCalcCost(gapCalc, i/2, i-i/2));
   }
for (i=1; ; i *= 10)
   {
   verbose(1, "%d: %d %d %d\n", i, gapCalcCost(gapCalc, i, 0), gapCalcCost(gapCalc, 0, i), 
           gapCalcCost(gapCalc, i/2, i-i/2));
   if (i == 1000000000)
       break;
   }
verbose(1, "%d %d cost %d\n", 6489540, 84240, gapCalcCost(gapCalc, 84240, 6489540));
verbose(1, "%d %d cost %d\n", 2746361, 1075188, gapCalcCost(gapCalc, 1075188, 2746361));
verbose(1, "%d %d cost %d\n", 6489540 + 2746361 + 72, 84240 + 1075188 + 72, gapCalcCost(gapCalc, 84240 + 1075188 + 72, 6489540 + 2746361 + 72));
}


