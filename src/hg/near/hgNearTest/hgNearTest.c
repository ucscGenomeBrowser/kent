/* hgNearTest - Test hgNear web page. */
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
#include "../hgNear/hgNear.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgNearTest.c,v 1.7 2004/03/04 02:21:13 kent Exp $";

/* Command line variables. */
char *dataDir = "/usr/local/apache/cgi-bin/hgNearData";
char *clOrg = NULL;	/* Organism from command line. */
char *clDb = NULL;	/* DB from command line */
char *clSearch = NULL;	/* Search var from command line. */
int clRepeat = 3;	/* Number of repetitions. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNearTest - Test hgNear web page\n"
  "usage:\n"
  "   hgNearTest url log\n"
  "options:\n"
  "   -org=Human - Restrict to Human (or Mouse, Fruitfly, etc.)\n"
  "   -db=hg16 - Restrict to particular database\n"
  "   -search=gene - Use selected gene\n"
  "   -dataDir=dataDir - Use selected data dir, default %s\n"
  "   -repeat=N - Number of times to repeat test (on random genes)\n"
  , dataDir, clRepeat);
}


static struct optionSpec options[] = {
   {"org", OPTION_STRING},
   {"db", OPTION_STRING},
   {"search", OPTION_STRING},
   {"dataDir", OPTION_STRING},
   {"repeat", OPTION_INT},
   {NULL, 0},
};

char *hgTestScanForErrorMessage(char *text)
/* Scan text for error message.  If one exists then
 * return copy of it.  Else return NULL. */
{
char *startPat = htmlWarnStartPattern();
char *startErr = stringIn(startPat, text);
if (startErr != NULL)
    {
    char *endErr;
    int errSize;
    startErr += strlen(startPat);
    endErr = stringIn(startErr, htmlWarnEndPattern());
    if (endErr == NULL)
        {
	errSize = strlen(startErr);
	if (errSize > 100)
	    errSize = 100;
	}
    else
        errSize = endErr - startErr;
    return cloneStringZ(startErr, errSize);
    }
return NULL;
}

struct qaStatus
/* Timing and other info about fetching a web page. */
    {
    struct qaStatus *next;
    int milliTime;	/* Time page fetch took. */
    char *errMessage;	/* Error message if any. */
    boolean hardError;	/* Crash of some sort. */
    };

void qaStatusReportOne(FILE *f, struct qaStatus *qs, char *format, ...)
/* Report status */
{
char *severity = "ok";
va_list args;
va_start(args, format);
char *errMessage = qs->errMessage;
if (errMessage == NULL)
    errMessage = "";
else
    {
    if (qs->hardError)
        severity = "hard";
    else
        severity = "soft";
    }
  
vfprintf(f, format, args);
fprintf(f, " %4.3fs (%s) %s\n", 0.001*qs->milliTime, severity, errMessage);
va_end(args);
}

struct qaStatus *qaStatusOnPage(struct errCatch *errCatch, 
	struct htmlPage *page, long startTime, struct htmlPage **retPage)
/* Fill in qs status and errMessage based on errCatch and page. */
{
char *errMessage = NULL;
struct qaStatus *qs;
AllocVar(qs);
if (errCatch->gotError || page == NULL)
    {
    errMessage = errCatch->message->string;
    qs->hardError = TRUE;
    }
else
    {
    if (page->status->status != 200)
	{
	dyStringPrintf(errCatch->message, "HTTP status code %d\n", 
		page->status->status);
	errMessage = errCatch->message->string;
	qs->hardError = TRUE;
	}
    else
        {
	errMessage = hgTestScanForErrorMessage(page->fullText);
	}
    }
qs->errMessage = cloneString(errMessage);
if (qs->errMessage != NULL)
    subChar(qs->errMessage, '\n', ' ');
qs->milliTime = clock1000() - startTime;
if (retPage != NULL)
    *retPage = page;
else
    htmlPageFree(&page);
return qs;
}

