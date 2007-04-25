/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */

#include "common.h"
#include "options.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "customTrack.h"
#include "customFactory.h"

static char const rcsid[] = "$Id: refreshNamedSessionCustomTracks.c,v 1.4 2007/04/25 17:30:37 angie Exp $";

#define savedSessionTable "namedSessionDb"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "refreshNamedSessionCustomTracks -- scan central database's %s\n"
  "    contents for custom tracks and touch any that are found, to prevent\n"
  "    them from being removed by the custom track cleanup process.\n"
  "usage:\n"
  "    refreshNamedSessionCustomTracks hgcentral[test,beta]\n"
  "This is intended to be run as a nightly cron job for each central db.\n"
  "The ~/.hg.conf file (or $HGDB_CONF) must specify the same central db\n"
  "as the command line.  [The command line arg exists only to suppress this\n"
  "message].\n"
  "\n",
  savedSessionTable
  );
}

/* Options: */
static struct optionSpec options[] = {
   {NULL, 0},
};

void scanSettingsForCT(char *userName, char *sessionName, char *contents,
		       int *pLiveCount, int *pExpiredCount)
/* Parse the CGI-encoded session contents into {var,val} pairs and search
 * for custom tracks.  If found, refresh the custom track.  Parsing code 
 * taken from cartParseOverHash. */
{
char *namePt = contents;
verbose(3, "Scanning %s %s\n", userName, sessionName);
while (isNotEmpty(namePt))
    {
    char *dataPt = strchr(namePt, '=');
    char *nextNamePt;
    if (dataPt == NULL)
	errAbort("Mangled session content string %s", namePt);
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, '&');
    if (nextNamePt != NULL)
         *nextNamePt++ = 0;
    cgiDecode(dataPt, dataPt, strlen(dataPt));
    if (startsWith(CT_FILE_VAR_PREFIX, namePt))
	{
	boolean thisGotLiveCT = FALSE, thisGotExpiredCT = FALSE;
	verbose(3, "Found variable %s = %s\n", namePt, dataPt);
	customFactoryTestExistence(dataPt, &thisGotLiveCT, &thisGotExpiredCT);
	if (thisGotLiveCT && pLiveCount != NULL)
	    (*pLiveCount)++;
	if (thisGotExpiredCT && pExpiredCount != NULL)
	    (*pExpiredCount)++;
	if (thisGotExpiredCT)
	    {
	    if (verboseLevel() >= 3)
		verbose(3, "Found expired custom track in %s %s: %s\n",
			userName, sessionName, dataPt);
	    else
		verbose(2, "Found expired custom track: %s\n", dataPt);
	    }
	}
    namePt = nextNamePt;
    }
}

void refreshNamedSessionCustomTracks(char *centralDbName)
/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */
{
struct sqlConnection *conn = hConnectCentral();
char *actualDbName = sqlGetDatabase(conn);
int liveCount=0, expiredCount=0;

if (!sameString(centralDbName, actualDbName))
    errAbort("Central database specified in hg.conf file is %s but %s "
	     "was specified on the command line.",
	     actualDbName, centralDbName);
else
    verbose(2, "Got connection to %s\n", centralDbName);

if (sqlTableExists(conn, savedSessionTable))
    {
    struct sqlResult *sr = NULL;
    char **row = NULL;
    char query[512];
    safef(query, sizeof(query),
	  "select userName,sessionName,contents from %s", savedSessionTable);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	scanSettingsForCT(row[0], row[1], row[2], &liveCount, &expiredCount);
	}
    sqlFreeResult(&sr);
    }

hDisconnectCentral(&conn);
verbose(1, "Found %d live and %d expired custom tracks in %s.\n",
	liveCount, expiredCount, centralDbName);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
setCurrentDir(CGI_BIN);
refreshNamedSessionCustomTracks(argv[1]);
return 0;
}

