/* hgMedianMicroarray - Create a copy of microarray database that contains the median value of replicas. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"
#include "jksql.h"
#include "hdb.h"
#include "hgRelate.h"
#include "expRecord.h"
#include "expData.h"

static char const rcsid[] = "$Id: hgMedianMicroarray.c,v 1.5 2006/05/05 15:43:14 angie Exp $";

char *tabDir = ".";
boolean doLoad;
int limit = 0;
int minExps = 2;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgMedianMicroarray - Create a copy of microarray database that contains the\n"
  "median value of replicas\n"
  "usage:\n"
  "   hgMedianMicroarray db table expTable specFile.ra newTable newExpTable\n"
  "The specFile.ra contains a line for each expression column you want in\n"
  "the new table.  The format of the line is\n"
  " 'name' 'group' ix1 ix2 ... ixN\n"
  "where 'name' is what will appear in the column label in the Family browser\n"
  "and 'group' is what whill appear in the row label in dense mode in the\n"
  "genome browser, and ix1 ... ixN are the (zero based) indices of columns you\n"
  "are taking the median of in the original data set\n"
  "options:\n"
  "   -tab=dir - Output tab-separated files to directory.\n"
  "   -noLoad  - If true don't load database and don't clean up tab files\n"
  "   -limit=N - Only do limit rows of table, for testing\n"
  "   -minExps=N - Minimum number of experiments to use, default %d\n"
  , minExps
  );
}

static struct optionSpec options[] = {
   {"tab", OPTION_STRING},
   {"noLoad", OPTION_BOOLEAN},
   {"limit", OPTION_INT},
   {"minExps", OPTION_INT},
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


void makeNewExpTable(char *oldTable, struct medSpec *medList, char *newTable)
/* Create new expTable in hgFixed that is very similar
 * to oldExpTable, but with rows defined by medList. */
{
struct medSpec *med;
struct expRecord *oldExp, newExp;
struct sqlConnection *conn = sqlConnect("hgFixed");
FILE *f = hgCreateTabFile(tabDir, newTable);
char query[256], **row;
struct sqlResult *sr;
int curId = 0;

for (med = medList; med != NULL; med = med->next)
    {
    /* Load expression record from old table of first
     * thing in median. */
    safef(query, sizeof(query),
    	"select * from %s where id = %d", oldTable, med->ids[0]);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) == NULL)
        errAbort("Can't find id %d in %s\n", med->ids[0], oldTable);
    oldExp = expRecordLoad(row);
    sqlFreeResult(&sr);
    if (oldExp->numExtras < 3)
        errAbort("Can only deal with old tables with 3 extras or more");


    /* Create new expression record, mostly just a shallow copy of old. */
    newExp = *oldExp;
    newExp.id = curId;
    ++curId;
    newExp.name = newExp.description = med->name;
    newExp.extras[2] = med->group;

    /* Save new one, free old one. */
    expRecordTabOut(&newExp, f);
    expRecordFree(&oldExp);
    }

if (doLoad)
    {
    expRecordCreateTable(conn, newTable);
    hgLoadTabFile(conn, tabDir, newTable, &f);
    hgRemoveTabFile(tabDir, newTable);
    }
sqlDisconnect(&conn);
}

#define missingData -10000.0

void makeNewDataTable(char *database, char *oldTable, struct medSpec *medList, char *newTable)
/* Create new table in database based on medians of data
 * in old table as defined by medList. */
{
struct medSpec *med;
struct sqlConnection *conn = sqlConnect(database);
FILE *f = hgCreateTabFile(tabDir, newTable);
char query[256], **row;
struct sqlResult *sr;
int medCount = slCount(medList);

if (limit != 0)
    {
    safef(query, sizeof(query),
	"select name,expScores from %s limit %d", oldTable, limit);
    }
else
    {
    safef(query, sizeof(query),
	"select name,expScores from %s", oldTable);
    }

sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    int valCount;
    double *vals;
    double median;

    fprintf(f, "%s\t%d\t", row[0], medCount);
    sqlDoubleDynamicArray(row[1], &vals, &valCount);
    for (med = medList; med != NULL; med = med->next)
	{
	int i, realCount = 0;
	double *selVals, val;
	AllocArray(selVals, med->count);
	for (i=0; i<med->count; ++i)
	    {
	    int ix = med->ids[i];
	    if (ix >= valCount)
	        errAbort("%d index in median list, but only %d total", 
			ix, valCount);
	    val = vals[ix];
	    if (val > missingData)
	        {
		selVals[realCount] = vals[ix];
		++realCount;
		}
	    }
	if (realCount < minExps)
	    median = missingData;
	else
	    median = doubleMedian(realCount, selVals);
	fprintf(f, "%0.3f,", median);
	freeMem(selVals);
	}
    fprintf(f, "\n");
    freeMem(vals);
    }

if (doLoad)
    {
    expDataCreateTable(conn, newTable);
    hgLoadTabFile(conn, tabDir, newTable, &f);
    hgRemoveTabFile(tabDir, newTable);
    }
sqlDisconnect(&conn);
}


void hgMedianMicroarray(char *database, char *table, char *expTable, 
	char *specFile, char *newTable, char *newExpTable)
/* hgMedianMicroarray - Create a copy of microarray database that contains 
 * the median value of replicas. */
{
struct medSpec *medList = medSpecReadAll(specFile);
makeNewExpTable(expTable, medList, newExpTable);
makeNewDataTable(database, table, medList, newTable);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
doLoad = !optionExists("noLoad");
minExps = optionInt("minExps", minExps);
if (optionExists("tab"))
    {
    tabDir = optionVal("tab", tabDir);
    makeDir(tabDir);
    }
limit = optionInt("limit", limit);

hgMedianMicroarray(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