struct qaStatus *qaPageGet(char *url, struct htmlPage **retPage)
/* Get info on given url, (and return page if retPage non-null). */
{
struct errCatch *errCatch = errCatchNew();
struct qaStatus *qs;
struct htmlPage *page = NULL;
long startTime = clock1000();
if (errCatchStart(errCatch))
    {
    page = htmlPageGet(url);
    htmlPageValidateOrAbort(page);
    }
else
    {
    htmlPageFree(&page);
    }
errCatchEnd(errCatch);
qs = qaStatusOnPage(errCatch, page, startTime, retPage);
errCatchFree(&errCatch);
return qs;
}

struct qaStatus *qaPageFromForm(struct htmlPage *origPage, struct htmlForm *form, 
	char *buttonName, char *buttonVal, struct htmlPage **retPage)
/* Get update to form. */
{
struct errCatch *errCatch = errCatchNew();
struct qaStatus *qs;
struct htmlPage *page = NULL;
long startTime = clock1000();
if (errCatchStart(errCatch))
    {
    page = htmlPageFromForm(origPage, form, buttonName, buttonVal);
    htmlPageValidateOrAbort(page);
    }
else
    {
    htmlPageFree(&page);
    }
errCatchEnd(errCatch);
qs = qaStatusOnPage(errCatch, page, startTime, retPage);
errCatchFree(&errCatch);
return qs;
}

struct qaStatistics
/* Stats on one set of tests. */
    {
    struct qaStatistics *next;
    int testCount;	/* Number of tests. */
    int softCount;	/* Soft error count. */
    int hardCount;	/* Hard error count. */
    long milliTotal;	/* Time tests took. */
    };

void qaStatisticsAdd(struct qaStatistics *stats, struct qaStatus *qs)
/* Add test results to totals */
{
stats->testCount += 1;
stats->milliTotal += qs->milliTime;
if (qs->errMessage)
    {
    if (qs->hardError)
        stats->hardCount += 1;
    else
        stats->softCount += 1;
    }
}

void qaStatisticsReport(struct qaStatistics *stats, char *label, FILE *f)
/* Write a line of stats to file. */
{
fprintf(f, "%20s:  %3d tests, %2d soft errors, %2d hard errors, %5.2f seconds\n",
	label, stats->testCount, stats->softCount, stats->hardCount, 
	0.001 * stats->milliTotal);
}


struct slName *randomSample(char *db, char *table, char *field, int count)
/* Get random sample from database. */
{
struct sqlConnection *conn = sqlConnect(db);
char query[256], **row;
struct sqlResult *sr;
struct slName *list = NULL, *el;
safef(query, sizeof(query), "select %s from %s order by rand() limit %d", 
	field, table, count);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    el = slNameNew(row[0]);
    slAddHead(&list, el);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return list;
}

struct nearTest
/* Test on one column. */
    {
    struct nearTest *next;
    struct qaStatus *status;	/* Result of test. */
    char *info[6];
    };

enum nearTestInfoIx {
   ntiiType = 0,
   ntiiSort = 1,
   ntiiOrg = 2,
   ntiiDb = 3,
   ntiiCol = 4,
   ntiiGene = 5,
};

char *nearTestInfoTypes[] =
   { "type", "sort", "organism", "db", "column", "gene" };

struct nearTest *nearTestNew(struct qaStatus *status,
	char *type, char *sort, char *org, char *db, char *col, char *gene)
/* Save away column test results. */
{
struct nearTest *test;
AllocVar(test);
test->status = status;
test->info[ntiiType] = type;
test->info[ntiiSort] = sort;
test->info[ntiiSort] = sort;
test->info[ntiiOrg] = org;
test->info[ntiiDb] = db;
test->info[ntiiCol] = col;
test->info[ntiiGene] = gene;
return test;
}

