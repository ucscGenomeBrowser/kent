/* hgTablesTest - Test hgTables web page. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "portable.h"
#include "options.h"
#include "errCatch.h"
#include "ra.h"
#include "htmlPage.h"
#include "../hgTables/hgTables.h"
#include "hdb.h"
#include "qa.h"

static char const rcsid[] = "$Id: hgTablesTest.c,v 1.1 2004/11/07 02:39:08 kent Exp $";

/* Command line variables. */
char *clOrg = NULL;	/* Organism from command line. */
char *clDb = NULL;	/* DB from command line */
int clRepeat = 3;	/* Number of repetitions. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTablesTest - Test hgTables web page\n"
  "usage:\n"
  "   hgTablesTest url log\n"
  "options:\n"
  "   -org=Human - Restrict to Human (or Mouse, Fruitfly, etc.)\n"
  "   -db=hg16 - Restrict to particular database\n"
  "   -repeat=N - Number of times to repeat test (on random genes)\n"
  , clRepeat);
}


static struct optionSpec options[] = {
   {"org", OPTION_STRING},
   {"db", OPTION_STRING},
   {"search", OPTION_STRING},
   {"repeat", OPTION_INT},
   {NULL, 0},
};

struct tablesTest
/* Test on one column. */
    {
    struct tablesTest *next;
    struct qaStatus *status;	/* Result of test. */
    char *info[6];
    };

enum tablesTestInfoIx {
   ntiiType,
   ntii_testType1,
   ntiiOrg,
   ntiiDb,
   ntiiGroup,
   ntiiTotalCount,
};

char *tablesTestInfoTypes[] =
   { "type", "testType1", "organism", "db", "group", };

struct tablesTest *tablesTestList = NULL;	/* List of all tests, latest on top. */

struct tablesTest *tablesTestNew(struct qaStatus *status,
	char *type, char *testType1, char *org, char *db, char *group)
/* Save away column test results. */
{
struct tablesTest *test;
AllocVar(test);
test->status = status;
test->info[ntiiType] = cloneString(naForNull(type));
test->info[ntii_testType1] = cloneString(naForNull(testType1));
test->info[ntiiOrg] = cloneString(naForNull(org));
test->info[ntiiDb] = cloneString(naForNull(db));
test->info[ntiiGroup] = cloneString(naForNull(group));
slAddHead(&tablesTestList, test);
return test;
}

void tablesTestLogOne(struct tablesTest *test, FILE *f)
/* Log test result to file. */
{
int i;
for (i=0; i<ArraySize(test->info); ++i)
    fprintf(f, "%s ", test->info[i]);
fprintf(f, "%s\n", test->status->errMessage);
}

struct htmlPage *quickSubmit(struct htmlPage *basePage,
	char *testType1, char *org, char *db, char *group,
	char *testName, char *button, char *buttonVal)
/* Submit page and record info.  Return NULL if a problem. */
{
struct tablesTest *test;
struct qaStatus *qs;
struct htmlPage *page;
if (basePage != NULL)
    {
    if (db != NULL)
	htmlPageSetVar(basePage, NULL, "db", db);
    if (org != NULL)
	htmlPageSetVar(basePage, NULL, "org", org);
    qs = qaPageFromForm(basePage, basePage->forms, 
	    button, buttonVal, &page);
    test = tablesTestNew(qs, testName, testType1, org, db, group);
    }
return page;
}

void serialSubmit(struct htmlPage **pPage,
	char *testType1, char *org, char *db, char *group,
	char *testName, char *button, char *buttonVal)
/* Submit page, replacing old page with new one. */
{
struct htmlPage *oldPage = *pPage;
if (oldPage != NULL)
    {
    *pPage = quickSubmit(oldPage, testType1, org, db, group,
    	testName, button, buttonVal);
    htmlPageFree(&oldPage);
    }
}

void quickErrReport()
/* Report error at head of list if any */
{
struct tablesTest *test = tablesTestList;
if (test->status->errMessage != NULL)
    tablesTestLogOne(test, stderr);
}

void testDb(struct htmlPage *orgPage, char *org, char *db)
/* Test on one database. */
{
struct htmlPage *dbPage;
htmlPageSetVar(orgPage, NULL, "db", db);
dbPage = quickSubmit(orgPage, NULL, org, db, NULL, "dbEmptyPage", "submit", "go");
quickErrReport();
if (dbPage != NULL)
    {
    /* Now we want to actually do some tests. */
    uglyf("Testing %s %s. More tests coming soon.\n", org, db);
    }
}


