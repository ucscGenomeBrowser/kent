/* hgNearTest - Test hgNear web page. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
#include "qa.h"


/* Command line variables. */
char *dataDir = "/usr/local/apache/cgi-bin/hgNearData";
char *clOrg = NULL;	/* Organism from command line. */
char *clDb = NULL;	/* DB from command line */
char *clSearch = NULL;	/* Search var from command line. */
int clRepeat = 3;	/* Number of repetitions. */
int seed = 0;           /* seed for random number generator */

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
  "               (default %d)\n"
  "   -seed flag to specify seed for random number generator as debugging aid.\n"
  , dataDir, clRepeat);
}


static struct optionSpec options[] = {
    {"org", OPTION_STRING},
    {"db", OPTION_STRING},
    {"search", OPTION_STRING},
    {"dataDir", OPTION_STRING},
    {"repeat", OPTION_INT},
    {"seed", OPTION_INT},
    {NULL, 0},
};

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

struct nearTest *nearTestList = NULL;	/* List of all tests, latest on top. */

struct nearTest *nearTestNew(struct qaStatus *status,
	char *type, char *sort, char *org, char *db, char *col, char *gene)
/* Save away column test results. */
{
struct nearTest *test;
AllocVar(test);
test->status = status;
test->info[ntiiType] = cloneString(naForNull(type));
test->info[ntiiSort] = cloneString(naForNull(sort));
test->info[ntiiOrg] = cloneString(naForNull(org));
test->info[ntiiDb] = cloneString(naForNull(db));
test->info[ntiiCol] = cloneString(naForNull(col));
test->info[ntiiGene] = cloneString(naForNull(gene));
slAddHead(&nearTestList, test);
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

char *nearStartTablePat = "<!-- Start Rows -->";
char *nearEndTablePat = "<!-- End Rows -->";
char *nearEndRowPat = "<!-- Row -->";

int nearCountRows(struct htmlPage *page)
/* Count number of rows in big table. */
{
return qaCountBetween(page->htmlText, nearStartTablePat,
	nearEndTablePat, nearEndRowPat);
}

int nearCountUniqAccRows(struct htmlPage *page)
/* Count number of unique rows in table containing just hyperlinked 
 * accessions. */
{
char *s, *e, *row, *acc;
int count = 0;
struct hash *uniqHash = hashNew(0);

if (page == NULL)
    return -1;

/* Set s to first row. */
s = stringIn(nearStartTablePat, page->htmlText);
if (s == NULL)
    return -1;
s += strlen(nearStartTablePat);

for (;;)
    {
    e = stringIn(nearEndRowPat, s);
    if (e == NULL)
        break;
    row = cloneStringZ(s, e-s);
    acc = qaStringBetween(row, ">", "</a>");
    if (acc == NULL)
        {
	warn("Can't find acc text between > and </a> while counting uniq row %s",
		row);
	freez(&row);
	break;
	}
    if (!hashLookup(uniqHash, acc))
        {
	hashAdd(uniqHash, acc, NULL);
	++count;
	}
    freez(&row);
    freez(&acc);
    s = e + strlen(nearEndRowPat);
    }
hashFree(&uniqHash);
return count;
}

struct htmlPage *quickSubmit(struct htmlPage *basePage,
	char *sort, char *org, char *db, char *col, char *gene, 
	char *testName, char *button, char *buttonVal)
/* Submit page and record info.  Return NULL if a problem. */
{
struct htmlPage *page = NULL;
if (basePage != NULL)
    {
    struct qaStatus *qs;
    if (db != NULL)
	htmlPageSetVar(basePage, NULL, "db", db);
    if (org != NULL)
	htmlPageSetVar(basePage, NULL, "org", org);
    qs = qaPageFromForm(basePage, basePage->forms, 
	    button, buttonVal, &page);
    // ignore return value, answer is accumulating in global: nearTestList
    (void) nearTestNew(qs, testName, sort, org, db, col, gene);
    }
return page;
}

void serialSubmit(struct htmlPage **pPage,
	char *sort, char *org, char *db, char *col, char *gene, 
	char *testName, char *button, char *buttonVal)
/* Submit page, replacing old page with new one. */
{
struct htmlPage *oldPage = *pPage;
if (oldPage != NULL)
    {
    *pPage = quickSubmit(oldPage, sort, org, db, col, gene, 
    	testName, button, buttonVal);
    htmlPageFree(&oldPage);
    }
}

void quickErrReport()
/* Report error at head of list if any */
{
struct nearTest *test = nearTestList;
if (test->status->errMessage != NULL)
    nearTestLogOne(test, stderr);
}

void testCol(struct htmlPage *emptyConfig, char *org, char *db, char *col, char *gene)
/* Test one column. */
{
struct htmlPage *printPage = NULL;
char visVar[256];
safef(visVar, sizeof(visVar), "near.col.%s.vis", col);
htmlPageSetVar(emptyConfig, NULL, visVar, "on");
htmlPageSetVar(emptyConfig, NULL, orderVarName, "nameSimilarity");
htmlPageSetVar(emptyConfig, NULL, countVarName, "25");

printPage = quickSubmit(emptyConfig, NULL, org, db, col, gene, "colPrint", "Submit", "on");
if (printPage != NULL)
    {
    int expectCount = 25;
    int lineCount = nearCountRows(printPage);
    if (lineCount != expectCount)
	qaStatusSoftError(nearTestList->status, 
		"Got %d rows, expected %d", lineCount, expectCount);
    }
quickErrReport();
htmlPageFree(&printPage);
htmlPageSetVar(emptyConfig, NULL, visVar, NULL);
}

struct htmlPage *emptyConfigPage(struct htmlPage *dbPage, char *org, char *db)
/* Get empty configuration page. */
{
return quickSubmit(dbPage, NULL, org, db, NULL, NULL, "emptyConfig", hideAllConfName, "on");
}

void testColInfo(struct htmlPage *dbPage, char *org, char *db, char *col) 
/* Click on all colInfo columns. */
{
struct htmlPage *infoPage = 
    quickSubmit(dbPage, NULL, org, db, col, NULL, "colInfo", colInfoVarName, col);

if (infoPage != NULL)
    {
    if (stringIn("No additional info available", infoPage->htmlText))
	qaStatusSoftError(nearTestList->status, 
		"%s failed - no %s.html?", colInfoVarName, col);
    }
quickErrReport();
htmlPageFree(&infoPage);
}

void testDbColumns(struct htmlPage *dbPage, char *org, char *db, 
	struct slName *geneList)
/* Test on one database. */
{
struct htmlPage *emptyConfig;
struct slName *colList = NULL, *col;
struct htmlFormVar *var;
struct slName *gene;

uglyf("testDbColumns %s %s\n", org, db);
emptyConfig = emptyConfigPage(dbPage, org, db);
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
	htmlPageSetVar(emptyConfig, NULL, searchVarName, gene->name);
	for (col = colList; col != NULL; col = col->next)
	    {
	    testCol(emptyConfig, org, db, col->name, gene->name);
	    }
	}
    for (col = colList; col != NULL; col = col->next)
        {
	testColInfo(dbPage, org, db, col->name);
	}
    }
