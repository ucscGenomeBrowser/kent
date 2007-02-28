/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */

#include "common.h"
#include "options.h"
#include "hash.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "customTrack.h"
#include "customFactory.h"

static char const rcsid[] = "$Id: refreshNamedSessionCustomTracks.c,v 1.1 2007/02/28 00:28:15 angie Exp $";

#define savedSessionTable "namedSessionDb"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "refreshNamedSessionCustomTracks -- scan central database's %s\n"
  "    contents for custom tracks and touch any that are found, to prevent\n"
  "    them from being removed by the custom track cleanup process.  This is\n"
  "    intended to be run as a nightly cron job on each central db host.\n"
  "usage:\n"
  "    refreshNamedSessionCustomTracks hgcentral[test,beta]\n"
  "\n",
  savedSessionTable
  );
}

/* Options: */
static struct optionSpec options[] = {
   {NULL, 0},
};

void scanSettingsForCT(char *contents)
/* Parse the CGI-encoded session contents into {var,val} pairs and search
 * for custom tracks.  If found, refresh the custom track.  Parsing code 
 * taken from cartParseOverHash. */
{
char *namePt = contents;
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
	customFactoryTestExistence(dataPt, &thisGotLiveCT, &thisGotExpiredCT);
	if (thisGotExpiredCT)
	    warn("Found expired custom track: %s", dataPt);
	}
    namePt = nextNamePt;
    }
}

void refreshNamedSessionCustomTracks(char *centralDbName)
/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */
{
struct sqlConnection *conn = sqlConnect(centralDbName);

if (sqlTableExists(conn, savedSessionTable))
    {
    struct sqlResult *sr = NULL;
    char **row = NULL;
    char query[512];
    safef(query, sizeof(query), "select contents from %s", savedSessionTable);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	scanSettingsForCT(row[0]);
	}
    sqlFreeResult(&sr);
    }

sqlDisconnect(&conn);
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

