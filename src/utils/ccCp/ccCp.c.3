/* ccCp - copy file to the cluster efficiently. */
#include "common.h"
#include "obscure.h"
#include "portable.h"
#include "dystring.h"

boolean amFirst = FALSE; /* Is this the copy that launched the copies? */

char *fullPathName(char *relName)
/* Return full version of path name. */
{
char firstChar = relName[0];
char fullPath[512];
char dir[512];

if (firstChar == '/' || firstChar == '~')
    return cloneString(relName);
getcwd(dir, sizeof(dir));
sprintf(fullPath, "%s/%s", dir, relName);
return cloneString(fullPath);
}

int scpFile(char *source, char *destHost, char *destFile)
/* Execute scp command to copy source file to host. */
{
struct dyString *dy = newDyString(512);
int ret;

dyStringPrintf(dy, "scp %s %s:/%s >/dev/null",  source, destHost, destFile);
ret = system(dy->string);
freeDyString(&dy);
return ret;
}

int cpFile(char *source, char *destFile)
/* Execute cp command to copy file to host. */
{
struct dyString *dy = newDyString(512);
int ret;

dyStringPrintf(dy, "cp %s %s",  source, destFile);
ret = system(dy->string);
freeDyString(&dy);
return ret;
}

void sshSelf(char *hostList, char *host, int start, char *destFile, char *lockDir)
/* Execute ssh command to invoke self. */
{
struct dyString *dy = newDyString(512);
dyStringPrintf(dy, 
        "ssh -x %s /projects/compbio/experiments/hg/bin/i386/ccCp %s %s %d %s &",
	host, destFile, hostList, start, lockDir);
system(dy->string);
freeDyString(&dy);
}

void usage()
/* Explain usage and exit. */
{
errAbort(
"ccCp - copy a file to cluster."
"usage:\n"
"   ccCp sourceFile destFile [hostList]\n"
"This will copy sourceFile to destFile for all machines in\n"
"hostList\n"
"\n"
"example:\n"
"   ccCp h.zip /var/tmp/h.zip newHosts");
}

char *lockName(char *lockDir, char *host)
/* Return name of lock for host. */
{
static char buf[512];
sprintf(buf, "%s/%s", lockDir, host);
return buf;
}

int makeLock(char *host, char *lockDir)
/* Return lock file handle. */
{
char *name = lockName(lockDir, host);
int fd;
fd = open(name, O_CREAT | O_EXCL | O_WRONLY, 0666);
return fd;
}

void cleanupLocks(char *lockDir)
/* Remove all locks. */
{
struct slName *dir = listDir(lockDir, "*");
char name[512];
struct slName *one;

for (one = dir; one != NULL; one = one->next)
    {
    sprintf(name, "%s/%s", lockDir, one->name);
    remove(name);
    }
rmdir(lockDir);
}

void ccMore(char *fileName, char *hostList, int start, char *lockDir)
/* Copy source file to hostList starting at start.  Look at
 * lock files in lock dir to see which one to do next. */
{
char **hosts;
char *newHost;
char *hostBuf;
int hostCount;
int i;

readAllWords(hostList, &hosts, &hostCount, &hostBuf);
if (hostCount <= 0)
    errAbort("%s is empty.", hostList);
for (i=start; i<hostCount; ++i)
    {
    int lockFd;
    newHost = hosts[i];
    if ((lockFd = makeLock(newHost, lockDir)) >= 0)
	{
	char ok = TRUE;
	if (scpFile(fileName, newHost, fileName) != 0)
	    ok = FALSE;
	write(lockFd, &ok, 1);
	close(lockFd);
	if (ok )
	    {
	    if (i <= (hostCount+1)/2)	/* Don't spawn off on last round. */
		sshSelf(hostList, newHost, i+1, fileName, lockDir);
	    }
	}
    }
}


