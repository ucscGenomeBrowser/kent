/* hgRatioMicroarray - Create a ratio form of microarray data. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "portable.h"
#include "expData.h"
#include "hgRelate.h"
#include "microarray.h"


/* Command line overridable options. */
char *database = "hgFixed";
float minAbsVal = 20;
int minMaxVal = 50;
boolean doAverage = FALSE;
boolean transpose = FALSE;
char *clump = NULL;
char *tabDir = ".";
boolean doLoad;
int limit = 0;
double c = 0;

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
  "   -transpose Median/average over arrays instead of genes\n"
  "   -minAbsVal=N Minimum absolute value considered meaningful\n"
  "                Anything less will be considered to be minAbsVal/2\n"
  "                Default %f\n"
  "   -minMaxVal   Minimum max value before throw away gene data\n"
  "                Must be expressed at least this much in one tissue\n"
  "                Default %d\n"
  "   -addConst=c  Add a constant c to data prior to log-ratio.  This may\n"
  "                result in less noise in the ratio data.\n"
  "   -clump=clump.ra - Since some tissues may be overrepresented\n"
  "                This lets you clump together related tissues\n"
  "                Only the median within each clump will be used\n"
  "                to create the denominator\n"
  "                The clump.ra file has a line for each clump.\n"
  "                The format is <name><group><ix1>...<ixN>\n"
  "                which is the same format hgMedianMicroarray uses\n"
  "                The 'name' and 'group' columns are ignored in this\n"
  "                program.  The ix1 ... ixN contain the indices of\n"
  "                all mRNA samples to clump together. This option\n"
  "                is incompatible with -transpose"
  "   -tab=dir - Output tab-separated files to directory.\n"
  "   -noLoad  - If true don't load database and don't clean up tab files\n"
  "   -limit=N - Only do limit rows of table, for testing\n"
  , database, minAbsVal, minMaxVal
  );
}

static struct optionSpec options[] = {
   {"database", OPTION_STRING},
   {"average", OPTION_BOOLEAN},
   {"transpose", OPTION_BOOLEAN},
   {"minAbsVal", OPTION_FLOAT},
   {"minMaxVal", OPTION_INT},
   {"clump", OPTION_STRING},
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"limit", OPTION_INT},
   {"addConst", OPTION_DOUBLE},
   {NULL, 0},
};


void hgRatioMicroarray(char *absTable, char *relTable)
/* hgRatioMicroarray - Create a ratio form of microarray. */
{
struct maMedSpec *clumpList = NULL;
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
char query[512];
struct expData *ex;
struct expData *expList = NULL;
FILE *f = hgCreateTabFile(tabDir, relTable);
int rowCount = 0;

if (clump != NULL)
    clumpList = maMedSpecReadAll(clump);

sqlSafef(query, sizeof(query),
	"select * from %s", absTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    ex = expDataLoad(row);
    slAddHead(&expList, ex);
    if (limit != 0 && rowCount >= limit)
        break;
    }
sqlFreeResult(&sr);
slReverse(&expList);
maExpDataClipMin(expList, minAbsVal, minAbsVal * 0.5);
maExpDataAddConstant(expList, c);
if (transpose)
    maExpDataDoLogRatioTranspose(expList, doAverage);
else
    maExpDataDoLogRatioGivenMedSpec(expList, clumpList, (doAverage) ? useMean : useMedian);
for (ex = expList; ex != NULL; ex = ex->next)
    expDataTabOut(ex, f);
if (doLoad)
    {
    expDataCreateTable(conn, relTable);
    hgLoadTabFile(conn, tabDir, relTable, &f);
    hgRemoveTabFile(tabDir, relTable);
    }
expDataFreeList(&expList);
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
transpose = optionExists("transpose");
minAbsVal = optionFloat("minAbsVal", minAbsVal);
minMaxVal = optionInt("minMaxVal", minMaxVal);
c = optionDouble("addConst", c);
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