htmlPageFree(&emptyConfig);
}


void testSort(struct htmlPage *emptyConfig, char *org, char *db, char *sort, char *gene, char *accColumn)
/* Test one column. */
{
char accVis[256];
struct htmlPage *printPage = NULL;
safef(accVis, sizeof(accVis), "near.col.%s.vis", accColumn);
htmlPageSetVar(emptyConfig, NULL, accVis, "on");
htmlPageSetVar(emptyConfig, NULL, orderVarName, sort);
htmlPageSetVar(emptyConfig, NULL, countVarName, "25");
htmlPageSetVar(emptyConfig, NULL, searchVarName, gene);

printPage = quickSubmit(emptyConfig, sort, org, db, NULL, gene, "sortType", "submit", "on");
if (printPage != NULL)
    {
    int lineCount = nearCountRows(printPage);
    if (lineCount < 1)
	qaStatusSoftError(nearTestList->status, "No rows for sort %s", sort);
    }
quickErrReport();
htmlPageFree(&printPage);
}



void testDbSorts(struct htmlPage *dbPage, char *org, char *db, 
	char *accColumn, struct slName *geneList)
/* Test on one database. */
{
struct htmlPage *emptyConfig;
struct htmlFormVar *sortVar = htmlFormVarGet(dbPage->forms, orderVarName);
struct slName *gene, *sort;

uglyf("testDbSorts %s %s\n", org, db);
if (sortVar == NULL)
    errAbort("Couldn't find var %s", orderVarName);

emptyConfig = emptyConfigPage(dbPage, org, db);
if (emptyConfig != NULL)
    {
    for (sort = sortVar->values; sort != NULL; sort= sort->next)
	{
	for (gene = geneList; gene != NULL; gene = gene->next)
	    {
	    testSort(emptyConfig, org, db, sort->name, gene->name, accColumn);
	    }
	}
    htmlPageFree(&emptyConfig);
    }
}

void testDbFilters(struct htmlPage *dbPage, char *org, char *db, 
	char *accColumn, struct slName *geneList)
