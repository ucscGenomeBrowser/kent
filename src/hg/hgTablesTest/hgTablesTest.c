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
#include "chromInfo.h"

static char const rcsid[] = "$Id: hgTablesTest.c,v 1.6 2004/11/07 20:25:39 kent Exp $";

/* Command line variables. */
char *clOrg = NULL;	/* Organism from command line. */
char *clDb = NULL;	/* DB from command line */
int clGroups = BIGNUM;	/* Number of groups to test. */
int clTracks = 2;	/* Number of track to test. */
int clTables = 2;	/* Number of tables to test. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTablesTest - Test hgTables web page\n"
  "usage:\n"
  "   hgTablesTest url log\n"
  "options:\n"
  "   -org=Human - Restrict to Human (or Mouse, Fruitfly, etc.)\n"
  "   -db=hg17 - Restrict to particular database\n"
  "   -groups=N - Number of groups to test (default all)\n"
  "   -tracks=N - Number of tracks per group to test (default %d)\n"
  "   -tables=N - Number of tables per track to test (deault %d)\n"
  , clTracks, clTables);
}


static struct optionSpec options[] = {
   {"org", OPTION_STRING},
   {"db", OPTION_STRING},
   {"search", OPTION_STRING},
   {"groups", OPTION_INT},
   {"tracks", OPTION_INT},
   {"tables", OPTION_INT},
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
   ntiiOrg,
   ntiiDb,
   ntiiGroup,
   ntiiTrack,
   ntiiTable,
   ntiiTotalCount,
};

char *tablesTestInfoTypes[] =
   { "type", "organism", "db", "group", "track", "table"};

struct tablesTest *tablesTestList = NULL;	/* List of all tests, latest on top. */

struct tablesTest *tablesTestNew(struct qaStatus *status,
	char *type, char *org, char *db, char *group, 
	char *track, char *table)
/* Save away test test results. */
{
struct tablesTest *test;
AllocVar(test);
test->status = status;
test->info[ntiiType] = cloneString(naForNull(type));
test->info[ntiiOrg] = cloneString(naForNull(org));
test->info[ntiiDb] = cloneString(naForNull(db));
test->info[ntiiGroup] = cloneString(naForNull(group));
test->info[ntiiTrack] = cloneString(naForNull(track));
test->info[ntiiTable] = cloneString(naForNull(table));
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
	char *org, char *db, char *group, char *track, char *table,
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
    if (group != NULL)
        htmlPageSetVar(basePage, NULL, hgtaGroup, group);
    if (track != NULL)
        htmlPageSetVar(basePage, NULL, hgtaTrack, track);
    if (table != NULL)
        htmlPageSetVar(basePage, NULL, hgtaTable, table);
    qs = qaPageFromForm(basePage, basePage->forms, 
	    button, buttonVal, &page);
    test = tablesTestNew(qs, testName, org, db, group, track, table);
    }
return page;
}

void serialSubmit(struct htmlPage **pPage,
	char *org, char *db, char *group, char *track, char *table,
	char *testName, char *button, char *buttonVal)
/* Submit page, replacing old page with new one. */
{
struct htmlPage *oldPage = *pPage;
if (oldPage != NULL)
    {
    *pPage = quickSubmit(oldPage, org, db, group, track, table,
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

void testSchema(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table)
/* Make sure schema page comes up. */
{
struct htmlPage *schemaPage = quickSubmit(tablePage, org, db, group,
        track, table, "schema", hgtaDoSchema, "submit");
htmlPageFree(&schemaPage);
}

void testSummaryStats(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table)
/* Make sure summary stats page comes up. */
{
if (htmlFormVarGet(mainForm, hgtaDoSummaryStats) != NULL)
    {
    struct htmlPage *statsPage = quickSubmit(tablePage, org, db, group,
    	track, table, "summaryStats", hgtaDoSummaryStats, "submit");
    htmlPageFree(&statsPage);
    }
}

boolean outTypeAvailable(struct htmlForm *form, char *value)
/* Return true if outType options include value. */
{
struct htmlFormVar *outType = htmlFormVarGet(form, hgtaOutputType);
if (outType == NULL)
    errAbort("Couldn't find %s variable in form", hgtaOutputType);
return slNameInList(outType->values, value);
}

int countNoncommentLines(char *s)
/* Count number of lines in s that don't start with # */
{
int count = 0;
s = skipLeadingSpaces(s);
while (s != NULL && s[0] != 0)
    {
    if (s[0] != '#')
	++count;
    s = strchr(s, '\n');
    s = skipLeadingSpaces(s);
    }
return count;
}

int testAllFields(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table)
/* Get all fields and return count of rows. */
{
struct htmlPage *outPage;
int rowCount = 0;

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "primaryTable");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "allFields", hgtaDoTopSubmit, "submit");
rowCount = countNoncommentLines(outPage->htmlText);
htmlPageFree(&outPage);
return rowCount;
}

struct htmlFormVar *findPrefixedVar(struct htmlFormVar *list, char *prefix)
/* Find first var with given prefix in list. */
{
struct htmlFormVar *var;
for (var = list; var != NULL; var = var->next)
    {
    if (startsWith(prefix, var->name))
        return var;
    }
return NULL;
}

void checkExpectedSimpleRows(struct htmlPage *outPage, int expectedRows)
/* Make sure we got the number of rows we expect. */
{
int rowCount = countNoncommentLines(outPage->htmlText);
if (rowCount != expectedRows)
    qaStatusSoftError(tablesTestList->status, 
	    "Got %d rows, expected %d", rowCount, expectedRows);
}

void testOneField(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table, 
     int expectedRows)
/* Get one field and make sure the count agrees with expected. */
{
struct htmlPage *outPage;

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "selectedFields");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "selFieldsPage", hgtaDoTopSubmit, "submit");
if (outPage != NULL)
    {
    struct htmlForm *form = outPage->forms;
    struct htmlFormVar *var;
    int rowCount = 0;
    if (form == NULL)
        errAbort("No forms in select fields page");
    var = findPrefixedVar(form->vars, "hgta_fs.check.");
    if (var == NULL)
        errAbort("No hgta_fs.check. vars in form");
    htmlPageSetVar(outPage, NULL, var->name, "on");
    serialSubmit(&outPage, org, db, group, track, table, "oneField",
    	hgtaDoPrintSelectedFields, "submit");
    checkExpectedSimpleRows(outPage, expectedRows);
    }
