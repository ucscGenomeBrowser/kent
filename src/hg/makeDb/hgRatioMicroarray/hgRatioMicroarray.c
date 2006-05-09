/* hgRatioMicroarray - Create a ratio form of microarray data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "portable.h"
#include "expData.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: hgRatioMicroarray.c,v 1.4 2006/05/09 14:36:05 angie Exp $";

/* Command line overridable options. */
char *database = "hgFixed";
float minAbsVal = 20;
int minMaxVal = 50;
boolean doAverage = FALSE;
char *clump = NULL;
char *tabDir = ".";
boolean doLoad;
int limit = 0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgRatioMicroarray - Create a ratio form of microarray data.\n"
  "usage:\n"
  "   hgRatioMicroarray absoluteTable ratioTable\n"
  "This will create a log-based-two ratio table out of\n"
  "the absolute table.  By default this will take the\n"
  "median value in absoluteTable for as the denumerator for\n"
  "the ratio\n"
  "options:\n"
  "   -database=XX Database name, default %s\n"
  "   -average Do average rather than median\n"
  "   -minAbsVal=N Minimum absolute value considered meaningful\n"
  "                Anything less will be considered to be minAbsVal/2\n"
  "                Default %f\n"
  "   -minMaxVal   Minimum max value before throw away gene data\n"
  "                Must be expressed at least this much in one tissue\n"
  "                Default %d\n"
  "   -clump=clump.ra - Since some tissues may be overrepresented\n"
  "                This lets you clump together related tissues\n"
  "                Only the median within each clump will be used\n"
  "                to create the denominator\n"
  "                The clump.ra file has a line for each clump.\n"
  "                The format is <name><group><ix1>...<ixN>\n"
  "                which is the same format hgMedianMicroarray uses\n"
  "                The 'name' and 'group' columns are ignored in this\n"
  "                program.  The ix1 ... ixN contain the indices of\n"
  "                all mRNA samples to clump together\n"
  "   -tab=dir - Output tab-separated files to directory.\n"
  "   -noLoad  - If true don't load database and don't clean up tab files\n"
  "   -limit=N - Only do limit rows of table, for testing\n"
  , database, minAbsVal, minMaxVal
  );
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"average", OPTION_BOOLEAN},
   {"minAbsVal", OPTION_FLOAT},
   {"minMaxVal", OPTION_INT},
   {"clump", OPTION_STRING},
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"limit", OPTION_INT},
   {NULL, 0},
};

struct medSpec
/* A specification for median. */
    {
    struct medSpec *next;
    char *name;		/* Column name. */
    char *group;	/* Tissue/group name. */
    int count;		/* Count of experiments to median. */
    int *ids;		/* Id's (indexes) of experiments to median. */
    };

struct medSpec *medSpecReadAll(char *fileName)
/* Read in file and parse it into medSpecs. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct medSpec *medList = NULL, *med;
char *row[256], *line;
int i, count;

while (lineFileNextReal(lf, &line))
    {
    char *name = nextQuotedWord(&line);
    char *group = nextQuotedWord(&line);
    if (group == NULL)
	lineFileShort(lf);
    count = chopLine(line, row);
    if (count < 1)
        lineFileShort(lf);
    if (count >= ArraySize(row))
        errAbort("Too many experiments in median line %d of %s",
		lf->lineIx, lf->fileName);
    AllocVar(med);
    med->name = cloneString(name);
    med->group = cloneString(group);
    med->count = count;
    AllocArray(med->ids, count);
    for (i=0; i<count; ++i)
        {
	char *asc = row[i];
	if (!isdigit(asc[0]))
	    errAbort("Expecting number got %s line %d of %s", asc,
	    	lf->lineIx, lf->fileName);
	med->ids[i] = atoi(asc);
	}
    slAddHead(&medList, med);
    }
lineFileClose(&lf);
slReverse(&medList);
return medList;
}

static void clumpMedians(float *vals, int valCount,
	struct medSpec *clumpList, double *clumpedVals)
/* Fill in medians of full in clump list. */
{
struct medSpec *clump;
int clumpIx = 0;
double *selVals, val;
float median;
AllocArray(selVals, valCount);
for (clump = clumpList; clump != NULL; clump = clump->next, ++clumpIx)
    {
    int i, realCount = 0;
    if (clump->count > valCount)
        internalErr();
    for (i=0; i<clump->count; ++i)
	{
	int ix = clump->ids[i];
	if (ix >= valCount)
	    errAbort("%d index in median list, but only %d total", 
		    ix, valCount);
	val = vals[ix];
	selVals[realCount] = vals[ix];
	++realCount;
	}
    if (realCount < 1)
	median = 0;
    else
	median = doubleMedian(realCount, selVals);
    clumpedVals[clumpIx] = median;
    }
freeMem(selVals);
}

