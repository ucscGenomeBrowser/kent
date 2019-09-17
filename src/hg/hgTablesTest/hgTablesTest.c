/* hgTablesTest - Test hgTables web page. */

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
#include "../hgTables/hgTables.h"
#include "hdb.h"
#include "qa.h"
#include "chromInfo.h"
#include "obscure.h"
#include <unistd.h>
#include <limits.h>

#ifndef HOST_NAME_MAX
// needed for OS/X
#define HOST_NAME_MAX _POSIX_HOST_NAME_MAX
#endif

#define MAX_ATTEMPTS 10


/* Command line variables. */
char *clOrg = NULL;	/* Organism from command line. */
char *clDb = NULL;	/* DB from command line */
char *clGroup = NULL;	/* Group from command line. */
char *clTrack = NULL;	/* Track from command line. */
char *clTable = NULL;	/* Table from command line. */
int clGroups = BIGNUM;	/* Number of groups to test. */
int clTracks = 4;	/* Number of track to test. */
int clTables = 2;	/* Number of tables to test. */
int clDbs = 1;		/* Number of databases per organism. */
int clOrgs = 2;		/* Number of organisms to test. */
boolean appendLog;      /* Append to log rather than create it. */
boolean noShuffle;      /* Suppress shuffling of track and table lists. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTablesTest - Test hgTables web page\n"
  "usage:\n"
  "   hgTablesTest url log\n"
  "Where url is something like hgwbeta.soe.ucsc.edu/cgi-bin/hgTables\n"
  "and log is a file where error messages and statistics will be written\n"
  "options:\n"
  "   -org=Human - Restrict to Human (or Mouse, Fruitfly, etc.)\n"
  "   -db=hg17 - Restrict to particular database\n"
  "   -group=genes - Restrict to a particular group\n"
  "   -track=knownGene - Restrict to a particular track\n"
  "   -table=knownGeneMrna - Restrict to a particular table\n"
  "   -orgs=N - Number of organisms to test.  Default %d\n"
  "   -dbs=N - Number of databases per organism to test. Default %d\n"
  "   -groups=N - Number of groups to test (default all)\n"
  "   -tracks=N - Number of tracks per group to test (default %d)\n"
  "   -tables=N - Number of tables per track to test (default %d)\n"
  "   -verbose=N - Set to 0 for silent operation, 2 or 3 for debugging\n"
  "   -appendLog - Append to log file rather than creating it\n"
  "   -seed N - Specify seed for random number generator as debugging aid.\n"
  "   -noShuffle - do not shuffle tracks and tables lists.\n"
  , clOrgs, clDbs, clTracks, clTables);
}

FILE *logFile;	/* Log file. */
int seed = 0;           /* seed for random number generator */

static struct optionSpec options[] = 
{
    {"org", OPTION_STRING},
    {"db", OPTION_STRING},
    {"group", OPTION_STRING},
    {"track", OPTION_STRING},
    {"table", OPTION_STRING},
    {"orgs", OPTION_INT},
    {"dbs", OPTION_INT},
    {"search", OPTION_STRING},
    {"groups", OPTION_INT},
    {"tracks", OPTION_INT},
    {"tables", OPTION_INT},
    {"appendLog", OPTION_BOOLEAN},
    {"seed", OPTION_INT},
    {"noShuffle", OPTION_BOOLEAN},
    {NULL, 0},
};

struct tablesTest
/* Test on one column. */
    {
    struct tablesTest *next;
    struct qaStatus *status;	/* Result of test. */
    char *info[6];
    };

