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
#include "hdb.h"

static char const rcsid[] = "$Id: hgNearTest.c,v 1.3 2004/03/03 20:54:21 kent Exp $";

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
  "   hgNearTest url\n"
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
    };

void qaStatusReportOne(struct qaStatus *qs, char *format, ...)
/* Report status */
{
va_list args;
va_start(args, format);
char *errMessage = qs->errMessage;
if (errMessage == NULL)
    errMessage = "ok";
vprintf(format, args);
printf(" %4.3fs %s\n", 0.001*qs->milliTime, errMessage);
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
    }
else
    {
    if (page->status->status != 200)
	{
	dyStringPrintf(errCatch->message, "HTTP status code %d\n", 
		page->status->status);
	errMessage = errCatch->message->string;
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
errCatchEnd(errCatch);
qs = qaStatusOnPage(errCatch, page, startTime, retPage);
errCatchFree(&errCatch);
return qs;
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

struct colTest
/* Test on one column. */
    {
    struct colTest *next;
    char *type;			/* Type of test. */
    struct qaStatus *status;	/* Result of test. */
    char *org;			/* Organism. */
    char *db;			/* Database. */
    char *col;			/* Column. */
    char *gene;			/* Gene. */
    };

struct colTest *colTestNew(char *type, struct qaStatus *status,
	char *org, char *db, char *col, char *gene)
/* Save away column test results. */
{
struct colTest *test;
AllocVar(test);
test->type = type;
test->status = status;
test->org = org;
test->db = db;
test->col = col;
test->gene = gene;
return test;
}

void testCol(struct htmlPage *emptyConfig, char *org, char *db, char *col, char *gene,
	struct colTest **pTestList)
/* Test one column. */
{
struct htmlPage *colPage;
struct colTest *test;
struct qaStatus *status;
char visVar[256];
safef(visVar, sizeof(visVar), "near.col.%s.vis", col);
uglyf("testCol(%s,%s,%s,%s)\n", org, db, col, gene);
htmlSetVar(emptyConfig, NULL, visVar, "on");
htmlSetVar(emptyConfig, NULL, "near.order", "geneDistance");
htmlSetVar(emptyConfig, NULL, "near.count", "25");

status = qaPageFromForm(emptyConfig, emptyConfig->forms, 
	"submit", "Submit", &colPage);
test = colTestNew("colOk", status, org, db, col, gene);
slAddHead(pTestList, test);
if (colPage != NULL && colPage->fullText != NULL)
    uglyf(" %d chars\n", strlen(colPage->fullText));
htmlPageFree(&colPage);
htmlSetVar(emptyConfig, NULL, visVar, NULL);
}

void testDb(struct htmlPage *orgPage, char *org, char *db, struct colTest **pTestList)
/* Test on one database. */
{
struct hash *genomeRa = hgReadRa(org, db, dataDir, "genome.ra", NULL);
struct htmlPage *emptyConfig, *oneColConfig;
struct slName *colList = NULL, *col;
struct htmlFormVar *var;
char *canonicalTable = hashMustFindVal(genomeRa, "canonicalTable");
struct slName *gene, *geneList = randomSample(db, canonicalTable, "transcript", clRepeat);

uglyf("testDb(%s,%s)\n", org, db);
htmlSetVar(orgPage, NULL, "db", db);
emptyConfig = htmlPageFromForm(orgPage, orgPage->forms, "near.do.colHideAll", "on");
if (emptyConfig == NULL)
    errAbort("Couldn't get empty config for %s\n", db);
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
    htmlSetVar(emptyConfig, NULL, "near_search", gene->name);
    for (col = colList; col != NULL; col = col->next)
	{
	testCol(emptyConfig, org, db, col->name, gene->name, pTestList);
	}
    }
htmlPageFree(&emptyConfig);
hashFree(&genomeRa);
}

void testOrg(struct htmlPage *rootPage, struct htmlForm *rootForm, char *org, char *forceDb,
	struct colTest **pTestList)
/* Test on organism.  If forceDb is non-null, only test on
 * given database. */
{
struct htmlPage *orgPage;
struct htmlForm *mainForm;
struct htmlFormVar *dbVar;
struct slName *db;

uglyf("testOrg %s (%s)\n", org, forceDb);
htmlSetVar(rootPage, rootForm, "org", org);
htmlSetVar(rootPage, rootForm, "db", org);
htmlSetVar(rootPage, rootForm, "near_search", "");
orgPage = htmlPageFromForm(rootPage, rootPage->forms, "submit", "Go");
if ((mainForm = htmlFormGet(orgPage, "mainForm")) == NULL)
    errAbort("Couldn't get main form on orgPage");
if ((dbVar = htmlFormVarGet(mainForm, "db")) == NULL)
    errAbort("Couldn't get org var");
for (db = dbVar->values; db != NULL; db = db->next)
    {
    if (forceDb == NULL || sameString(forceDb, db->name))
	testDb(orgPage, org, db->name, pTestList);
    }
htmlPageFree(&orgPage);
}

void hgNearTest(char *url)
/* hgNearTest - Test hgNear web page. */
{
struct htmlPage *rootPage = htmlPageGet(url);
struct htmlForm *mainForm;
struct htmlFormVar *orgVar;
struct colTest *testList = NULL;
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
	testOrg(rootPage, mainForm, org->name, clDb, &testList);
	}
    }
htmlPageFree(&rootPage);
uglyf("Executed %d test\n", slCount(testList));
}

#ifdef TEST 
void hgNearTest(char *url)
{
struct htmlPage *page;
struct qaStatus *qs;
qs = qaPageGet(url, &page);
qaStatusReportOne(qs, url);
htmlPageFree(&page);
}
#endif /* TEST */

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(200000000);
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clDb = optionVal("db", clDb);
clOrg = optionVal("org", clOrg);
clSearch = optionVal("search", clSearch);
dataDir = optionVal("dataDir", dataDir);
clRepeat = optionInt("repeat", clRepeat);
hgNearTest(argv[1]);
carefulCheckHeap();
return 0;
}
