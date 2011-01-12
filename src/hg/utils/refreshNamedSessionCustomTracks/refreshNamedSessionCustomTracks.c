/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */

#include "common.h"
#include "options.h"
#include "hash.h"
#include "cheapcgi.h"
#include "customTrack.h"
#include "customFactory.h"
#include "hui.h"
#include "hgConfig.h"
#include <sys/wait.h>
#include "errCatch.h"

static char const rcsid[] = "$Id: refreshNamedSessionCustomTracks.c,v 1.11 2010/01/13 17:27:35 angie Exp $";

#define savedSessionTable "namedSessionDb"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "refreshNamedSessionCustomTracks -- scan central database table: '%s'\n"
  "    contents for custom tracks and touch any that are found, to prevent\n"
  "    them from being removed by the custom track cleanup process.\n"
  "usage:\n"
  "    refreshNamedSessionCustomTracks hgcentral[test,beta] [-workDir=/path]\n"
  "options:\n"
  "    -workDir=%s - a directory to work from where\n"
  "                                       - ../trash can be found\n"
  "             default will be %s\n"
  "             which implies ../trash is: /usr/local/apache/trash\n"
  "    -atime=N - If the session has not been accessed since N days ago,\n"
  "             - don't refresh its custom tracks.  Default: no limit.\n"
  "This is intended to be run as a nightly cron job for each central db.\n"
  "The ~/.hg.conf file (or $HGDB_CONF) must specify the same central db\n"
  "as the command line.  [The command line arg helps to verify coordination.]",
  savedSessionTable, CGI_BIN, CGI_BIN
  );
}

/* Options: */
static struct optionSpec options[] = {
    {"atime",    OPTION_INT},
    {"workDir",  OPTION_STRING},
    {"hardcore", OPTION_BOOLEAN}, /* Intentionally omitted from usage(). */
    {NULL, 0},
};

int CFTEcalls = 0;  // DEBUG REMOVE
int numUpdates = 0;  // DEBUG REMOVE

struct sqlConnection *unCachedCentralConn()
/* do not want a cached connection because we will close and fork */
{
return sqlConnectRemote( 
 cfgOption("central.host"),
 cfgOption("central.user"    ),
 cfgOption("central.password"),
 cfgOption("central.db")
);
}


// due to bug in OS, won't work without a handler
static void handle_SIGCHLD(int sig)
{
}


void customFactoryTestExistenceCall(char *genomeDb, char *fileName, boolean *retGotLive,
				boolean *retGotExpired)
/* Fork and run it to avoid gigabytes of leaked memory */
{

sigset_t mask;
sigset_t orig_mask;  
struct timespec timeout;

// due to bug we have to set up a sigchld handler
// even though we don't use it.
struct sigaction act;
(void)memset (&act, 0, sizeof(act));
act.sa_handler = handle_SIGCHLD;
if (sigaction(SIGCHLD, &act, 0)) {
    perror("sigaction err");
    return;
}

sigemptyset (&mask);
sigaddset (&mask, SIGCHLD);

/* BLOCK to prevent a race condition that loses SIGCHLD events */
if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) 
    {
    perror ("sigprocmask");
    return;
    }

/* This is critical because we are about to fork, 
 * otherwise your output is a mess with weird duplicates */
fflush(stdout); fflush(stderr);


pid_t pid = 0;
pid = fork();
if (pid < 0)
    errnoAbort("refreshNamedSessionCustomTracks can't fork");
if (pid == 0)
    {

    /* Put some error catching in so it won't just abort
     *  and also we don't want to get thrown out to any higher-level catcher */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
        if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0)  // unblock SIGCHLD in child.
	    {
	    perror("sigprocmask SIG_SETMASK to unblock child SIGCHLD");
	    }
	customFactoryTestExistence(genomeDb, fileName, retGotLive, retGotExpired);
	int retCode = 0;
	if (*retGotLive)
	    retCode |= 1;
	if (*retGotExpired)
	    retCode |= 2;

	exit(retCode);
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	verbose(1, "%s", errCatch->message->string);
	}
    errCatchFree(&errCatch);
    exit(4);   

    }