enum tablesTestInfoIx 
{
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
struct htmlPage *page = NULL;

// don't get ahead of the botDelay
sleep1000(5000);

verbose(2, "quickSubmit(%p, %s, %s, %s, %s, %s, %s, %s, %s)\n",
	basePage, naForNull(org), naForNull(db), naForNull(group), 
	naForNull(track), naForNull(table), naForNull(testName), 
	naForNull(button), naForNull(buttonVal));
if (basePage != NULL)
    {
    struct qaStatus *qs;
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

    if (!page)
	{
	verbose(2, "page is NULL, qs->errMessage=[%s]\n", qs->errMessage);
	if (startsWith("carefulAlloc: Allocated too much memory", qs->errMessage))
	    {
	          verbose(1, "Response html page too large (500MB) (%s %s %s %s %s)\n", org, db, group, track, table);
	    fprintf(logFile, "Response html page too large (500MB) (%s %s %s %s %s)\n", org, db, group, track, table);
	    }
	}

    /* 
    if (page->forms != NULL)
        htmlFormPrint(page->forms, stdout);
    */
    // do not need to keep the returned structure, the answer is accumulating
    // in global variable: tablesTestList
    (void) tablesTestNew(qs, testName, org, db, group, track, table);
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

int tableSize(char *db, char *table)
/* Return number of rows in table. */
{
struct sqlConnection *conn = sqlConnect(db);
int size = sqlTableSize(conn, table);
sqlDisconnect(&conn);
return size;
}

void showConnectInfo(char *db)
/* Show connection info used by this program. */
{
struct sqlConnection *conn = sqlConnect(db);
char *user = sqlQuickString(conn, NOSQLINJ "select current_user()");
char *hostinfo = sqlHostInfo(conn);
      verbose(1, "Connecting as %s to database server %s\n", user, hostinfo);
fprintf(logFile, "Connecting as %s to database server %s\n", user, hostinfo); fflush(logFile);
sqlDisconnect(&conn);
}

void showRunningHostName()
/* Show hostname of the machine we are running on. */
{
char hostname[HOST_NAME_MAX];
if (gethostname(hostname, sizeof hostname))
    {
    perror("gethostname");
    safecpy(hostname, sizeof hostname, "error-reading-hostname");
    }
      verbose(1, "Running on machine %s\n", hostname);
fprintf(logFile, "Running on machine %s\n", hostname); fflush(logFile);
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
/* mainForm not used */
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

boolean varIncludesType(struct htmlForm *form, char *var, char *value)
/* Return TRUE if value is one of the options for var. */
{
struct htmlFormVar *formVar = htmlFormVarGet(form, var);
if (formVar == NULL)
    errAbort("Couldn't find %s variable in form", var);
return slNameInList(formVar->values, value);
}

boolean outTypeAvailable(struct htmlForm *form, char *value)
/* Return true if outType options include value. */
{
return varIncludesType(form, hgtaOutputType, value);
}

int countNoncommentLines(char *s)
/* Count number of lines in s that don't start with # */
{
int count = 0;
// beware, skipLeadingSpaces skips '\n' (isspace) so it skips blank lines.  Check for '\n' first.
if (s && *s != '\n')
    s = skipLeadingSpaces(s);
while (s != NULL && s[0] != 0)
    {
    if (s[0] != '#')
	++count;
    s = strchr(s, '\n');
    if (s != NULL)
        {
        s++;
        if (*s != '\n')
            s = skipLeadingSpaces(s);
        }
    }
return count;
}

int testAllFields(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table)
/* Get all fields and return count of rows. */
/* mainForm not used */
{
struct htmlPage *outPage;
int rowCount = 0;

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "primaryTable");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "allFields", hgtaDoTopSubmit, "submit");
/* check for NULL outPage */
if (outPage == NULL)
    {
          verbose(1, "Null page in testAllFields (%s %s %s %s %s)\n", org, db, group, track, table);
    fprintf(logFile, "Null page in testAllFields (%s %s %s %s %s)\n", org, db, group, track, table);
    return -1;
    }
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
if (outPage != NULL)
    {
    char *results = outPage->htmlText;
    if (startsWith(NO_RESULTS, results))
        results += strlen(NO_RESULTS);
    int rowCount = countNoncommentLines(results);
    if (rowCount != expectedRows)
	qaStatusSoftError(tablesTestList->status, 
		"Got %d rows, expected %d", rowCount, expectedRows);
    }
}

void testOneField(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table, 
     int expectedRows)
/* Get one field and make sure the count agrees with expected. */
/* mainForm not used */
{
struct htmlPage *outPage;
struct htmlForm *form;
struct htmlFormVar *var;
int attempts = 0;

if (tablePage->forms == NULL) 
     errAbort("testOneField: Missing form (tablePage)");

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "selectedFields");

outPage = quickSubmit(tablePage, org, db, group, track, table,
    "selFieldsPage", hgtaDoTopSubmit, "submit");
while (outPage == NULL && attempts < MAX_ATTEMPTS)
    {
    printf("testOneField: trying again to get selFieldsPage\n");
    outPage = quickSubmit(tablePage, org, db, group, track, table,
        "selFieldsPage", hgtaDoTopSubmit, "submit");
    attempts++;
    }

if (outPage == NULL) 
    {
    qaStatusSoftError(tablesTestList->status,
           "Error in testOneField - couldn't get outPage.");
    return;
    }

if (outPage->forms == NULL)
    {
    qaStatusSoftError(tablesTestList->status,
           "Error in testOneField - missing form.");
    htmlPageFree(&outPage);
    return;
}

form = outPage->forms;
var = findPrefixedVar(form->vars, "hgta_fs.check.");
if (var == NULL)
    errAbort("No hgta_fs.check. vars in form");
htmlPageSetVar(outPage, NULL, var->name, "on");
serialSubmit(&outPage, org, db, group, track, table, "oneField",
    hgtaDoPrintSelectedFields, "submit");
// check that outPage != NULL
checkExpectedSimpleRows(outPage, expectedRows);
htmlPageFree(&outPage);
}
	