void ccFirst(char *source, char *dest, char *hostList, char *lockDir)
/* Do first instance of this program.  Copy file to first host,
 * make up lock directory, and then poll lock directory to see
 * if we're done. */
{
char *firstHost, *lastHost;
char **hosts;
char *hostBuf;
int hostCount;
int firstLock;
int childPid;
char *thisHost = getenv("HOST");
char ok;
long startTime = clock1000();

if (thisHost == NULL)
    errAbort("HOST environment variable undefined\n");
readAllWords(hostList, &hosts, &hostCount, &hostBuf);
if (hostCount <= 0)
    errAbort("%s is empty.", hostList);
if (stringArrayIx(thisHost, hosts, hostCount) < 0)
    errAbort("Current host (%s) not in host list\n", thisHost);
if (mkdir(lockDir, 0777) < 0)
    errAbort("Couldn't make lock directory %s\n", lockDir);
firstHost = thisHost;
lastHost = hosts[hostCount-1];
if (sameString(lastHost, thisHost) && hostCount > 1)
    lastHost = hosts[hostCount-2];
firstLock = makeLock(firstHost, lockDir);
if (firstLock < 0)
    errAbort("Couldn't make lock file %s/%s\n", lockDir, firstHost);
if (cpFile(source, dest) != 0)
    {
    warn("Couldn't copy %s to %s:%d\n", source, firstHost, dest);
    close(firstLock);
    cleanupLocks(lockDir);
    errAbort("Cleaned up locks in %s, aborting copy.", lockDir);
    }
ok = 1;
write(firstLock, &ok, 1);
close(firstLock);

childPid = fork();
if (childPid == 0)
    {
    /* Have child process keep copying. */
    ccMore(dest, hostList, 0, lockDir);
    }
else
    {
    int sleepIx = 0;
    int sleepTime = 10;
    int lastStart = 0, lastErr = 0, lastEnd = 0;

    /* Have parent process wait until last file done. */
    for (sleepIx = 0; ; ++sleepIx)
	{
	int lockFd;
	int i;
	int startCount = 0;
	int endCount = 0;
	int errCount = 0;
	int toGo = 0;
	int procCount = 0;
	int lastProcCount = 0;
	int finCount;
	boolean reportErr;

	for (i=0; i<hostCount; ++i)
	    {
	    char *ln = lockName(lockDir, hosts[i]);
	    lockFd = open(ln, O_RDONLY);
	    if (lockFd < 0)
		++toGo;
	    else
		{
		char ok;
		if (read(lockFd, &ok, 1) < 1)
		    ++startCount;
		else
		    {
		    if (ok)
			++endCount;
		    else
			++errCount;
		    }
		close(lockFd);
		}
	    }
	finCount = endCount + errCount;
	// if (lastStart != startCount || lastEnd != endCount || lastErr != errCount)
	    {
	    printf(" copies in progress %d finished %d errors %d total %d\n",
		startCount, endCount, errCount, hostCount);
	    lastStart = startCount;
	    lastEnd = endCount;
	    lastErr = errCount;
	    }
	if (finCount >= hostCount)
	    {
	    if (errCount > 0)
		{
		fprintf(stderr, "Errors copying to hosts:");
		for (i=0; i<hostCount; ++i)
		    {
		    char *ln = lockName(lockDir, hosts[i]);
		    lockFd = open(ln, O_RDONLY);
		    if (lockFd < 0)
			{
			fprintf(stderr, " ??%s??", hosts[i]);
			}
		    else
			{
			char ok;
			if (read(lockFd, &ok, 1) < 1)
			    {
			    fprintf(stderr, " ?%s?", hosts[i]);
			    ++startCount;
			    }
			else
			    {
			    if (!ok)
				{
				fprintf(stderr, " %s", hosts[i]);
				++errCount;
				}
			    }
			close(lockFd);
			}
		    }
		fprintf(stderr, "\n");
		}
	    cleanupLocks(lockDir);
	    break;
	    }
	sleep(sleepTime);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
int start = 0, count = 0;
char *lockDir;
char *source, *dest, *hostList;

if (argc != 3 && argc != 4 && argc != 5)
    usage();
if (argc == 5)
    {
    dest = argv[1];
    hostList = argv[2];
    start = atoi(argv[3]);
    lockDir = argv[4];
    ccMore(dest, hostList, start, lockDir);
    }
else
    {
    source = argv[1];
    dest = argv[2];
    if (argc == 4)
	{
	hostList = argv[3];
	hostList = fullPathName(hostList);
	}
    else
	hostList = "/projects/compbio/experiments/hg/masterList";
    lockDir = tempnam("/projects/compbio/experiments/hg/tmp", "lock");
    amFirst = TRUE;
    ccFirst(source, dest, hostList, lockDir);
    }
}