htmlPageFree(&outPage);
}
	
void testOutBed(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table, 
     int expectedRows)
/* Get as bed and make sure count agrees with expected. */
{
struct htmlPage *outPage;

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "bed");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "bedUiPage", hgtaDoTopSubmit, "submit");
if (outPage != NULL)
    {
    serialSubmit(&outPage, org, db, group, track, table, "outBed",
    	hgtaDoGetBed, "submit");
    checkExpectedSimpleRows(outPage, expectedRows);
    }
htmlPageFree(&outPage);
}

void testOutGff(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table)
/* Get as GFF and make sure no crash. */
{
struct htmlPage *outPage;

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "gff");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "outGff", hgtaDoTopSubmit, "submit");
htmlPageFree(&outPage);
}

int countTagsBetween(struct htmlPage *page, char *start, char *end, char *type)
/* Count number of tags of given type (which should be upper case)
 * between start and end. If start is NULL it will start from
 * beginning of page.  If end is NULL it will end at end of page. */
{
int count = 0;
struct htmlTag *tag;
if (start == NULL)
    start = page->htmlText;
if (end == NULL)
    end = start + strlen(start);
for (tag = page->tags; tag != NULL; tag = tag->next)
    {
    if (tag->start >= start && tag->start < end && sameString(tag->name, type))
	{
        ++count;
	}
    }
return count;
}

void testOutHyperlink(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table, 
     int expectedRows)
/* Get as hyperlink and make sure count agrees with expected. */
{
struct htmlPage *outPage;

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "hyperlinks");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "outHyperlinks", hgtaDoTopSubmit, "submit");
if (outPage != NULL)
    {
    char *s = stringIn("<!--Content", outPage->htmlText);
    int rowCount;
    if (s == NULL) errAbort("Can't find <!-Content");
    rowCount = countTagsBetween(outPage, s, NULL, "A");
    if (rowCount != expectedRows)
	qaStatusSoftError(tablesTestList->status, 
		"Got %d rows, expected %d", rowCount, expectedRows);
    }
htmlPageFree(&outPage);
}

void testOutCustomTrack(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table)
/* Get as customTrack and make sure nothing explodes. */
{
struct htmlPage *outPage;

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "customTrack");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "customTrackUi", hgtaDoTopSubmit, "submit");
if (outPage != NULL)
    {
    struct htmlFormVar *groupVar;
    serialSubmit(&outPage, org, db, group, track, table, "outCustom",
    	hgtaDoGetCustomTrackTb, "submit");
    groupVar = htmlFormVarGet(outPage->forms, hgtaGroup);
    if (!slNameInList(groupVar->values, "user"))
	{
	qaStatusSoftError(tablesTestList->status, 
		"No custom track group after custom track submission");
	}
    }
htmlPageFree(&outPage);
}

	
	
void testOneTable(struct htmlPage *trackPage, char *org, char *db,
	char *group, char *track, char *table)