void testOutBed(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table, 
     int expectedRows)
/* Get as bed and make sure count agrees with expected. */
/* mainForm not used */
{
struct htmlPage *outPage;
int attempts = 0;

if (tablePage->forms == NULL) 
     errAbort("testOutBed: Missing form (tablePage)");

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "bed");

outPage = quickSubmit(tablePage, org, db, group, track, table,
    "bedUiPage", hgtaDoTopSubmit, "submit");
while (outPage == NULL && attempts < MAX_ATTEMPTS)
    {
    printf("testOutBed: trying again to get bedUiPage\n");
    outPage = quickSubmit(tablePage, org, db, group, track, table,
       "bedUiPage", hgtaDoTopSubmit, "submit");
    attempts++;
    }
if (outPage == NULL)
    {
    qaStatusSoftError(tablesTestList->status,
           "Error in testOneBed - couldn't get outPage.");
    return;
    }
if (outPage->forms == NULL)
    {
    qaStatusSoftError(tablesTestList->status,
           "Error in testOneBed - missing form.");
    htmlPageFree(&outPage);
    return;
    }

serialSubmit(&outPage, org, db, group, track, table, "outBed", hgtaDoGetBed, "submit");
// check that outPage != NULL
checkExpectedSimpleRows(outPage, expectedRows);
htmlPageFree(&outPage);
}

void testOutGff(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table)
/* Get as GFF and make sure no crash. */
/* mainForm not used */
{
struct htmlPage *outPage;

if (tablePage->forms == NULL) 
     errAbort("testOutGff: Missing form (tablePage)");
htmlPageSetVar(tablePage, NULL, hgtaOutputType, "gff");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "outGff", hgtaDoTopSubmit, "submit");
/* no checking here */
if (outPage != NULL)
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
/* mainForm not used */
{
struct htmlPage *outPage;
int attempts = 0;
char *s;
int rowCount;

if (tablePage->forms == NULL) 
     errAbort("testOutHyperlink: Missing form (tablePage)");
htmlPageSetVar(tablePage, NULL, hgtaOutputType, "hyperlinks");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "outHyperlinks", hgtaDoTopSubmit, "submit");
while (outPage == NULL && attempts < MAX_ATTEMPTS)
    {
    printf("testOutHyperLink: trying again to get outHyperLinks\n");
    outPage = quickSubmit(tablePage, org, db, group, track, table,
        "outHyperlinks", hgtaDoTopSubmit, "submit");
    attempts++;
    }

if (outPage == NULL) 
    {
    qaStatusSoftError(tablesTestList->status,
           "Error in testOutHyperLink - couldn't get outPage.");
    return;
    }

s = stringIn("<!-- +++++++++++++++++++++ CONTENT TABLES +++++++++++++++++++ -->", outPage->htmlText);
if (s == NULL) errAbort("Can't find <!-- +++++++++++++++++++++ CONTENT TABLES +++++++++++++++++++ -->");
rowCount = countTagsBetween(outPage, s, NULL, "A");
if (rowCount != expectedRows)
    qaStatusSoftError(tablesTestList->status, "Got %d rows, expected %d", rowCount, expectedRows);
htmlPageFree(&outPage);
}

void testOutCustomTrack(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table)
/* Get as customTrack and make sure nothing explodes. */
/* mainForm not used */
{
struct htmlPage *outPage;
int attempts = 0;
struct htmlFormVar *groupVar;

if (tablePage->forms == NULL) 
    errAbort("testOutCustomTrack: Missing form (tablePage)");

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "customTrack");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "customTrackUi", hgtaDoTopSubmit, "submit");
while (outPage == NULL && attempts < MAX_ATTEMPTS)
    {
    printf("testOutCustomTrack: trying again to get customTrackUi\n");
    outPage = quickSubmit(tablePage, org, db, group, track, table,
        "customTrackUi", hgtaDoTopSubmit, "submit");
    attempts++;
    }
if (outPage == NULL)
    {
    qaStatusSoftError(tablesTestList->status,
           "Error in testOutCustomTrack - couldn't get outPage.");
    return;
    }

