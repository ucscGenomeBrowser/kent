/* hgFindSpec - Create trackDb table from text files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlList.h"
#include "jksql.h"
#include "trackDb.h"
#include "hgFindSpec.h"
#include "hdb.h"
#include "obscure.h"
#include "portable.h"
#include "dystring.h"
#include "verbose.h"

static char const rcsid[] = "$Id: hgFindSpec.c,v 1.7.24.1 2008/07/31 02:24:35 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgFindSpec - Create hgFindSpec table from trackDb.ra files.\n\n"
  "usage:\n"
  "   hgFindSpec [options] orgDir database hgFindSpec hgFindSpec.sql hgRoot\n"
  "\n"
  "Options:\n"
  "   -strict		Add spec to hgFindSpec only if its table(s) exist.\n"
  "   -local - connect to local host, instead of default host, using localDb.XXX variables defined in .hg.conf.\n"
  "   -raName=trackDb.ra - Specify a file name to use other than trackDb.ra\n"
  "   for the ra files.\n" 
  );
}

static struct optionSpec optionSpecs[] = {
    {"raName", OPTION_STRING},
    {"strict", OPTION_BOOLEAN},
	{"local", OPTION_BOOLEAN},
};

static char *raName = "trackDb.ra";
boolean localDb=FALSE;              /* Connect to local host, instead of default host, using localDb.XXX variables defined in .hg.conf.\n"*/ 

void addVersion(boolean strict, char *database, char *dirName, char *raName, 
    struct hash *uniqHash,
    struct hash *htmlHash,
    struct hgFindSpec **pSpecList)
/* Read in specs from raName and add them to list/database if new. */
{
struct hgFindSpec *hfsList = NULL, *hfs = NULL;
struct hgFindSpec *hfsNext = NULL;

hfsList = hgFindSpecFromRa(database, raName);
if (strict) 
    {
    for (hfs = hfsList; hfs != NULL; hfs = hfsNext)
        {
	hfsNext = hfs->next;
        if (! hTableOrSplitExists(database, hfs->searchTable))
            {
	    if (verboseLevel() > 1)
		printf("%s missing\n", hfs->searchTable);
            slRemoveEl(&hfsList, hfs);
            }
	else if (hfs->xrefTable[0] != 0 &&
                 ! hTableOrSplitExists(database, hfs->xrefTable))
	    {
	    if (verboseLevel() > 1)
		printf("%s (xref) missing\n", hfs->xrefTable);
            slRemoveEl(&hfsList, hfs);
	    }
        }
    }

for (hfs = hfsList; hfs != NULL; hfs = hfsNext)
    {
    hfsNext = hfs->next;
    if (! hashLookup(uniqHash, hfs->searchName))
        {
	hashAdd(uniqHash, hfs->searchName, hfs);
	slAddHead(pSpecList, hfs);
	}
    }
}

void dyStringQuoteString(struct dyString *dy, char quotChar, char *text)
/* Stringize text onto end of dy. */
{
char c;

dyStringAppendC(dy, quotChar);
while ((c = *text++) != 0)
    {
    if (c == quotChar)
        dyStringAppendC(dy, '\\');
    dyStringAppendC(dy, c);
    }
dyStringAppendC(dy, quotChar);
}

void updateBigTextField(struct sqlConnection *conn, char *table,
     char *whereField, char *whereVal,	
     char *textField, char *textVal)
/* Generate sql code to update a big text field that may include
 * newlines and stuff. */
{
struct dyString *dy = newDyString(4096);
dyStringPrintf(dy, "update %s set %s=", table, textField);
dyStringQuoteString(dy, '"', textVal);
dyStringPrintf(dy, " where %s = '%s'", whereField, whereVal);
sqlUpdate(conn, dy->string);
freeDyString(&dy);
}

char *subTrackName(char *create, char *tableName)
/* Substitute tableName for whatever is between CREATE TABLE  and  first '(' 
   freez()'s create passed in. */
{
char newCreate[strlen(create) + strlen(tableName) + 10];
char *front = NULL;
char *rear = NULL;
int length = (strlen(create) + strlen(tableName) + 10);
/* try to find "TABLE" or "table" in create */
front = strstr(create, "TABLE");
if(front == NULL)
    front = strstr(create, "table");
if(front == NULL)
    errAbort("hgFindSpec::subTrackName() - Can't find 'TABLE' in %s", create);

/* try to find first "(" in string */
rear = strstr(create, "(");
if(rear == NULL)
    errAbort("hgFindSpec::subTrackName() - Can't find '(' in %s", create);

/* set to NULL character after "TABLE" */
front += 5;
*front = '\0';

snprintf(newCreate, length , "%s %s %s", create, tableName, rear);
return cloneString(newCreate);
}


void layerOn(boolean strict, char *database, char *dir, struct hash *uniqHash, 
	struct hash *htmlHash,  boolean mustExist, struct hgFindSpec **hfsList)