/* Test filter that returns just geneList. */
{
struct slName *gene;
int rowCount;
char accFilter[256];

/* Start out with filter page. */
struct htmlPage *page = quickSubmit(dbPage, NULL, org, db, accColumn, NULL,
	"accOneFilterPage", advFilterVarName, "on");
verbose(1, "testFilters %s %s\n", org, db);
if (page == NULL)
    return;

/* Set up to filter exactly one gene. */
safef(accFilter, sizeof(accFilter), "near.as.%s.wild", accColumn);
    {
    htmlPageSetVar(page, NULL, accFilter, geneList->name);
    htmlPageSetVar(page, NULL, searchVarName, geneList->name);
    serialSubmit(&page, NULL, org, db, accColumn, geneList->name,
	    "accOneFilterSubmit", "Submit", "on");
    if (page == NULL)
	return;

    /* Make sure really got one gene. */
    rowCount = nearCountUniqAccRows(page);
    if (rowCount != 1)
	{
	qaStatusSoftError(nearTestList->status, 
	    "Acc exact filter returned %d items", rowCount);
	}
    }

/* Set up filter for all genes in list. */
    {
    struct dyString *dy = newDyString(0);
    int geneCount = slCount(geneList);
    for (gene = geneList; gene != NULL; gene = gene->next)
	dyStringPrintf(dy, "%s ", gene->name);
    htmlPageSetVar(page, NULL, accFilter, dy->string);
    htmlPageSetVar(page, NULL, countVarName, "all");  /* despite 3 genes requested, must see all if many dupes */
    serialSubmit(&page, NULL, org, db, accColumn, dy->string,
	    "accMultiFilterSubmit", "Submit", "on");
    dyStringFree(&dy);
    if (page == NULL)
	return;
    rowCount = nearCountUniqAccRows(page);
    if (rowCount != geneCount)
	{
	qaStatusSoftError(nearTestList->status, 
	    "Acc multi filter expecting %d, got %d items", geneCount, rowCount);
	}
    }

/* Set up filter for wildcard in list. */
    {
    struct dyString *dy = newDyString(0);
    char len = strlen(geneList->name);
    dyStringAppendN(dy, geneList->name, len-1);
    dyStringAppendC(dy, '*');
    htmlPageSetVar(page, NULL, accFilter, dy->string);
    serialSubmit(&page, NULL, org, db, accColumn, dy->string,
	    "accWildFilterSubmit", "Submit", "on");
    dyStringFree(&dy);
    if (page == NULL)
	return;
    rowCount = nearCountRows(page);
    if (rowCount < 1)
	{
	qaStatusSoftError(nearTestList->status, 
	    "Acc wild filter no match");
	}
    }


/* Clear out advanced filters. */
    {
    htmlPageFree(&page);
    page = quickSubmit(dbPage, NULL, org, db, NULL, NULL,
	"advFilterClear", advFilterClearVarName, "on");
    }
htmlPageFree(&page);
}

void testDb(struct htmlPage *orgPage, char *org, char *db)
/* Test on one database. */
{
struct hash *genomeRa = hgReadRa(org, db, dataDir, "genome.ra", NULL);
char *canonicalTable = hashMustFindVal(genomeRa, "canonicalTable");
char *accColumn = hashMustFindVal(genomeRa, "idColumn");

struct slName *geneList = sqlRandomSampleWithSeed(db, canonicalTable, "transcript", clRepeat, seed);
struct htmlPage *dbPage;
struct slName *ptr;

verbose(1, "genelist:\n");
for (ptr = geneList; ptr != NULL; ptr = ptr->next)
    verbose(1, "%s\n", ptr->name);

htmlPageSetVar(orgPage, NULL, "db", db);
htmlPageSetVar(orgPage, NULL, searchVarName, "");

dbPage = quickSubmit(orgPage, NULL, org, db, NULL, NULL, "dbEmptyPage", "submit", "go");

quickErrReport();
if (dbPage != NULL)
    {
    testDbColumns(dbPage, org, db, geneList);
    testDbSorts(dbPage, org, db, accColumn, geneList);
    testDbFilters(dbPage, org, db, accColumn, geneList);
    }

htmlPageFree(&dbPage);
hashFree(&genomeRa);
slNameFreeList(&geneList);
}


void testOrg(struct htmlPage *rootPage, struct htmlForm *rootForm, char *org, char *forceDb)
/* Test on organism.  If forceDb is non-null, only test on
 * given database. */
{
struct htmlPage *orgPage;
struct htmlForm *mainForm;
struct htmlFormVar *dbVar;
struct slName *db;
htmlPageSetVar(rootPage, rootForm, "org", org);
htmlPageSetVar(rootPage, rootForm, "db", NULL);
htmlPageSetVar(rootPage, rootForm, searchVarName, "");
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
int i;

AllocVar(stats);
for (i=0; i<=ntiiCol; ++i)
    statsOnSubsets(list, i, f);
for (test = list; test != NULL; test = test->next)
    qaStatisticsAdd(stats, test->status);
qaStatisticsReport(stats, "Total", f);
freeMem(stats);
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
FILE *f = mustOpen(log, "w");

htmlPageValidateOrAbort(rootPage);
htmlPageSetVar(rootPage, NULL, orderVarName, "geneDistance");
htmlPageSetVar(rootPage, NULL, countVarName, "25");
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

htmlPageFree(&rootPage);

slReverse(&nearTestList);

reportSummary(nearTestList, stdout);
fprintf(f,"seed=%d\n",seed);
reportAll(nearTestList, f);
fprintf(f, "---------------------------------------------\n");
reportSummary(nearTestList, f);
slFreeList(&nearTestList);
carefulClose(&f);
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

/* Seed the random number generator. */
seed = optionInt("seed",time(NULL));
printf("seed=%d\n",seed);
srand(seed);

hgNearTest(argv[1], argv[2]);
carefulCheckHeap();
return 0;
}