void testOrg(struct htmlPage *rootPage, struct htmlForm *rootForm, char *org, char *forceDb)
/* Test on organism.  If forceDb is non-null, only test on
 * given database. */
{
struct htmlPage *orgPage;
struct htmlForm *mainForm;
struct htmlFormVar *dbVar;
struct slName *db;

uglyf("testOrg %s %s\n", org, forceDb);
htmlPageSetVar(rootPage, rootForm, "org", org);
if (forceDb)
    htmlPageSetVar(rootPage, rootForm, "db", forceDb);
else
   htmlPageSetVar(rootPage, rootForm, "db", "0");
orgPage = htmlPageFromForm(rootPage, rootPage->forms, "submit", "Go");
if ((mainForm = htmlFormGet(orgPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on orgPage");
if ((dbVar = htmlFormVarGet(mainForm, "db")) == NULL)
    errAbort("Couldn't get org var");
for (db = dbVar->values; db != NULL; db = db->next)
    {
    if (forceDb == NULL || sameString(forceDb, db->name))
	testDb(orgPage, org, db->name);
    }
htmlPageFree(&orgPage);
}

void statsOnSubsets(struct tablesTest *list, int subIx, FILE *f)
/* Report tests of certain subtype. */
{
struct tablesTest *test;
struct hash *hash = newHash(0);
struct slName *typeList = NULL, *type;

fprintf(f, "\n%s subtotals\n", tablesTestInfoTypes[subIx]);

/* Get list of all types in this field. */
for (test = list; test != NULL; test = test->next)
    {
    char *info = test->info[subIx];
    if (!hashLookup(hash, info))
       {
       type = slNameNew(info);
       hashAdd(hash, info, type);
       slAddHead(&typeList, type);
       }
    }
slNameSort(&typeList);
hashFree(&hash);

for (type = typeList; type != NULL; type = type->next)
    {
    struct qaStatistics *stats;
    AllocVar(stats);
    for (test = list; test != NULL; test = test->next)
        {
	if (sameString(type->name, test->info[subIx]))
	    {
	    qaStatisticsAdd(stats, test->status);
	    }
	}
    qaStatisticsReport(stats, type->name, f);
    freez(&stats);
    }
}


void reportSummary(struct tablesTest *list, FILE *f)
/* Report summary of test results. */
{
struct qaStatistics *stats;
struct tablesTest *test;
AllocVar(stats);
int i;

for (i=0; i<ntiiTotalCount; ++i)
    statsOnSubsets(list, i, f);
for (test = list; test != NULL; test = test->next)
    qaStatisticsAdd(stats, test->status);
qaStatisticsReport(stats, "Total", f);
}


void reportAll(struct tablesTest *list, FILE *f)
/* Report all tests. */
{
struct tablesTest *test;
for (test = list; test != NULL; test = test->next)
    {
    if (test->status->errMessage != NULL)
	tablesTestLogOne(test, f);
    }
}

void hgTablesTest(char *url, char *log)
/* hgTablesTest - Test hgTables web page. */
{
struct htmlPage *rootPage = htmlPageGet(url);
FILE *f = mustOpen(log, "w");
htmlPageValidateOrAbort(rootPage);
if (clDb != NULL)
    {
    testDb(rootPage, NULL, clDb);
    }
else
    {
    struct htmlForm *mainForm;
    struct htmlFormVar *orgVar;
    if ((mainForm = htmlFormGet(rootPage, "mainForm")) == NULL)
	errAbort("Couldn't get main form");
    if ((orgVar = htmlFormVarGet(mainForm, "org")) == NULL)
	errAbort("Couldn't get org var");
    if (clOrg != NULL)
	testOrg(rootPage, mainForm, clOrg, clDb);
    else
	{
	struct slName *org;
	for (org = orgVar->values; org != NULL; org = org->next)
	    {
	    testOrg(rootPage, mainForm, org->name, clDb);
	    }
	}
    }
htmlPageFree(&rootPage);
slReverse(&tablesTestList);
reportSummary(tablesTestList, stdout);
reportAll(tablesTestList, f);
fprintf(f, "---------------------------------------------\n");
reportSummary(tablesTestList, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(200000000);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clDb = optionVal("db", clDb);
clOrg = optionVal("org", clOrg);
clRepeat = optionInt("repeat", clRepeat);
hgTablesTest(argv[1], argv[2]);
carefulCheckHeap();
return 0;
}
