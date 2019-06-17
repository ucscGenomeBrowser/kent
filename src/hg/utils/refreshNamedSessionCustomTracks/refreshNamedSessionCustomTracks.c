/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "options.h"
#include "hash.h"
#include "cheapcgi.h"
#include "customTrack.h"
#include "customFactory.h"
#include "hui.h"
#include "hgConfig.h"
#include <sys/wait.h>
#include <signal.h>
#include "obscure.h"

int version = 44;  // PLEASE INCREMENT THIS BEFORE PUSHING TO SHARED REPO
                   // SO THAT OTHERS MAY TEST WITH IT, SO THAT EVERYONE KNOWS THEY HAVE THE
                   // EXACT RIGHT VERSION.

#define savedSessionTable "namedSessionDb"

int CFTEcalls = 0; 
int numUpdates = 0;

int numForks = 10;

int timeoutSecs = 7200; // Timeout for each forked child process
                        // default 7200 seconds is two hours

char *testFailure = NULL;

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
  "    -forks=N - Number of times to fork to recover memory.  Default: %d\n"
  "    -timeoutSecs=N - Number of seconds to kill a timed-out forked child.  Default: %d\n"
  "    -testFailure={exitCode|errAbort|segfault|timeout} must cause the parent to return non-zero exitCode\n"
  "This is intended to be run as a nightly cron job for each central db.\n"
  "The ~/.hg.conf file (or $HGDB_CONF) must specify the same central db\n"
  "as the command line.  [The command line arg helps to verify coordination.]",
  savedSessionTable, CGI_BIN, CGI_BIN, numForks, timeoutSecs
  );
}

/* Options: */
static struct optionSpec options[] = {
    {"atime",    OPTION_INT},
    {"workDir",  OPTION_STRING},
    {"forks",  OPTION_INT},
    {"timeoutSecs", OPTION_INT},
    {"testFailure",  OPTION_STRING},
    {"hardcore", OPTION_BOOLEAN}, /* Intentionally omitted from usage(). */
    {NULL, 0},
};

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

void showVmPeak()
/* show peak mem usage */
{
pid_t pid = getpid();
char temp[256];
printf("# pid=%d: ",pid); fflush(stdout);
safef(temp, sizeof(temp), "grep VmPeak /proc/%d/status", (int) pid);
if (system(temp) != 0)
    errAbort("system(\"%s\") failed", temp);
fflush(stdout);
}

// due to bug in OS, won't work without a handler
static void handle_SIGCHLD(int sig)
{
}

sigset_t mask;
sigset_t orig_mask;  

pid_t forkIt()
/* block sigchld and fork */
{

// due to bug in OS we have to set up a sigchld handler
// even though we don't use it.
struct sigaction act;
(void)memset (&act, 0, sizeof(act));
act.sa_handler = handle_SIGCHLD;
if (sigaction(SIGCHLD, &act, 0)) 
    {
    errnoAbort("sigaction err");
    }

sigemptyset (&mask);
sigaddset (&mask, SIGCHLD);

/* BLOCK to prevent a race condition that loses SIGCHLD events */
if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) 
    {
    errnoAbort("sigprocmask");
    }

/* This is critical because we are about to fork, 
 * otherwise your output is a mess with weird duplicates */
fflush(stdout); fflush(stderr);

pid_t pid = fork();
if (pid < 0)
    errnoAbort("refreshNamedSessionCustomTracks can't fork");
if (pid == 0)  // child
    {
    if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0)  // unblock SIGCHLD
	{
	errnoAbort("sigprocmask SIG_SETMASK to unblock child SIGCHLD");
	}
    }

return pid;
}