void nearTestLogOne(struct nearTest *test, FILE *f)
/* Log test result to file. */
{
int i;
for (i=0; i<ArraySize(test->info); ++i)
    fprintf(f, "%s ", test->info[i]);
fprintf(f, "%s\n", test->status->errMessage);
}

void qaStatusSoftError(struct qaStatus *qs, char *format, ...)
/* Add error message for something less than a crash. */
{
struct dyString *dy = dyStringNew(0);
va_list args;
va_start(args, format);
if (qs->errMessage)
    {
    dyStringAppend(dy, qs->errMessage);
    dyStringAppendC(dy, '\n');
    }
dyStringVaPrintf(dy, format, args);
va_end(args);
freez(&qs->errMessage);
qs->errMessage = cloneString(dy->string);
dyStringFree(&dy);
}

int qaCountBetween(char *s, char *startPattern, char *endPattern, 
	char *midPattern)
/* Count the number of midPatterns that occur between start and end pattern. */
{
int count = 0;
char *e;
s = stringIn(startPattern, s);
if (s != NULL)
    {
    s += strlen(startPattern);
    e = stringIn(endPattern, s);
    while (s < e)
        {
	if (startsWith(midPattern, s))
	    ++count;
	s += 1;
	}
    }
return count;
}

void testCol(struct htmlPage *emptyConfig, char *org, char *db, char *col, char *gene,
	struct nearTest **pTestList)
/* Test one column. */
{
struct htmlPage *printPage = NULL;
struct nearTest *test;
struct qaStatus *qs;
char visVar[256];
safef(visVar, sizeof(visVar), "near.col.%s.vis", col);
htmlPageSetVar(emptyConfig, NULL, visVar, "on");
htmlPageSetVar(emptyConfig, NULL, "near.order", "geneDistance");
htmlPageSetVar(emptyConfig, NULL, "near.count", "25");

qs = qaPageFromForm(emptyConfig, emptyConfig->forms, 
	"submit", "Submit", &printPage);
test = nearTestNew(qs, "colPrint", "n/a", org, db, col, gene);
slAddHead(pTestList, test);
if (printPage != NULL)
    {
    int expectCount = 25;
    int lineCount = qaCountBetween(printPage->fullText, 
	    "<!-- Start Rows -->", "<!-- End Rows -->", "<!-- Row -->");
    if (lineCount != expectCount)
	qaStatusSoftError(qs, "Got %d rows, expected %d", lineCount, expectCount);
    }
if (test->status->errMessage != NULL)
    nearTestLogOne(test, stderr);
htmlPageFree(&printPage);
htmlPageSetVar(emptyConfig, NULL, visVar, NULL);
}

struct htmlPage *emptyConfigPage(struct htmlPage *dbPage, char *org,
	char *db, struct nearTest **pTestList)
/* Get empty configuration page. */
{
struct nearTest *test;
struct qaStatus *qs;
struct htmlPage *emptyConfig;
qs = qaPageFromForm(dbPage, dbPage->forms, "near.do.colHideAll", "on", &emptyConfig);
test = nearTestNew(qs, "emptyConfig", "n/a", org, db, "n/a", "n/a");
slAddHead(pTestList, test);
if (emptyConfig == NULL)
    warn("Couldn't get empty config page for %s\n", db);
return emptyConfig;
}

void testDbColumns(struct htmlPage *dbPage, char *org, char *db, 
	struct slName *geneList, struct nearTest **pTestList)