else
    {
    int wstat;

    timeout.tv_sec = 10;
    timeout.tv_nsec = 0;

    while (1)
	{
	int sig = sigtimedwait(&mask, NULL, &timeout);
	int savedErrno = errno;

	if (sig < 0) 
	    {
	    if (savedErrno == EINTR) 
		{
		/* Interrupted by a signal other than SIGCHLD. */
                /* An minor improvement would be to subtract the time already consumed before continuing. */
                verbose(1, "EINTR received, ignoring");
		fflush(stdout); fflush(stderr);
		continue;
		}
	    else if (savedErrno == EAGAIN) 
		{
		verbose(1,"Timed out, killing child pid %d\n", pid);
		fflush(stdout); fflush(stderr);
		kill (pid, SIGKILL);
		//continue;  /* to catch the resulting SIGCHLD ?*/
		}
	    else 
		{
		perror ("sigtimedwait");
		fflush(stdout); fflush(stderr);
		return;
		}
	    }

	break;  /* received SIGCHLD */
	}

    if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0)  // unblock SIGCHLD
	{
	perror("sigprocmask SIG_SETMASK to unblock child SIGCHLD");
	}
    if (waitpid(pid, &wstat, 0) < 0)
	{
	perror("waitpid failed");
	fflush(stdout); fflush(stderr);
	return;
	}

    int retCode = WEXITSTATUS(wstat);

    if (retCode == 4)
	return;
    if (retCode & 1)
	*retGotLive = TRUE;
    if (retCode & 2)
	*retGotExpired = TRUE;

    }

}


void scanSettingsForCT(char *userName, char *sessionName, // char *contents,
			int *pLiveCount, int *pExpiredCount)
/* Parse the CGI-encoded session contents into {var,val} pairs and search
 * for custom tracks.  If found, refresh the custom track.  Parsing code 
 * taken from cartParseOverHash. 
 * If any nonexistent custom track files are found, return a SQL update
 * command that will remove those from this session.  We can't just do 
 * the update here because that messes up the caller's query. */
{

struct sqlConnection *conn = unCachedCentralConn();

char query[512];

safef(query, sizeof(query),
	  "select contents from %s "
	  "where userName='%s' and sessionName = '%s'", savedSessionTable, userName, sessionName);
char *contents = sqlQuickString(conn, query);
sqlDisconnect(&conn);
if (!contents)
    return;

int contentLength = strlen(contents);
struct dyString *newContents = dyStringNew(contentLength+1);
struct dyString *oneSetting = dyStringNew(contentLength / 4);
char *contentsToChop = cloneString(contents);
char *namePt = contentsToChop;


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
    dyStringClear(oneSetting);
    dyStringPrintf(oneSetting, "%s=%s%s",
		   namePt, dataPt, (nextNamePt ? "&" : ""));
    if (startsWith(CT_FILE_VAR_PREFIX, namePt))
	{
	boolean thisGotLiveCT = FALSE, thisGotExpiredCT = FALSE;
	cgiDecode(dataPt, dataPt, strlen(dataPt));
	verbose(3, "Found variable %s = %s\n", namePt, dataPt);
	/* If the file does not exist, omit this setting from newContents so 
	 * it doesn't get copied from session to session.  If it does exist,
	 * leave it up to customFactoryTestExistence to parse the file for 
	 * possible customTrash table references, some of which may exist 
	 * and some not. */
	if (! fileExists(dataPt))
	    {
	    verbose(3, "Removing %s from %s %s\n", oneSetting->string,
		    userName, sessionName);
	    thisGotExpiredCT = TRUE;
	    }
	else
	    {
	    dyStringAppend(newContents, oneSetting->string);
	    char *db = namePt + strlen(CT_FILE_VAR_PREFIX);

	    customFactoryTestExistenceCall(db, dataPt, &thisGotLiveCT, &thisGotExpiredCT);

            //verbose(1,"called CFTE, got live=%d expired=%d\n", thisGotLiveCT, thisGotExpiredCT);  // DEBUG REMOVE
	    ++CFTEcalls;  // DEBUG REMOVE
	    }
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
	if (thisGotLiveCT)
	    verbose(4, "Found live custom track: %s\n", dataPt);
	}
    else
	dyStringAppend(newContents, oneSetting->string);
    namePt = nextNamePt;
    }
if (newContents->stringSize != contentLength) 
    ++numUpdates;