void waitForChildWithTimeout(pid_t pid)
/* wait for child with timeout. exit non-zero if child had error. */
{

int wstat;
struct timespec timeout;

timeout.tv_sec = timeoutSecs;   // default 3600 is one hour
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
	    verbose(1, "EINTR received, ignoring.\n");
	    fflush(stdout); fflush(stderr);
	    continue;
	    }
	else if (savedErrno == EAGAIN) 
	    {
	    verbose(1,"Timed out, killing child pid %d\n", pid);
	    fflush(stdout); fflush(stderr);
	    kill (pid, SIGKILL);
	    }
	else 
	    {
	    errnoAbort ("sigtimedwait");
	    }
	}

    break;  /* received SIGCHLD */
    }

if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0)  // unblock SIGCHLD
    {
    errnoAbort("sigprocmask SIG_SETMASK to unblock SIGCHLD");
    }
if (waitpid(pid, &wstat, 0) < 0)
    {
    errnoAbort("waitpid failed");
    }
else
    {
    if (WIFEXITED(wstat))
	{
	if (WEXITSTATUS(wstat) > 0)
	    {
	    verbose(1,"waitpid child had a error exit status %d. Exiting.\n", WEXITSTATUS(wstat));
	    exit(1);	
	    }
	}
    else if (WIFSIGNALED(wstat))
	{
	verbose(1,"waitpid child had a signal. Exiting.\n");
	exit(1);	
	}
    }

}


void scanSettingsForCT(char *userName, char *sessionName,
			int *pLiveCount, int *pExpiredCount, struct sqlConnection *conn)