/* Test on one database. */
{
struct htmlPage *emptyConfig;
struct slName *colList = NULL, *col;
struct htmlFormVar *var;
struct slName *gene;

uglyf("testDbColumns %s %s\n", org, db);
emptyConfig = emptyConfigPage(dbPage, org, db, pTestList);
if (emptyConfig != NULL )
    {
    for (var = emptyConfig->forms->vars; var != NULL; var = var->next)
	{
	if (startsWith("near.col.", var->name) && endsWith(var->name, ".vis"))
	    {
	    char *colNameStart = var->name + strlen("near.col.");
	    char *colNameEnd = strchr(colNameStart, '.');
	    *colNameEnd = 0;
	    col = slNameNew(colNameStart);
	    slAddHead(&colList, col);
	    *colNameEnd = '.';
	    }
	}
    slReverse(&colList);

    for (gene = geneList; gene != NULL; gene = gene->next)
	{
	htmlPageSetVar(emptyConfig, NULL, "near_search", gene->name);
	for (col = colList; col != NULL; col = col->next)
	    {
	    testCol(emptyConfig, org, db, col->name, gene->name, pTestList);
	    }
	}
    }
htmlPageFree(&emptyConfig);
}

void testSort(struct htmlPage *emptyConfig, char *org, char *db, char *sort, char *gene,
	struct nearTest **pTestList)
/* Test one column. */
{
struct htmlPage *printPage = NULL;
struct nearTest *test;
struct qaStatus *qs;
htmlPageSetVar(emptyConfig, NULL, "near.col.acc.vis", "on");
htmlPageSetVar(emptyConfig, NULL, "near.order", sort);
htmlPageSetVar(emptyConfig, NULL, "near.count", "25");

qs = qaPageFromForm(emptyConfig, emptyConfig->forms, 
	"submit", "Submit", &printPage);
test = nearTestNew(qs, "sortType", cloneString(sort), org, db, "n/a", gene);
slAddHead(pTestList, test);
if (printPage != NULL)
    {
    int lineCount = qaCountBetween(printPage->fullText, 
	    "<!-- Start Rows -->", "<!-- End Rows -->", "<!-- Row -->");
    if (lineCount < 1)
	qaStatusSoftError(qs, "No rows for sort %s", sort);
    }
if (test->status->errMessage != NULL)
    nearTestLogOne(test, stderr);
htmlPageFree(&printPage);
}


void testDbSorts(struct htmlPage *dbPage, char *org, char *db, 
	struct slName *geneList, struct nearTest **pTestList)
/* Test on one database. */
{
struct htmlPage *emptyConfig;
struct slName *colList = NULL, *col;
struct htmlFormVar *sortVar = htmlFormVarGet(dbPage->forms, orderVarName);
struct slName *gene, *sort;

uglyf("testDbSorts %s %s\n", org, db);
if (sortVar == NULL)
    errAbort("Couldn't find var %s", orderVarName);

emptyConfig = emptyConfigPage(dbPage, org, db, pTestList);
if (emptyConfig != NULL)
    {
    for (sort = sortVar->values; sort != NULL; sort= sort->next)
	{
	for (gene = geneList; gene != NULL; gene = gene->next)
	    {
	    testSort(emptyConfig, org, db, sort->name, gene->name, pTestList);
	    }
	}
    htmlPageFree(&emptyConfig);
    }
}

void testDb(struct htmlPage *orgPage, char *org, char *db, struct nearTest **pTestList)
/* Test on one database. */
{
struct hash *genomeRa = hgReadRa(org, db, dataDir, "genome.ra", NULL);
char *canonicalTable = hashMustFindVal(genomeRa, "canonicalTable");
struct slName *geneList = randomSample(db, canonicalTable, "transcript", clRepeat);
struct nearTest *test;
struct qaStatus *qs;
struct htmlPage *dbPage;

htmlPageSetVar(orgPage, NULL, "db", db);
htmlPageSetVar(orgPage, NULL, "near_search", "");
qs = qaPageFromForm(orgPage, orgPage->forms, "Submit", "Go!", &dbPage);
test = nearTestNew(qs, "dbEmptyPage", "n/a", org, db, "n/a", "n/a");
slAddHead(pTestList, test);

testDbColumns(dbPage, org, db, geneList, pTestList);
testDbSorts(dbPage, org, db, geneList, pTestList);

slFreeList(&geneList);
hashFree(&genomeRa);
}