/* Test stuff on one table. */
{
struct htmlPage *tablePage = quickSubmit(trackPage, org, db, group, 
	track, table, "selectTable", hgtaTable, table);
struct htmlForm *mainForm;

if ((mainForm = htmlFormGet(tablePage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on tablePage");
testSchema(tablePage, mainForm, org, db, group, track, table);
testSummaryStats(tablePage, mainForm, org, db, group, track, table);
if (outTypeAvailable(mainForm, "primaryTable"))
    {
    int rowCount;
    rowCount = testAllFields(tablePage, mainForm, org, db, group, track, table);
    testOneField(tablePage, mainForm, org, db, group, track, table, rowCount);
    if (outTypeAvailable(mainForm, "bed"))
        {
	testOutBed(tablePage, mainForm, org, db, group, track, table, rowCount);
	testOutHyperlink(tablePage, mainForm, org, db, group, track, table, rowCount);
	testOutGff(tablePage, mainForm, org, db, group, track, table);
	testOutCustomTrack(tablePage, mainForm, org, db, group, track, table);
	}
    }
verbose(1, "Tested %s %s %s %s %s\n", org, db, group, track, table);
htmlPageFree(&tablePage);
}

void testOneTrack(struct htmlPage *groupPage, char *org, char *db,
	char *group, char *track, int maxTables)
/* Test a little something on up to maxTables in one track. */
{
struct htmlPage *trackPage = quickSubmit(groupPage, org, db, group, 
	track, NULL, "selectTrack", hgtaTrack, track);
struct htmlForm *mainForm;
struct htmlFormVar *tableVar;
struct slName *table;
int tableIx;

if ((mainForm = htmlFormGet(trackPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on trackPage");
if ((tableVar = htmlFormVarGet(mainForm, hgtaTable)) == NULL)
    errAbort("Can't find table var");
for (table = tableVar->values, tableIx = 0; 
	table != NULL && tableIx < maxTables; 
	table = table->next, ++tableIx)
    {
    testOneTable(trackPage, org, db, group, track, table->name);
    }
/* Clean up. */
htmlPageFree(&trackPage);
}

void testOneGroup(struct htmlPage *dbPage, char *org, char *db, char *group, 
	int maxTracks)
/* Test a little something on up to maxTracks in one group */
{
struct htmlPage *groupPage = quickSubmit(dbPage, org, db, group, NULL, NULL,
	"selectGroup", hgtaGroup, group);
struct htmlForm *mainForm;
struct htmlFormVar *trackVar;
struct slName *track;
int trackIx;

if ((mainForm = htmlFormGet(groupPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on groupPage");
if ((trackVar = htmlFormVarGet(mainForm, hgtaTrack)) == NULL)
    errAbort("Can't find track var");
for (track = trackVar->values, trackIx = 0; 
	track != NULL && trackIx < maxTracks; 
	track = track->next, ++trackIx)
    {
    testOneTrack(groupPage, org, db, group, track->name, clTables);
    }

/* Clean up. */
htmlPageFree(&groupPage);
}

void testGroups(struct htmlPage *dbPage, char *org, char *db, int maxGroups)
/* Test a little something in all groups for dbPage. */
{
struct htmlForm *mainForm;
struct htmlFormVar *groupVar;
struct slName *group;
int groupIx;

if ((mainForm = htmlFormGet(dbPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on dbPage");
if ((groupVar = htmlFormVarGet(mainForm, hgtaGroup)) == NULL)
    errAbort("Can't find group var");
for (group = groupVar->values, groupIx=0; 
	group != NULL && groupIx < maxGroups; 
	group = group->next, ++groupIx)
    {
    if (!sameString("allTables", group->name))
        testOneGroup(dbPage, org, db, group->name, clTracks);
    }
}

void getTestRegion(char *db, char region[256], int regionSize)
/* Look up first chromosome in database and grab five million bases
 * from the middle of it. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr = sqlGetResult(conn, "select * from chromInfo limit 1");
char **row;
struct chromInfo ci;
int start,end,middle;

if ((row = sqlNextRow(sr)) == NULL)
    errAbort("Couldn't get one row from chromInfo");
chromInfoStaticLoad(row, &ci);
middle = ci.size/2;
start = middle-2500000;
end = middle+2500000;
if (start < 0) start = 0;
if (end > ci.size) end = ci.size;
safef(region, regionSize, "%s:%d-%d", ci.chrom, start+1, end);
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

void testDb(struct htmlPage *orgPage, char *org, char *db)
/* Test on one database. */
{
struct htmlPage *dbPage;
char region[256];
htmlPageSetVar(orgPage, NULL, "db", db);
getTestRegion(db, region, sizeof(region));
htmlPageSetVar(orgPage, NULL, "position", region);
htmlPageSetVar(orgPage, NULL, hgtaRegionType, "range");
dbPage = quickSubmit(orgPage, org, db, NULL, NULL, NULL, "selectDb", "submit", "go");
quickErrReport();
if (dbPage != NULL)
    {
    testGroups(dbPage, org, db, clGroups);
    }
htmlPageFree(&dbPage);
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
clGroups = optionInt("groups", clGroups);
clTracks = optionInt("tracks", clTracks);
clTables = optionInt("tables", clTables);
hgTablesTest(argv[1], argv[2]);
carefulCheckHeap();
return 0;
}
