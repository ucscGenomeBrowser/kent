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
  "hgTrackDb - Create trackDb table from text files\n"
  "usage:\n"
  "   hgTrackDb database trackDb.sql hgRoot\n"
  );
}

void addVersion(char *database, char *dirName, char *raName, struct hash *uniqHash,
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

void hgTrackDb(char *database, char *sqlFile, char *hgRoot)
/* hgTrackDb - Create trackDb table from text files. */
{
struct hash *uniqHash = newHash(8);
struct hash *htmlHash = newHash(8);
struct trackDb *tdList = NULL, *td;
char dirName[512], raName[512];
char *tab = "trackDb.tab";

/* Create track list from hgRoot and hgRoot/database. */
sprintf(dirName, "%s/%s", hgRoot, database);
sprintf(raName, "%s/%s", dirName, "trackDb.ra");
if (fileExists(raName))
    {
    addVersion(database, dirName, raName, uniqHash, htmlHash, &tdList);
    printf("Loaded %d track descriptions from %s\n", slCount(tdList), dirName);
    }
sprintf(dirName, "%s", hgRoot);
sprintf(raName, "%s/%s", dirName, "trackDb.ra");
addVersion(database, dirName, raName, uniqHash, htmlHash, &tdList);
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
    end = create + strlen(create)-1;
    if (*end == ';') *end = 0;
    sqlRemakeTable(conn, "trackDb", create);

    /* Load in regular fields. */
    sprintf(query, "load data local infile '%s' into table trackDb", tab);
    sqlUpdate(conn, query);

    /* Load in html fields. */
    for (td = tdList; td != NULL; td = td->next)
	{
	char *html = hashFindVal(htmlHash, td->tableName);
        if (html != NULL)
	    updateBigTextField(conn, "trackDb", "tableName", td->tableName, "html", html);
	}

    sqlDisconnect(&conn);
    printf("Loaded database %s\n", database);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
hgTrackDb(argv[1], argv[2], argv[3]);
return 0;
}