/* Read search specs from trackDb.ra from directory,
 * and layer them on top of whatever is in hfsList. */
{
char raFile[512];
if (raName[0] != '/') 
    safef(raFile, sizeof(raFile), "%s/%s", dir, raName);
else
    safef(raFile, sizeof(raFile), "%s", raName);
if (fileExists(raFile))
    {
    addVersion(strict, database, dir, raFile, uniqHash, htmlHash, hfsList);
    }
else 
    {
    if (mustExist)
        errAbort("%s doesn't exist!", raFile);
    }
}


static char *settingsFromHash(struct hash *hash)
/* Create settings string from settings hash. */
{
if (hash == NULL)
    return cloneString("");
else
    {
    struct dyString *dy = dyStringNew(1024);
    char *ret;
    struct hashEl *el, *list = hashElListHash(hash);
    slSort(&list, hashElCmp);
    for (el = list; el != NULL; el = el->next)
	dyStringPrintf(dy, "%s %s\n", el->name, (char *)el->val);
    slFreeList(&list);
    ret = cloneString(dy->string);
    dyStringFree(&dy);
    return ret;
    }
}

void hgFindSpec(char *org, char *database, char *hgFindSpecName, char *sqlFile,
		char *hgRoot, boolean strict)
/* hgFindSpec - Create hgFindSpec table from text files. */
{
struct hash *uniqHash = newHash(8);
struct hash *htmlHash = newHash(8);
struct hgFindSpec *hfsList = NULL, *hfs;
char rootDir[512], orgDir[512], asmDir[512];
char tab[512];
snprintf(tab, sizeof(tab), "%s.tab", hgFindSpecName);

/* Create track list from hgRoot and hgRoot/org and hgRoot/org/assembly 
 * ra format database. */
sprintf(rootDir, "%s", hgRoot);
sprintf(orgDir, "%s/%s", hgRoot, org);
sprintf(asmDir, "%s/%s/%s", hgRoot, org, database);
layerOn(strict, database, asmDir, uniqHash, htmlHash, FALSE, &hfsList);
layerOn(strict, database, orgDir, uniqHash, htmlHash, FALSE, &hfsList);
layerOn(strict, database, rootDir, uniqHash, htmlHash, TRUE, &hfsList);
slSort(&hfsList, hgFindSpecCmp);
if (verboseLevel() > 0)
    printf("Loaded %d search specs total\n", slCount(hfsList));

/* Write to tab-separated file. */
    {
    FILE *f = mustOpen(tab, "w");
    for (hfs = hfsList; hfs != NULL; hfs = hfs->next)
	hgFindSpecTabOut(hfs, f);
    carefulClose(&f);
    }

/* Update database */
    {
    char *create, *end;
    char query[256];
    struct sqlConnection *conn = NULL;
    if (!localDb)
	conn = sqlConnect(database);
    else
	conn = hConnectLocalDb(database);

    /* Load in table definition. */
    readInGulp(sqlFile, &create, NULL);
    create = trimSpaces(create);
    create = subTrackName(create, hgFindSpecName);
    end = create + strlen(create)-1;
    if (*end == ';') *end = 0;
    sqlRemakeTable(conn, hgFindSpecName, create);

    /* Load in regular fields. */
    sprintf(query, "load data local infile '%s' into table %s", tab,
	    hgFindSpecName);
    sqlUpdate(conn, query);

    /* Load in settings fields. */
    for (hfs = hfsList; hfs != NULL; hfs = hfs->next)
	{
	if (hfs->settingsHash != NULL)
	    {
	    char *settings = settingsFromHash(hfs->settingsHash);
	    updateBigTextField(conn, hgFindSpecName, "searchName",
			       hfs->searchName, 
			       "searchSettings", settings);
	    freeMem(settings);
	    }
	}

    sqlDisconnect(&conn);
    if (verboseLevel() > 0)
	printf("Loaded database %s\n", database);
    }
}

void adjustTrackDbName(char *hgFindSpecName)
/* Some hgFindSpec info is pulled from the trackDb table.  When the 
 * hgFindSpec name is hgFindSpec_$USER and the user's ~/.hg.conf file 
 * specifies trackDb_$USER, that works fine.  However, when the 
 * hgFindSpec name is just hgFindSpec (as for make alpha / make strict 
 * invocations), but ~/hg.conf says trackDb_$USER, they're inconsistent.
 * So to make a long story short -- circumvent the ~/.hg.conf!  */
{
if (sameString(hgFindSpecName, "hgFindSpec"))
    hSetTrackDbName("trackDb");
else if (startsWith(hgFindSpecName, "hgFindSpec_"))
    {
    char trackDbName[256];
    safef(trackDbName, sizeof(trackDbName), "trackDb_%s",
	  hgFindSpecName + strlen("hgFindSpec_"));
    hSetTrackDbName(trackDbName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 6)
    usage();
raName = optionVal("raName", raName);
localDb = optionExists("local");
adjustTrackDbName(argv[3]);
hgFindSpec(argv[1], argv[2], argv[3], argv[4], argv[5],
           optionExists("strict"));
return 0;
}