static float getDenominator(struct expData *ex, boolean doAve,
	struct medSpec *clumpList)
/* Figure out median or average to use as denominator for ratios. */
{
double *data = NULL;
int i, dataCount;
float q;
int goodCount = 0;
if (clumpList != NULL)
    {
    dataCount = slCount(clumpList);
    AllocArray(data, dataCount);
    clumpMedians(ex->expScores, ex->expCount, clumpList, data);
    }
else
    {
    dataCount = ex->expCount;
    AllocArray(data, dataCount);
    for (i=0; i<dataCount; ++i)
	data[i] = ex->expScores[i];
    }
for (i=0; i<dataCount; ++i)
    if (data[i] >= minMaxVal)
        ++goodCount;
if (goodCount < 1)
    return -1;
if (doAve)
    {
    float sum = 0;
    for (i=0; i<dataCount; ++i)
	 sum += data[i];
    q = sum/dataCount;
    }
else
    {
    q = doubleMedian(dataCount, data);
    }
freeMem(data);
return q;
}

static boolean convertToRatio(struct expData *ex, boolean doAve,
	struct medSpec *clumpList)
/* Convert ex->expScores to ratios.  Returns FALSE if can't. */
{
float q = getDenominator(ex, doAve, clumpList);
float invQ;
int i;

if (q <= 0)
    return FALSE;
invQ = 1.0/q;
for (i=0; i<ex->expCount; ++i)
    {
    ex->expScores[i] = logBase2(ex->expScores[i]*invQ);
    }
return TRUE;
}

static void clipMin(int count, float *scores, float minGood, float minVal)
/* Clip everything below minGood and set it to minVal. */
{
int i;
for (i=0; i<count; ++i)
    {
    if (scores[i] < minGood)
        scores[i] = minVal;
    }
}

void hgRatioMicroarray(char *absTable, char *relTable)
/* hgRatioMicroarray - Create a ratio form of microarray. */
{
struct medSpec *clumpList = NULL;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
char query[512];
struct expData *ex;
FILE *f = hgCreateTabFile(tabDir, relTable);
int rowCount = 0;

if (clump != NULL)
    clumpList = medSpecReadAll(clump);
safef(query, sizeof(query),
	"select * from %s", absTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ex = expDataLoad(row);
    clipMin(ex->expCount, ex->expScores, minAbsVal, minAbsVal*0.5);
    if (convertToRatio(ex, doAverage, clumpList))
	expDataTabOut(ex, f);
    expDataFree(&ex);
    ++rowCount;
    if (limit != 0 && rowCount >= limit)
        break;
    }
sqlFreeResult(&sr);

if (doLoad)
    {
    expDataCreateTable(conn, relTable);
    hgLoadTabFile(conn, tabDir, relTable, &f);
    hgRemoveTabFile(tabDir, relTable);
    }

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
database = optionVal("database", database);
doAverage = optionExists("average");
minAbsVal = optionFloat("minAbsVal", minAbsVal);
minMaxVal = optionInt("minMaxVal", minMaxVal);
clump = optionVal("clump", clump);
doLoad = !optionExists("noLoad");
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
limit = optionInt("limit", limit);
hgRatioMicroarray(argv[1], argv[2]);
return 0;
}
