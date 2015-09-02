/* hubPublicCheck - checks that the labels in hubPublic match what is in the hub labels. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hdb.h"
#include "udc.h"
#include "options.h"
#include "hubConnect.h"
#include "errCatch.h"
#include "trackHub.h"
#include "dystring.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubPublicCheck - checks that the labels in hubPublic match what is in the hub labels\n"
  "   outputs SQL statements to put the table into compliance\n"
  "usage:\n"
  "   hubPublicCheck tableName \n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  "   -addHub=url           - output statments to add url to table\n"
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"addHub", OPTION_STRING},
   {NULL, 0},
};

int hubPublicCheck(char *table)
/* hubPublicCheck - checks that the labels in hubPublic match what is in the hub labels. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
bool hasDescriptionUrl = sqlColumnExists(conn, table, "descriptionUrl");
if (hasDescriptionUrl)
    sqlSafef(query, sizeof(query), "select hubUrl, shortLabel,longLabel,dbList,descriptionUrl from %s", 
	table); 
else
    sqlSafef(query, sizeof(query), "select hubUrl, shortLabel,longLabel,dbList from %s", 
	table); 

struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
int differences = 0;

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *url = row[0], *shortLabel = row[1], *longLabel = row[2], *dbList = row[3], *descriptionUrl = row[4];
    struct errCatch *errCatch = errCatchNew();
    boolean gotWarning = FALSE;
    struct trackHub *tHub = NULL;
    
    if (errCatchStart(errCatch))
	tHub = trackHubOpen(url, "hub_1"); 
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	gotWarning = TRUE;
	warn("%s", errCatch->message->string);
	}
    errCatchFree(&errCatch);

    if (gotWarning)
	{
	continue;
	}

    if (!sameString(shortLabel, tHub->shortLabel))
	{
	differences++;

	printf("update %s set shortLabel=\"%s\" where hubUrl=\"%s\";\n",table, tHub->shortLabel, url);
	}
    if (!sameString(longLabel, tHub->longLabel))
	{
	differences++;

	printf("update %s set longLabel=\"%s\" where hubUrl=\"%s\";\n",table, tHub->longLabel, url);
	}

    struct dyString *dy = newDyString(1024);
    struct trackHubGenome *genome = tHub->genomeList;

    for(; genome; genome = genome->next)
	dyStringPrintf(dy, "%s,", trackHubSkipHubName(genome->name));

    if (!sameString(dy->string, dbList))
	{
	differences++;

	printf("update %s set dbList=\"%s\" where hubUrl=\"%s\";\n",table, dy->string, url);
	}

    if (hasDescriptionUrl && !isEmpty(tHub->descriptionUrl) && ((descriptionUrl == NULL) || !sameString(descriptionUrl, tHub->descriptionUrl)))
	{
	differences++;

	printf("update %s set descriptionUrl=\"%s\" where hubUrl=\"%s\";\n",table, tHub->descriptionUrl, url);
	}

    trackHubClose(&tHub);
    }
return differences;
}

int hubPublicAdd(char *table, char *url)
/* hubPublicAdd -- add url to hubPublic table */
{
struct errCatch *errCatch = errCatchNew();
boolean gotWarning = FALSE;
struct trackHub *tHub = NULL;
int dbCount = 0;

if (errCatchStart(errCatch))
    tHub = trackHubOpen(url, "hub_1"); 
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    gotWarning = TRUE;
    warn("%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

if (gotWarning)
    return 1;

struct hashEl *hel;
struct hashCookie cookie = hashFirst(tHub->genomeHash);
struct dyString *dy = newDyString(1024);

while ((hel = hashNext(&cookie)) != NULL)
    {
    dbCount++;
    dyStringPrintf(dy, "%s,", trackHubSkipHubName(hel->name));
    }

char *descriptionUrl = tHub->descriptionUrl;
if (descriptionUrl == NULL)
    descriptionUrl="";
printf("insert into %s (hubUrl,descriptionUrl,shortLabel,longLabel,registrationTime,dbCount,dbList) values (\"%s\",\"%s\", \"%s\", \"%s\", now(),%d, \"%s\");\n",
    table, url, descriptionUrl, tHub->shortLabel, tHub->longLabel, dbCount, dy->string); 

return 0;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

char *hubUrl = optionVal("addHub", NULL);

if (hubUrl != NULL)
    return hubPublicAdd(argv[1],hubUrl);
else
    return hubPublicCheck(argv[1]);
}