void testOrg(struct htmlPage *rootPage, struct htmlForm *rootForm, char *org, char *forceDb,
	struct nearTest **pTestList)
/* Test on organism.  If forceDb is non-null, only test on
 * given database. */
{
struct htmlPage *orgPage;
struct htmlForm *mainForm;
struct htmlFormVar *dbVar;
struct slName *db;

htmlPageSetVar(rootPage, rootForm, "org", org);
htmlPageSetVar(rootPage, rootForm, "db", NULL);
htmlPageSetVar(rootPage, rootForm, "near_search", "");
orgPage = htmlPageFromForm(rootPage, rootPage->forms, "submit", "Go");
if ((mainForm = htmlFormGet(orgPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on orgPage");
if ((dbVar = htmlFormVarGet(mainForm, "db")) == NULL)
    errAbort("Couldn't get org var");
for (db = dbVar->values; db != NULL; db = db->next)
    {
    if (forceDb == NULL || sameString(forceDb, db->name))
	testDb(orgPage, org, cloneString(db->name), pTestList);
    }
htmlPageFree(&orgPage);
}

void statsOnSubsets(struct nearTest *list, int subIx, FILE *f)
/* Report tests of certain subtype. */
{
struct nearTest *test;
struct hash *hash = newHash(0);
struct slName *typeList = NULL, *type;

fprintf(f, "\n%s subtotals\n", nearTestInfoTypes[subIx]);

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


void reportSummary(struct nearTest *list, FILE *f)
/* Report summary of test results. */
{
struct qaStatistics *stats;
struct nearTest *test;
AllocVar(stats);
int i;

for (i=0; i<=ntiiCol; ++i)
    statsOnSubsets(list, i, f);
for (test = list; test != NULL; test = test->next)
    qaStatisticsAdd(stats, test->status);
qaStatisticsReport(stats, "Total", f);
}


void reportAll(struct nearTest *list, FILE *f)
/* Report all tests. */
{
struct nearTest *test;
for (test = list; test != NULL; test = test->next)
    {
    if (test->status->errMessage != NULL)
	nearTestLogOne(test, f);
    }
}

void hgNearTest(char *url, char *log)
/* hgNearTest - Test hgNear web page. */
{
struct htmlPage *rootPage = htmlPageGet(url);
struct htmlForm *mainForm;
struct htmlFormVar *orgVar;
struct nearTest *testList = NULL;
FILE *f = mustOpen(log, "w");
htmlPageValidateOrAbort(rootPage);
if ((mainForm = htmlFormGet(rootPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form");
if ((orgVar = htmlFormVarGet(mainForm, "org")) == NULL)
    errAbort("Couldn't get org var");
if (clOrg != NULL)
    testOrg(rootPage, mainForm, clOrg, clDb, &testList);
else
    {
    struct slName *org;
    for (org = orgVar->values; org != NULL; org = org->next)
        {
	testOrg(rootPage, mainForm, cloneString(org->name), clDb, &testList);
	}
    }
htmlPageFree(&rootPage);
reportSummary(testList, stdout);
reportAll(testList, f);
fprintf(f, "---------------------------------------------\n");
reportSummary(testList, f);
}

#ifdef TEST 
void hgNearTest(char *url)
{
struct htmlPage *page;
struct qaStatus *qs;
qs = qaPageGet(url, &page);
qaStatusReportOne(stdout, qs, url);
htmlPageFree(&page);
}
#endif /* TEST */

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(200000000);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clDb = optionVal("db", clDb);
clOrg = optionVal("org", clOrg);
clSearch = optionVal("search", clSearch);
dataDir = optionVal("dataDir", dataDir);
clRepeat = optionInt("repeat", clRepeat);
hgNearTest(argv[1], argv[2]);
carefulCheckHeap();
return 0;
}
