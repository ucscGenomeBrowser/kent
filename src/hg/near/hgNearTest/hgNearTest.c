/* hgNearTest - Test hgNear web page. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"
#include "htmlPage.h"
#include "hdb.h"

static char const rcsid[] = "$Id: hgNearTest.c,v 1.1 2004/03/03 10:41:39 kent Exp $";

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

struct htmlForm *htmlFormGet(struct htmlPage *page, char *name)
/* Get named form. */
{
struct htmlForm *form;
for (form = page->forms; form != NULL; form = form->next)
    if (sameWord(form->name, name))
        break;
return form;
}

struct htmlFormVar *htmlFormVarGet(struct htmlForm *form, char *name)
/* Get named variable. */
{
struct htmlFormVar *var;
for (var = form->vars; var != NULL; var = var->next)
    if (sameWord(var->name, name))
	break;
return var;
}

void setVar(struct htmlPage *page, struct htmlForm *form, char *name, char *val)
/* Set variable to given value. */
{
struct htmlFormVar *var;
if (form == NULL)
    form = page->forms;
var = htmlFormVarGet(form, name);
if (var == NULL)
    {
    AllocVar(var);
    var->type = "TEXT";
    var->tagName = "INPUT";
    var->name = name;
    slAddHead(&form->vars, var);
    }
freez(&var->curVal);
var->curVal = cloneString(val);
}

void clearVar(struct htmlPage *page, struct htmlForm *form, char *name)
/* Remove variable of given name. */
{
struct htmlFormVar *var;
if (form == NULL)
    form = page->forms;
var = htmlFormVarGet(form, name);
if (var != NULL)
    slRemoveEl(&form->vars, var);
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


void testCol(struct htmlPage *emptyConfig, char *org, char *db, char *col, char *gene)
/* Test one column. */
{
struct htmlPage *colPage;
char visVar[256];
safef(visVar, sizeof(visVar), "near.col.%s.vis", col);
uglyf("testCol(%s,%s,%s,%s)\n", org, db, col, gene);
setVar(emptyConfig, NULL, visVar, "on");

colPage = htmlPageFromForm(emptyConfig, emptyConfig->forms, "submit", "Submit");
uglyf(" %d chars\n", strlen(colPage->fullText));
htmlPageFree(&colPage);

clearVar(emptyConfig, NULL, visVar);
}

void testDb(struct htmlPage *orgPage, char *org, char *db)
/* Test on one database. */
{
struct hash *genomeRa = hgReadRa(org, db, dataDir, "genome.ra", NULL);
struct htmlPage *emptyConfig, *oneColConfig;
struct slName *colList = NULL, *col;
struct htmlFormVar *var;
char *canonicalTable = hashMustFindVal(genomeRa, "canonicalTable");
struct slName *gene, *geneList = randomSample(db, canonicalTable, "transcript", clRepeat);

uglyf("testDb(%s,%s)\n", org, db);
setVar(orgPage, NULL, "db", db);
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
    setVar(emptyConfig, NULL, "near_search", gene->name);
    for (col = colList; col != NULL; col = col->next)
	{
	testCol(emptyConfig, org, db, col->name, gene->name);
	}
    }
htmlPageFree(&emptyConfig);
hashFree(&genomeRa);
}

void testOrg(struct htmlPage *rootPage, struct htmlForm *rootForm, char *org, char *forceDb)
/* Test on organism.  If forceDb is non-null, only test on
 * given database. */
{
struct htmlPage *orgPage;
struct htmlForm *mainForm;
struct htmlFormVar *dbVar;
struct slName *db;

uglyf("testOrg %s (%s)\n", org, forceDb);
setVar(rootPage, rootForm, "org", org);
setVar(rootPage, rootForm, "db", org);
setVar(rootPage, rootForm, "near_search", "");
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

void hgNearTest(char *url)
/* hgNearTest - Test hgNear web page. */
{
struct htmlPage *rootPage = htmlPageGet(url);
struct htmlForm *mainForm;
struct htmlFormVar *orgVar;
htmlPageValidateOrAbort(rootPage);
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
}

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