serialSubmit(&outPage, org, db, group, track, table, "outCustom", hgtaDoGetCustomTrackTb, "submit");
if (outPage == NULL)
    {
    qaStatusSoftError(tablesTestList->status,
           "Error in testOutCustomTrack - serialSubmit returned null page.");
    return;
    }
if (outPage->forms == NULL)
    {
    qaStatusSoftError(tablesTestList->status,
           "Error in custom track - no form produced.");
    htmlPageFree(&outPage);
    return;
    }
groupVar = htmlFormVarGet(outPage->forms, hgtaGroup);
if (!slNameInList(groupVar->values, "user"))
    {
    qaStatusSoftError(tablesTestList->status, 
	"No custom track group after custom track submission");
    }
htmlPageFree(&outPage);
}

void checkFaOutput(struct htmlPage *page, int expectedCount, boolean lessOk)
/* Check that page contains expected number of sequences.  If lessOk is set
 * (needed to handle some multiply mapped cases in refSeq) then just check
 * that have at least one if expecting any. */
{
if (page != NULL)
    {
    int count = countChars(page->htmlText, '>');
    if (count != expectedCount)
	{
	if (!lessOk || count > expectedCount || (expectedCount > 0 && count <= 0))
	    qaStatusSoftError(tablesTestList->status, 
		    "Got %d sequences, expected %d", count, expectedCount);
	}
    }
}

void testOutSequence(struct htmlPage *tablePage, struct htmlForm *mainForm,
     char *org, char *db, char *group, char *track, char *table, 
     int expectedRows)
/* Get as sequence and make sure count agrees with expected. */
/* mainForm not used */
{
struct htmlPage *outPage;
int attempts = 0;
struct htmlFormVar *typeVar;

if (tablePage->forms == NULL) 
    errAbort("testOutSequence: Missing form (tablePage)");

htmlPageSetVar(tablePage, NULL, hgtaOutputType, "sequence");
outPage = quickSubmit(tablePage, org, db, group, track, table,
    "seqUi1", hgtaDoTopSubmit, "submit");
while (outPage == NULL && attempts < MAX_ATTEMPTS) 
    {
    printf("testOutSequence: trying again to get seqUi1\n");
    outPage = quickSubmit(tablePage, org, db, group, track, table,
        "seqUi1", hgtaDoTopSubmit, "submit");
    attempts++;
    }
if (outPage == NULL) 
    {
    qaStatusSoftError(tablesTestList->status,
        "Error in testOutSequence - couldn't get outPage");
    return;
    }
if (outPage->forms == NULL)
    {
    qaStatusSoftError(tablesTestList->status,
        "Error in testOutSequence - missing form");
    htmlPageFree(&outPage);
    return;
    }

/* Since some genomic sequence things are huge, this will
 * only test in case where it's a gene prediction. */
typeVar = htmlFormVarGet(outPage->forms, hgtaGeneSeqType);
if (typeVar != NULL)
    {
    struct htmlPage *seqPage;
    static char *types[] = {"protein", "mRNA"};
    int i;
    for (i=0; i<ArraySize(types); ++i)
        {
        char *type = types[i];
        if (slNameInList(typeVar->values, type))
             {
	     struct htmlPage *page;
	     char testName[128];
	     htmlPageSetVar(outPage, NULL, hgtaGeneSeqType, type);
	     safef(testName, sizeof(testName), "%sSeq", type);
	     page = quickSubmit(outPage, org, db, group, track, table,
	        testName, hgtaDoGenePredSequence, "submit");
	     checkFaOutput(page, expectedRows, TRUE);
	     htmlPageFree(&page);
	     }
         }
    htmlPageSetVar(outPage, NULL, hgtaGeneSeqType, "genomic");
    serialSubmit(&outPage, org, db, group, track, table, "seqUi2", hgtaDoGenePredSequence, "submit");
    // check that outPage != NULL

    /* On genomic page uncheck intron if it's there, then get results * and count them. */
    if (htmlFormVarGet(outPage->forms, "hgSeq.intron") != NULL)
         htmlPageSetVar(outPage, NULL, "hgSeq.intron", NULL);
    seqPage = quickSubmit(outPage, org, db, group, track, table, "genomicSeq", hgtaDoGenomicDna, "submit");
    // check that seqPage != NULL
    checkFaOutput(seqPage, expectedRows, FALSE);
    htmlPageFree(&seqPage);
    }

htmlPageFree(&outPage);
}
	