if (optionExists("hardcore") && newContents->stringSize != contentLength)  // almost never used
    {
    struct sqlConnection *conn = unCachedCentralConn();
    struct dyString *update = dyStringNew(contentLength*2);
    if (newContents->stringSize > contentLength)
	errAbort("Uh, why is newContents (%d) longer than original (%d)??",
		 newContents->stringSize, contentLength);
    dyStringPrintf(update, "UPDATE %s set contents='", savedSessionTable);
    dyStringAppendN(update, newContents->string, newContents->stringSize);
    dyStringPrintf(update, "', lastUse=now(), useCount=useCount+1 "
		   "where userName=\"%s\" and sessionName=\"%s\";",
		   userName, sessionName);
    verbose(3, "Removing one or more dead CT file settings from %s %s "
	    "(original length %d, now %d)\n", 
	    userName, sessionName,
	    contentLength, newContents->stringSize);
    sqlUpdate(conn, update->string);
    dyStringFree(&update);
    sqlDisconnect(&conn);
    }
dyStringFree(&oneSetting);
dyStringFree(&newContents);
freeMem(contentsToChop);
freeMem(contents);
return;

}

struct sessionInfo
    {
    struct sessionInfo *next;
    char userName[256];
    char sessionName[256];
    char *contents;  // keep or remove?
    };

void refreshNamedSessionCustomTracks(char *centralDbName)
/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */
{

struct sqlConnection *conn = unCachedCentralConn();
char *actualDbName = sqlGetDatabase(conn);
int liveCount=0, expiredCount=0;

setUdcCacheDir();  /* programs that use udc must call this to initialize cache dir location */

if (!sameString(centralDbName, actualDbName))
    errAbort("Central database specified in hg.conf file is %s but %s "
	     "was specified on the command line.",
	     actualDbName, centralDbName);
else
    verbose(2, "Got connection to %s\n", centralDbName);

long long threshold = 0;
int atime = optionInt("atime", 0);
if (atime > 0)
    {
    time_t now = time(NULL);
    threshold = now - ((long long)atime * 24 * 60 * 60);
    }

struct sessionInfo *sessionList = NULL, *si;
if (sqlTableExists(conn, savedSessionTable))
    {
    struct sqlResult *sr = NULL;
    char **row = NULL;
    char query[512];
    safef(query, sizeof(query),
	  "select userName,sessionName,UNIX_TIMESTAMP(lastUse) from %s "
	  "order by userName,sessionName", savedSessionTable);
    sr = sqlGetResult(conn, query);
    // Slurp results into memory instead of processing row by row,
    // reducing the chance of lost connection.
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (atime > 0)
	    {
	    long long lastUse = atoll(row[2]);
	    if (lastUse < threshold)
		{
		verbose(2, "User %s session %s is older than %d days, skipping.\n",
			row[0], row[1], atime);
		continue;
		}
	    }
	AllocVar(si);
	safecpy(si->userName, sizeof(si->userName), row[0]);
	safecpy(si->sessionName, sizeof(si->sessionName), row[1]);
	slAddHead(&sessionList, si);
	}
    sqlFreeResult(&sr);
    }

sqlDisconnect(&conn);

for (si = sessionList;  si != NULL;  si = si->next)
    {
    /* put some error catching in so it won't just abort */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	scanSettingsForCT(si->userName, si->sessionName, &liveCount, &expiredCount);
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	warn("sessionList errCatch: %s", errCatch->message->string);
    errCatchFree(&errCatch);
    }

//DEBUG REMOVE
verbose(1, "# of updates found: %d\n", numUpdates);
verbose(1, "# of CustomFactoryTextExistence calls done: %d\n", CFTEcalls);

verbose(1, "Found %d live and %d expired custom tracks in %s.\n",
	liveCount, expiredCount, centralDbName);
}


int main(int argc, char *argv[])
/* Process command line. */
{

optionInit(&argc, argv, options);
if (argc != 2)
    usage();
char *workDir = optionVal("workDir", CGI_BIN);
setCurrentDir(workDir);

refreshNamedSessionCustomTracks(argv[1]);

// DEBUG check out ram usage
pid_t pid = getpid();
char temp[256];
safef(temp, sizeof(temp), "grep VmPeak /proc/%d/status", (int) pid);
system(temp);

return 0;
}