/* Parse the CGI-encoded session contents into {var,val} pairs and search
 * for custom tracks.  If found, refresh the custom track.  Parsing code 
 * taken from cartParseOverHash. 
 * If any nonexistent custom track files are found, return a SQL update
 * command that will remove those from this session.  We can't just do 
 * the update here because that messes up the caller's query. */
{

char query[512];

sqlSafef(query, sizeof(query),
	  "select contents from %s "
	  "where userName='%s' and sessionName = '%s'", savedSessionTable, userName, sessionName);
char *contents = sqlQuickString(conn, query);
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
	errAbort("ERROR: Mangled session content string %s", namePt);
    *dataPt++ = 0;
    nextNamePt = strchr(dataPt, '&');
    if (nextNamePt != NULL)
	*nextNamePt++ = 0;
    dyStringClear(oneSetting);
    dyStringPrintf(oneSetting, "%s=%s%s",
		   namePt, dataPt, (nextNamePt ? "&" : ""));
    if (startsWith(CT_FILE_VAR_PREFIX, namePt))
	{
	cgiDecode(dataPt, dataPt, strlen(dataPt));
	boolean thisGotLiveCT = FALSE, thisGotExpiredCT = FALSE;
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

	    customFactoryTestExistence(db, dataPt, &thisGotLiveCT, &thisGotExpiredCT, NULL);

	    ++CFTEcalls;
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
    else if (sameString(namePt, "multiRegionsBedUrl"))
	{
	// touch corresponding multi-region custom regions .bed and .sha1 files to save them from trash cleaner.
	cgiDecode(dataPt, dataPt, strlen(dataPt));
	char multiRegionsBedUrlSha1Name[1024];
	safef(multiRegionsBedUrlSha1Name, sizeof multiRegionsBedUrlSha1Name, "%s.sha1", dataPt);
        if (!sameString(dataPt,"") && !strstr(dataPt,"://"))
	    {  // should have a bed file in trash and a sha1 for quick change detection
	    if (fileExists(dataPt) && fileExists(multiRegionsBedUrlSha1Name))
		{
		readAndIgnore(dataPt); // touch access time
		readAndIgnore(multiRegionsBedUrlSha1Name);
		verbose(4, "setting multiRegionsBedUrl: %s\n", dataPt);
		verbose(4, "setting multiRegionsBedUrl: %s\n", multiRegionsBedUrlSha1Name);
	        dyStringAppend(newContents, oneSetting->string);
		}
	    }
	}
    else if (startsWith("customComposite", namePt))
	{
	cgiDecode(dataPt, dataPt, strlen(dataPt));
	if (fileExists(dataPt))
	    {
	    readAndIgnore(dataPt); // touch access time
	    verbose(4, "setting compositeFile: %s\n", dataPt);
	    dyStringAppend(newContents, oneSetting->string);
	    }
	}
    else
	{
	dyStringAppend(newContents, oneSetting->string);
	}
    namePt = nextNamePt;
    }
if (newContents->stringSize != contentLength) 
    ++numUpdates;
if (optionExists("hardcore") && newContents->stringSize != contentLength)  // almost never used
    {
    struct dyString *update = dyStringNew(contentLength*2);
    if (newContents->stringSize > contentLength)
	errAbort("ERROR: Uh, why is newContents (%ld) longer than original (%d)",
		 newContents->stringSize, contentLength);
    sqlDyStringPrintf(update, "UPDATE %s set contents='", savedSessionTable);
    dyStringAppendN(update, newContents->string, newContents->stringSize);
    dyStringPrintf(update, "', lastUse=now(), useCount=useCount+1 "
		   "where userName=\"%s\" and sessionName=\"%s\";",
		   userName, sessionName);
    verbose(3, "Removing one or more dead CT file settings from %s %s "
	    "(original length %d, now %ld)\n", 
	    userName, sessionName,
	    contentLength, newContents->stringSize);
    sqlUpdate(conn, update->string);
    dyStringFree(&update);
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
    };

void refreshNamedSessionCustomTracks(char *centralDbName)
/* refreshNamedSessionCustomTracks -- cron robot for keeping alive custom 
 * tracks that are referenced by saved sessions. */
{
// Helpful for showing rapid version changes.  
// Be sure to increment this when committing and pushing.
// People testing will know they have the correct new version.
verbose(1, "refreshNamedSessionCustomTracks version #%d\n", version);  

// Avoids a problem in hdb.c deep in the library
// that would otherwise fail when trying to set this
// on a new child process using a custom track that
// was defined on a hub that is not presently able to load.
// However, because that hub might be OK in the future,
// we need to not fail and keep touching the custom trash db files.
// This static library variable has been wrong ever since this program was created
// and run to clean the trash, so clearly it is not actually needed 
// for the trash cleaning.
setMinIndexLengthForTrashCleaner();  

struct sqlConnection *conn = unCachedCentralConn();
char *actualDbName = sqlGetDatabase(conn);
int liveCount=0, expiredCount=0;

setUdcCacheDir();  /* programs that use udc must call this to initialize cache dir location */

if (!sameString(centralDbName, actualDbName))
    errAbort("ERROR: Central database specified in hg.conf file is %s but %s "
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
    sqlSafef(query, sizeof(query),
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
    // processing them in the same order they appear in the table helps testing 
    slReverse(&sessionList); 
    sqlFreeResult(&sr);
    }
else
    errAbort("ERROR: can not find table %s.%s on central.host: '%s'",
	savedSessionTable, centralDbName, cfgOption("central.host"));

// parent must close all db connections before forking
sqlDisconnect(&conn);

int sessionsPerForkDone=0; // number of sessions done for current fork
int listLength = slCount(sessionList);
int perFork = listLength / numForks;  // number of sessions per fork
int forkRem = listLength % numForks;  // remainder of the above division
if (forkRem > 0)
    ++perFork;   // deal with remainder by starting off with elevated perFork

pid_t pid = 0;
boolean parent = TRUE;
int fork = 0;

// DEALING WITH USING LOTS OF MEMORY
// To avoid taking too much ram from leaks when loading the saved named sessions,
// this program splits the session list into numForks pieces (default 10),
// and the hands them off to a child forked process.
// When the child finishes, the operating system will free up its memory.
// This repeats for each child until the entire list has finished.
// The children run sequentially, not in parallel.

// It is CRUCIAL that this program exits with non-zero exit code
// when it or any of its children exit non-zero, abort, get killed, or crash,
// in order to tell the calling program that it has failed.
// This is the only thing will stop the deletion and loss of saved named sessions!

// Every single session MUST be processed in order to save it from deletion.
for (si = sessionList;  si != NULL;  si = si->next)
    {
	
    if (parent && sessionsPerForkDone == 0)
	{
	pid = forkIt();
	if (pid == 0)
	    {
	    parent = FALSE;
	    conn = unCachedCentralConn(); // avoid cached connections when forking 
	    }
        }
    
    if (!parent)
	{
    	scanSettingsForCT(si->userName, si->sessionName, &liveCount, &expiredCount, conn);
	}
    ++sessionsPerForkDone;

    if (sessionsPerForkDone >= perFork) 
	// the fork has done all of its sessions
	{

	// Adjust for the fact that divisions have remainders.
	// We want to split the list into numForks, but it often does not divide evenly.
	// The first forkRem forks will get one extra session to do if forkRem > 0.
	// It use important that we do not create any extra fork so that the count will
	// match numForks (default 10) in the output.

	++fork;

	if (fork == forkRem) 
	    --perFork;

	sessionsPerForkDone = 0;
	if (parent)
	    {
	    waitForChildWithTimeout(pid);
	    }
        else
	    {
	    verbose(1, "# of updates found: %d\n", numUpdates);
	    verbose(1, "# of CustomFactoryTextExistence calls done: %d\n", CFTEcalls);
	    verbose(1, "Found %d live and %d expired custom tracks in %s.\n",
		liveCount, expiredCount, centralDbName);
	    sqlDisconnect(&conn);
	    // Causes "VmPeak" to appear in the stderr output.
	    // The caller of this program can look for exactly numForks
	    // lines of "VmPeak" in the output as a double-check that the program completed.
	    showVmPeak();  
	    verbose(1, "forked child process# %d done.\n", fork);
	    // It is important to be able to easily test that these failures 
	    // behave correctly, and cause the parent process to exit with non-zero exit code.
	    if (testFailure) // {exitCode|errAbort|segfault|timeout}
		{
		if (sameString(testFailure, "exitCode"))
		    exit(1);
		else if (sameString(testFailure, "errAbort"))
		    errAbort("Test asked for errAbort in child");
		else if (sameString(testFailure, "segfault"))
		    {
		    char *ptr = NULL;
		    char c = *ptr; // invalid null pointer should cause segfault.
		    printf("c=%c\n",c);  // it should never get here. Make compiler happy.
		    }
		else if (sameString(testFailure, "timeout"))
		    {
		    // nothing happens in child, but parent will timeout in 1 second and kill the child.
		    }
		else 
		    errAbort("unknown value [%s] for testFailure", testFailure);
		}
	    exit(0);
	    }
	}
    }

verbose(1, "parent process done.\n");

}


int main(int argc, char *argv[])
/* Process command line. */
{
// EXERCISE CARE WHEN MODIFYING THIS PROGRAM!

// THIS PROGRAM IS CRITICAL FOR PRESERVING USER SAVED SESSION DATA
// ESPECIALLY CUSTOM TRACKS DATA AND CUSTOM TRACKS DATABASE.  
// BE SURE TO DISCUSS CHANGES IN DETAIL WITH THE PEOPLE THAT RUN THE PROGRAM.

// Please also increment the "version" variable at the top of the source.
// This is helpful when you are commiting a new change and giving it
// to others to run or test with the trash cleaning scripts,
// especially when it is changing rapidly.
optionInit(&argc, argv, options); // causes it to emit ### kent source version 999 ###
if (argc != 2)
    usage();
numForks = optionInt("forks", numForks);
if (numForks < 1)
    errAbort("forks option must specify positive integer >= 1");
timeoutSecs = optionInt("timeoutSecs", timeoutSecs);
testFailure = optionVal("testFailure", testFailure);
if (testFailure && sameString(testFailure,"timeout"))
    timeoutSecs = 1; // Timeout for each forked child process
char *workDir = optionVal("workDir", CGI_BIN);
setCurrentDir(workDir);

refreshNamedSessionCustomTracks(argv[1]);

return 0;
}