boolean isObsolete(char *table)
/* Some old table types we can't handle.  Just warn that
 * they are there and skip. */
{
boolean obsolete = sameString(table, "wabaCbr");
if (obsolete)
    qaStatusSoftError(tablesTestList->status, 
	"Skipping obsolete table %s", table);
return obsolete;
}

void testOneTable(struct htmlPage *trackPage, char *org, char *db,
	char *group, char *track, char *table)
/* Test stuff on one table if we haven't already tested this table. */
{
/* Why declared here and not globally? */
static struct hash *uniqHash = NULL;
char fullName[256];
if (uniqHash == NULL)
     uniqHash = newHash(0);
safef(fullName, sizeof(fullName), "%s.%s", db, table);
if (!hashLookup(uniqHash, fullName))
    {
    struct htmlPage *tablePage;
    struct htmlForm *mainForm;

    hashAdd(uniqHash, fullName, NULL);
    verbose(1, "Testing %s %s %s %s %s\n", naForNull(org), db, group, track, table);
    tablePage = quickSubmit(trackPage, org, db, group, 
	    track, table, "selectTable", hgtaTable, table);
    if (!isObsolete(table) && tablePage != NULL)
	{
	if ((mainForm = htmlFormGet(tablePage, "mainForm")) == NULL)
	    {
	    qaStatusSoftError(tablesTestList->status, 
		    "Couldn't get main form on tablePage for %s %s %s %s", db, group, track, table);
	    }
	else
	    {
	    verbose(3, "testOneTable testSchema() got here 1.1\n");
	    testSchema(tablePage, mainForm, org, db, group, track, table);
	    verbose(3, "testOneTable testSummaryStats() got here 1.2\n");
	    testSummaryStats(tablePage, mainForm, org, db, group, track, table);
	    verbose(3, "testOneTable got here 1.3\n");
	    if (outTypeAvailable(mainForm, "bed")) 
		{
		verbose(3, "testOneTable bed output avail means can filter on position got here 2\n");
		if (outTypeAvailable(mainForm, "primaryTable"))
		    {
		    verbose(3, "testOneTable got here 3\n");
		    int rowCount = testAllFields(tablePage, mainForm, org, db, group, track, table);
		    if (rowCount >= 0)
			{
			testOneField(tablePage, mainForm, org, db, group, track, table, rowCount);
			testOutSequence(tablePage, mainForm, org, db, group, track, table, rowCount);
			testOutBed(tablePage, mainForm, org, db, group, track, table, rowCount);
			testOutHyperlink(tablePage, mainForm, org, db, group, track, table, rowCount);
			testOutGff(tablePage, mainForm, org, db, group, track, table);
			if (rowCount > 0)
			    testOutCustomTrack(tablePage, mainForm, org, db, group, track, table);
			}
		    }
		}
	    else if (outTypeAvailable(mainForm, "primaryTable"))
		{
		verbose(3, "testOneTable no bed output available, so no position filtering available. got here 4\n");
		/* If BED type is not available then the region will be ignored, and
		 * we'll end up scanning whole table.  Make sure table is not huge
		 * before proceeding. */
		int tableRows = tableSize(db, table);
		if (tableRows < 500000)
		    {
		    int rowCount = testAllFields(tablePage, mainForm, org, db, group, track, table);
		    if (rowCount >= 0)
			testOneField(tablePage, mainForm, org, db, group, track, table, rowCount);
		    }
		else
		    {
			  verbose(1, "%s.%s tableRows=%d, too large >= 500000, skipping.\n", db, table, tableRows);
		    fprintf(logFile, "%s.%s tableRows=%d, too large >= 500000, skipping.\n", db, table, tableRows);
		    }
		}
	    }
	htmlPageFree(&tablePage);
	}
    carefulCheckHeap();
    }
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

if (trackPage == NULL)
    {
    // is this an exception?
    // exception for bigPsl (2016-06-20), may be short-lived.
    struct sqlConnection *conn = sqlConnect(db);
    char query[256];
    sqlSafef(query, sizeof query, "select type from trackDb where tableName='%s'", track);
    char *type = sqlQuickString(conn, query);
    sqlDisconnect(&conn);
    if (sameString(type, "bigPsl"))
	{
    	      verbose(1, "Skipping testing track %s since type bigPsl not supported by hgTables at this time (2016-06-20)\n", track);
    	fprintf(logFile, "Skipping testing track %s since type bigPsl not supported by hgTables at this time (2016-06-20)\n", track);
	return;
	}
    else
	errAbort("Couldn't select track %s", track);
    }
if ((mainForm = htmlFormGet(trackPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on trackPage");
if ((tableVar = htmlFormVarGet(mainForm, hgtaTable)) == NULL)
    errAbort("Can't find table var");

// put the tables in random order:
if (!noShuffle)
    shuffleList(&tableVar->values);

for (table = tableVar->values, tableIx = 0; 
	table != NULL && tableIx < maxTables; 
	table = table->next)
    {
    if (clTable && !sameString(clTable, table->name))
	continue;
    testOneTable(trackPage, org, db, group, track, table->name);
    ++tableIx;
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

if (groupPage == NULL)
    errAbort("Error when changing group to %s", group);
if ((mainForm = htmlFormGet(groupPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on groupPage");
if ((trackVar = htmlFormVarGet(mainForm, hgtaTrack)) == NULL)
    errAbort("Can't find track var");

// put the tracks in random order:
if (!noShuffle)
    shuffleList(&trackVar->values);

for (track = trackVar->values, trackIx = 0; 
	track != NULL && trackIx < maxTracks; 
	track = track->next)
    {
    if (clTrack && !sameString(track->name, clTrack))
	continue;
    testOneTrack(groupPage, org, db, group, track->name, clTables);
    ++trackIx;
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
	group = group->next)
    {
    if (!sameString("allTables", group->name))
	{
	if (clGroup && !sameString(clGroup, group->name))
	    continue;
	testOneGroup(dbPage, org, db, group->name, clTracks);
	++groupIx;
	}
    }
}

void getTestRegion(char *db, char region[256], int regionSize)
/* Look up first chromosome in database and grab five million bases
 * from the middle of it. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr = sqlGetResult(conn, NOSQLINJ "select * from chromInfo limit 1");
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
verbose(1, "Testing %s at position %s\n", db, region);
fprintf(logFile, "Testing %s at position %s\n", db, region);
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
if (dbPage != NULL)
    testGroups(dbPage, org, db, clGroups);
htmlPageFree(&dbPage);
}


void testOrg(struct htmlPage *rootPage, struct htmlForm *rootForm, char *org)
/* Test on organism.  */
{
struct htmlPage *orgPage;
struct htmlForm *mainForm;
struct htmlFormVar *dbVar;
struct slName *db;
int dbIx;

/* Get page with little selected beyond organism.  This page
 * will get whacked around a little by testDb, so set range
 * position and db to something safe each time through. */
htmlPageSetVar(rootPage, rootForm, "org", org);
htmlPageSetVar(rootPage, NULL, "db", NULL); 
htmlPageSetVar(rootPage, NULL, hgtaRegionType, NULL); 
htmlPageSetVar(rootPage, NULL, "position", NULL); 
orgPage = htmlPageFromForm(rootPage, rootPage->forms, "submit", "Go");
if ((mainForm = htmlFormGet(orgPage, "mainForm")) == NULL)
    {
    errAbort("Couldn't get main form on orgPage");
    }
if ((dbVar = htmlFormVarGet(mainForm, "db")) == NULL)
    errAbort("Couldn't get org var");
for (db = dbVar->values, dbIx=0; db != NULL && dbIx < clDbs; 
	db = db->next, ++dbIx)
    {
    testDb(orgPage, org, db->name);
    }
htmlPageFree(&orgPage);
}

void verifyJoinedFormat(char *s)
/* Verify that s consists of lines with two tab-separated fields,
 * and that the second field has some n/a and some comma-separated lists. */
{
char *e;
int lineIx = 0;
boolean gotCommas = FALSE, gotNa = FALSE;

while (s != NULL && s[0] != 0)
    {
    char *row[3];
    int fieldCount;
    ++lineIx;
    e = strchr(s, '\n');
    if (e != NULL)
       *e++ = 0;
    if (s[0] != '#')
	{
	fieldCount = chopTabs(s, row);
	if (fieldCount != 2)
	    {
	    qaStatusSoftError(tablesTestList->status, 
		    "Got %d fields line %d of  joined result, expected 2", 
		    fieldCount, lineIx);
	    break;
	    }
	if (sameString(row[1], "n/a"))
	     gotNa = TRUE;
	if (countChars(row[1], ',') >= 2)
	     gotCommas = TRUE;
	}
    s = e;
    }
if (!gotCommas)
    qaStatusSoftError(tablesTestList->status, 
           "Expected some rows in join to have comma separated lists.");
if (!gotNa)
    qaStatusSoftError(tablesTestList->status, 
           "Expected some rows in join to have n/a.");
}


void testJoining(struct htmlPage *rootPage)
/* Simulate pressing buttons to get a reasonable join on a
 * couple of uniProt tables. */
{
struct htmlPage *allPage, *page;
char *org = NULL, *db = NULL, *group = "allTables", *track="uniProt";
int expectedCount = tableSize("uniProt", "taxon");

allPage = quickSubmit(rootPage, org, db, group, "uniProt", 
	"uniProt.taxon", "taxonJoin1", NULL, NULL);
if (allPage != NULL)
    {
    if (allPage->forms == NULL)
        {
	errAbort("uniProt page with no form");
	}
    else
	{
	int count = testAllFields(allPage, allPage->forms, org, db,
	    group, track, "uniProt.taxon");
	if (count != expectedCount)
	    qaStatusSoftError(tablesTestList->status, 
		    "Got %d rows in uniProt.taxon, expected %d", count, 
		    expectedCount);
	htmlPageSetVar(allPage, NULL, hgtaOutputType, "selectedFields");
	page = quickSubmit(allPage, org, db, group, track, 
	    "uniProt.taxon", "taxonJoin2", hgtaDoTopSubmit, "submit");
	htmlPageSetVar(page, NULL, "hgta_fs.linked.uniProt.commonName", "on");
	serialSubmit(&page, org, db, group, track, NULL, "taxonJoin3",
	    hgtaDoSelectFieldsMore, "submit");
	if (page != NULL)
	    {
	    htmlPageSetVar(page, NULL, "hgta_fs.check.uniProt.taxon.binomial", "on");
	    htmlPageSetVar(page, NULL, "hgta_fs.check.uniProt.commonName.val", "on");
	    serialSubmit(&page, org, db, group, track, NULL, "taxonJoin4",
		hgtaDoPrintSelectedFields, "submit");
	    if (page != NULL)
		{
		checkExpectedSimpleRows(page, expectedCount);
		verifyJoinedFormat(page->htmlText);
		htmlPageFree(&page);
		}
	    }
	}
    }

htmlPageFree(&allPage);
verbose(1, "Tested joining on uniProt.taxon & commonName\n");
}

void checkXenopus(char *s)
/* Check that all lines start with xenopus, and that we
 * see laevis in there somewhere. */
{
char *e;
boolean gotLaevis = FALSE;
while (s != NULL && s[0] != 0)
    {
    s = skipLeadingSpaces(s);
    e = strchr(s, '\n');
    if (e != NULL)
        *e++ = 0;
    if (s[0] != '#')
	{
	char *t = strchr(s, '\t');
	if (t != NULL)
	    *t = 0;
	if (!startsWith("Xenopus", s))
	    {
	    qaStatusSoftError(tablesTestList->status, 
	         "Xenopus filter passing non-Xenopus");
	    return;
	    }
	if (sameString(s, "Xenopus laevis"))
	    gotLaevis = TRUE;
	}
    s = e;
    }
if (!gotLaevis)
    qaStatusSoftError(tablesTestList->status, 
	 "Can't find Xenopus laevis in filtered uniProt.taxon");
}

void testFilter(struct htmlPage *rootPage)
/* Simulate pressing buttons to get a reasonable filter on
 * uniProt taxon. */
{
char *org = NULL, *db = NULL, *group = "allTables", *track="uniProt",
	*table = "uniProt.taxon";
struct htmlPage *page;
page = quickSubmit(rootPage, org, db, group, "uniProt", 
	table, "taxonFilter1", hgtaDoFilterPage, "submit");
if (page != NULL)
    {
    struct htmlFormVar *var = htmlFormVarGet(page->forms, 
    	"hgta_fil.v.uniProt.taxon.binomial.pat");
    if (var == NULL)
        internalErr();
    htmlPageSetVar(page, NULL, "hgta_fil.v.uniProt.taxon.binomial.pat",
        "Xenopus*");
    serialSubmit(&page, org, db, group, track, table, "taxonFilter2",
    	hgtaDoFilterSubmit, "submit");
    if (page != NULL)
        {
	htmlPageSetVar(page, NULL, hgtaOutputType, "selectedFields");
	serialSubmit(&page, org, db, group, track, table, "taxonFilter3",
	    hgtaDoTopSubmit, "submit");
	if (page != NULL)
	    {
	    htmlPageSetVar(page, NULL, "hgta_fs.check.uniProt.taxon.binomial",
	    	"on");
	    serialSubmit(&page, org, db, group, track, table, "taxonFilter4",
		hgtaDoPrintSelectedFields, "submit");
	    if (page != NULL)
		checkXenopus(page->htmlText);
	    htmlPageFree(&page);
	    }
	}
    }
verbose(1, "Tested filter on uniProt.taxon\n");
}

void testIdentifier(struct htmlPage *rootPage)
/* Do simple check on identifiers. Relies on
 * 8355	Xenopus laevis being stable taxon (and not being filtered out
 * by testFilter). */
{
char *org = NULL, *db = NULL, *group = "allTables", *track="uniProt",
	*table = "uniProt.taxon";
struct htmlPage *page;
page = quickSubmit(rootPage, org, db, group, "uniProt", 
	table, "taxonId1", hgtaDoPasteIdentifiers, "submit");
if (page != NULL)
    {
    htmlPageSetVar(page, NULL, hgtaPastedIdentifiers, "8355");
    serialSubmit(&page, org, db, group, track, table, "taxonId2",
    	hgtaDoPastedIdentifiers, "submit");
    if (page != NULL)
        {
	htmlPageSetVar(page, NULL, hgtaOutputType, "selectedFields");
	serialSubmit(&page, org, db, group, track, table, "taxonId3",
	    hgtaDoTopSubmit, "submit");
	if (page != NULL)
	    {
	    htmlPageSetVar(page, NULL, "hgta_fs.check.uniProt.taxon.binomial",
	    	"on");
	    serialSubmit(&page, org, db, group, track, table, "taxonId4",
		hgtaDoPrintSelectedFields, "submit");
	    if (page != NULL)
		{
		if (!stringIn("Xenopus laevis", page->htmlText))
		    {
		    qaStatusSoftError(tablesTestList->status, 
			 "Can't find Xenopus laevis in uniProt.taxon #8355");
		    }
		checkExpectedSimpleRows(page, 1);
		}
	    htmlPageFree(&page);
	    }
	}
    }
verbose(1, "Tested identifier on uniProt.taxon\n");
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
int i;

AllocVar(stats);
for (i=0; i<ntiiTotalCount; ++i)
    statsOnSubsets(list, i, f);
for (test = list; test != NULL; test = test->next)
    qaStatisticsAdd(stats, test->status);
fprintf(f, "\ngrand total\n");
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

void hgTablesTest(char *url, char *logName)
/* hgTablesTest - Test hgTables web page. */
{
/* Get default page, and open log. */
struct htmlPage *rootPage = htmlPageGet(url);
if (appendLog)
    logFile = mustOpen(logName, "a");
else
    logFile = mustOpen(logName, "w");
if (! endsWith(url, "hgTables"))
    warn("Warning: first argument should be a complete URL to hgTables, "
	 "but doesn't look like one (%s)", url);

fprintf(logFile,"seed=%d\n",seed);
 
showRunningHostName();

verbose(1, "Testing URL %s\n", url);
fprintf(logFile, "Testing URL %s\n", url);

/* Show what database server we are connecting to. 
Matters for expected rows in tables. */
showConnectInfo("uniProt");

htmlPageValidateOrAbort(rootPage);

/* Go test what they've specified in command line. */
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
	testOrg(rootPage, mainForm, clOrg);
    else
	{
	struct slName *org;
	int orgIx;
	for (org = orgVar->values, orgIx=0; org != NULL && orgIx < clOrgs; 
		org = org->next, ++orgIx)
	    {
	    testOrg(rootPage, mainForm, org->name);
	    }
	}
    }

/* Do some more complex tests on uniProt. */
testJoining(rootPage);
testFilter(rootPage);
testIdentifier(rootPage);

/* Clean up and report. */
htmlPageFree(&rootPage);
slReverse(&tablesTestList);
reportSummary(tablesTestList, stdout);
reportAll(tablesTestList, logFile);
fprintf(logFile, "---------------------------------------------\n");
reportSummary(tablesTestList, logFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(500000000);
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
seed = optionInt("seed",time(NULL));
      verbose(1,"seed=%d\n",seed);
srand(seed);
clDb = optionVal("db", clDb);
clOrg = optionVal("org", clOrg);
clGroup = optionVal("group", clGroup);
clTrack = optionVal("track", clTrack);
clTable = optionVal("table", clTable);
clDbs = optionInt("dbs", clDbs);
clOrgs = optionInt("orgs", clOrgs);
clGroups = optionInt("groups", clGroups);
clTracks = optionInt("tracks", clTracks);
clTables = optionInt("tables", clTables);
appendLog = optionExists("appendLog");
noShuffle = optionExists("noShuffle");
if (clOrg != NULL)
   clOrgs = BIGNUM;
hgTablesTest(argv[1], argv[2]);
carefulCheckHeap();
return 0;
}
