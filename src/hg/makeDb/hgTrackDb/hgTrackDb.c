/* hgTrackDb - Create trackDb table from text files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "trackDb.h"
#include "sqlList.h"
#include "hdb.h"
#include "obscure.h"
#include "jksql.h"
#include "portable.h"
#include "dystring.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTrackDb - Create trackDb table from text files.\n\n"
  "Note that the browser supports multiple trackDb tables, usually\n"
  "in the form: trackDb_YourUserName. Which particular trackDb\n"
  "table the browser uses is specified in the hg.conf file found\n"
  "either in your home directory as '.hg.conf' or in the web \n"
  "server's cgi-bin directory as 'hg.conf'.\n"
  "usage:\n"
  "   hgTrackDb database trackDb_$(USER) trackDb.sql hgRoot\n"
  );
}

void addVersion(char *dirName, char *raName, 
    struct hash *uniqHash,
    struct hash *htmlHash,
    struct trackDb **pTrackList)
/* Read in tracks from raName and add them to list/database if new. */
{
struct trackDb *tdList = NULL, *td, *tdNext;
char fileName[512];

tdList = trackDbFromRa(raName);
for (td = tdList; td != NULL; td = tdNext)
    {
    tdNext = td->next;
    if (!hashLookup(uniqHash, td->tableName))
        {
	hashAdd(uniqHash, td->tableName, td);
	slAddHead(pTrackList, td);
	sprintf(fileName, "%s/%s.html", dirName, td->tableName);
	if (fileExists(fileName))
	    {
	    char *s;
	    readInGulp(fileName, &s, NULL);
	    hashAdd(htmlHash, td->tableName, s);
	    }
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
    errAbort("hgTrackDb::subTrackName() - Can't find 'TABLE' in %s", create);

/* try to find first "(" in string */
rear = strstr(create, "(");
if(rear == NULL)
    errAbort("hgTrackDb::subTrackName() - Can't find '(' in %s", create);

/* set to NULL character after "TABLE" */
front += 5;
*front = '\0';

snprintf(newCreate, length , "%s %s %s", create, tableName, rear);
return cloneString(newCreate);
}


void layerOn(char *dir, struct hash *uniqHash, 
	struct hash *htmlHash,  boolean mustExist, struct trackDb **tdList)
/* Read trackDb.ra from directory and any associated .html files,
 * and layer them on top of whatever is in tdList. */
{
char raFile[512];
sprintf(raFile, "%s/trackDb.ra", dir);
if (fileExists(raFile))
    {
    addVersion(dir, raFile, uniqHash, htmlHash, tdList);
    }
else 
    {
    if (mustExist)
        errAbort("%s doesn't exist!", raFile);
    }
}

void hgTrackDb(char *org, char *database, char *trackDbName, char *sqlFile, char *hgRoot)
/* hgTrackDb - Create trackDb table from text files. */
{
struct hash *uniqHash = newHash(8);
struct hash *htmlHash = newHash(8);
struct trackDb *tdList = NULL, *td;
char rootDir[512], orgDir[512], asmDir[512];
char tab[512];
snprintf(tab, sizeof(tab), "%s.tab", trackDbName);

/* Create track list from hgRoot and hgRoot/org and hgRoot/org/assembly 
 * ra format database. */
sprintf(rootDir, "%s", hgRoot);
sprintf(orgDir, "%s/%s", hgRoot, org);
sprintf(asmDir, "%s/%s/%s", hgRoot, org, database);
layerOn(asmDir, uniqHash, htmlHash, FALSE, &tdList);
layerOn(orgDir, uniqHash, htmlHash, FALSE, &tdList);
layerOn(rootDir,uniqHash, htmlHash, TRUE, &tdList);
slSort(&tdList, trackDbCmp);
printf("Loaded %d track descriptions total\n", slCount(tdList));

/* Write to tab-separated file. */
    {
    FILE *f = mustOpen(tab, "w");
    for (td = tdList; td != NULL; td = td->next)
	trackDbTabOut(td, f);
    carefulClose(&f);
    }

/* Update database */
    {
    char *create, *end;
    char query[256];
    struct sqlConnection *conn = sqlConnect(database);

    /* Load in table definition. */
    readInGulp(sqlFile, &create, NULL);
    create = trimSpaces(create);
    create = subTrackName(create, trackDbName);
    end = create + strlen(create)-1;
    if (*end == ';') *end = 0;
    sqlRemakeTable(conn, trackDbName, create);

    /* Load in regular fields. */
    sprintf(query, "load data local infile '%s' into table %s", tab, trackDbName);
    sqlUpdate(conn, query);

    /* Load in html fields. */
    for (td = tdList; td != NULL; td = td->next)
	{
	char *html = hashFindVal(htmlHash, td->tableName);
        if (html != NULL)
	    updateBigTextField(conn,  trackDbName, "tableName", td->tableName, "html", html);
	}

    sqlDisconnect(&conn);
    printf("Loaded database %s\n", database);
    }

remove(tab);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 6)
    usage();
hgTrackDb(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
