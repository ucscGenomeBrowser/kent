/* hubPublicCheck - checks that the labels in hubPublic match what is in the hub labels. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "hdb.h"
#include "udc.h"
#include "options.h"
#include "hubConnect.h"
#include "errCatch.h"
#include "trackHub.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubPublicCheck - checks that the labels in hubPublic match what is in the hub labels\n"
  "usage:\n"
  "   hubPublicCheck XXX\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

void hubPublicCheck()
/* hubPublicCheck - checks that the labels in hubPublic match what is in the hub labels. */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
safef(query, sizeof(query), "select hubUrl, shortLabel,longLabel,dbList from %s", 
	hubPublicTableName); 
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;

while ((row = sqlNextRow(sr)) != NULL)
    {
    char *url = row[0], *shortLabel = row[1], *longLabel = row[2], 
    	  *dbList = row[3];
    struct errCatch *errCatch = errCatchNew();
    boolean gotWarning = FALSE;
    struct trackHub *tHub = NULL;
    
    if (errCatchStart(errCatch))
	tHub = trackHubOpen(url, "1"); // open hub.. it'll get renamed later
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
	printf("short label: \"%s\" \"%s\"\n",shortLabel, tHub->shortLabel);
	}
    if (!sameString(longLabel, tHub->longLabel))
	{
	printf("long label: \"%s\" \"%s\"\n",longLabel, tHub->longLabel);
	}
    dbList = NULL;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 1)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
hubPublicCheck();
return 0;
}
