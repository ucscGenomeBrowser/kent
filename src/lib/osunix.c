/* Some wrappers around operating-system specific stuff. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include <dirent.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <pwd.h>
#include <termios.h>
#include "portable.h"
#include "portimpl.h"
#include <sys/wait.h>
#include <regex.h>
#include <utime.h>




off_t fileSize(char *pathname)
/* get file size for pathname. return -1 if not found */
{
struct stat mystat;
ZeroVar(&mystat);
if (stat(pathname,&mystat)==-1)
    {
    return -1;
    }
return mystat.st_size;
}


long clock1000()
/* A millisecond clock. */
{
struct timeval tv;
static long origSec;
gettimeofday(&tv, NULL);
if (origSec == 0)
    origSec = tv.tv_sec;
return (tv.tv_sec-origSec)*1000 + tv.tv_usec / 1000;
}

void sleep1000(int milli)
/* Sleep for given number of 1000ths of second */
{
if (milli > 0)
    {
    struct timeval tv;
    tv.tv_sec = milli/1000;
    tv.tv_usec = (milli%1000)*1000;
    select(0, NULL, NULL, NULL, &tv);
    }
}

long clock1()
/* A seconds clock. */
{
struct timeval tv;
gettimeofday(&tv, NULL);
return tv.tv_sec;
}

void uglyfBreak()
/* Go into debugger. */
{
static char *nullPt = NULL;
nullPt[0] = 0;
}

char *getCurrentDir()
/* Return current directory.  Abort if it fails. */
{
static char dir[PATH_LEN];

if (getcwd( dir, sizeof(dir) ) == NULL )
    errnoAbort("getCurrentDir: can't get current directory");
return dir;
}

void setCurrentDir(char *newDir)
/* Set current directory.  Abort if it fails. */
{
if (chdir(newDir) != 0)
    errnoAbort("setCurrentDir: can't to set current directory: %s", newDir);
}

boolean maybeSetCurrentDir(char *newDir)
/* Change directory, return FALSE (and set errno) if fail. */
{
return chdir(newDir) == 0;
}

struct slName *listDir(char *dir, char *pattern)
/* Return an alphabetized list of all files that match 
 * the wildcard pattern in directory. */
{
struct slName *list = NULL, *name;
struct dirent *de;
DIR *d;

if ((d = opendir(dir)) == NULL)
    return NULL;
while ((de = readdir(d)) != NULL)
    {
    char *fileName = de->d_name;
    if (differentString(fileName, ".") && differentString(fileName, ".."))
	{
	if (pattern == NULL || wildMatch(pattern, fileName))
	    {
	    name = newSlName(fileName);
	    slAddHead(&list, name);
	    }
	}
    }
closedir(d);
slNameSort(&list);
return list;
}

struct slName *listDirRegEx(char *dir, char *regEx, int flags)
/* Return an alphabetized list of all files that match 
 * the regular expression pattern in directory.
 * See REGCOMP(3) for flags (e.g. REG_ICASE)  */
{
struct slName *list = NULL, *name;
struct dirent *de;
DIR *d;
regex_t re;
int err = regcomp(&re, regEx, flags | REG_NOSUB);
if(err)
    errAbort("regcomp failed; err: %d", err);

if ((d = opendir(dir)) == NULL)
    return NULL;
while ((de = readdir(d)) != NULL)
    {
    char *fileName = de->d_name;
    if (differentString(fileName, ".") && differentString(fileName, ".."))
	{
	if (!regexec(&re, fileName, 0, NULL, 0))
	    {
	    name = newSlName(fileName);
	    slAddHead(&list, name);
	    }
	}
    }
closedir(d);
regfree(&re);
slNameSort(&list);
return list;
}

struct fileInfo *newFileInfo(char *name, off_t size, bool isDir, int statErrno, 
	time_t lastAccess)
/* Return a new fileInfo. */
{
int len = strlen(name);
struct fileInfo *fi = needMem(sizeof(*fi) + len);
fi->size = size;
fi->isDir = isDir;
fi->statErrno = statErrno;
fi->lastAccess = lastAccess;
strcpy(fi->name, name);
return fi;
}

int cmpFileInfo(const void *va, const void *vb)
/* Compare two fileInfo. */
{
const struct fileInfo *a = *((struct fileInfo **)va);
const struct fileInfo *b = *((struct fileInfo **)vb);
return strcmp(a->name, b->name);
}

boolean makeDir(char *dirName)
/* Make dir.  Returns TRUE on success.  Returns FALSE
 * if failed because directory exists.  Prints error
 * message and aborts on other error. */
{
int err;
if ((err = mkdir(dirName, 0777)) < 0)
    {
    if (errno != EEXIST)
	{
	perror("");
	errAbort("Couldn't make directory %s", dirName);
	}
    return FALSE;
    }
return TRUE;
}


struct fileInfo *listDirXExt(char *dir, char *pattern, boolean fullPath, boolean ignoreStatFailures)
/* Return list of files matching wildcard pattern with
 * extra info. If full path is true then the path will be
 * included in the name of each file. */
{
struct fileInfo *list = NULL, *el;
struct dirent *de;
DIR *d;
int dirNameSize = strlen(dir);
int fileNameOffset = dirNameSize+1;
char pathName[512];

if ((d = opendir(dir)) == NULL)
    return NULL;
memcpy(pathName, dir, dirNameSize);
pathName[dirNameSize] = '/';

while ((de = readdir(d)) != NULL)
    {
    char *fileName = de->d_name;
    if (differentString(fileName, ".") && differentString(fileName, ".."))
	{
	if (pattern == NULL || wildMatch(pattern, fileName))
	    {
	    struct stat st;
	    bool isDir = FALSE;
	    int statErrno = 0;
	    strcpy(pathName+fileNameOffset, fileName);
	    if (stat(pathName, &st) < 0)
		{
		if (ignoreStatFailures)
		    statErrno = errno;
		else
    		    errAbort("stat failed in listDirX");
		}
	    if (S_ISDIR(st.st_mode))
		isDir = TRUE;
	    if (fullPath)
		fileName = pathName;
	    el = newFileInfo(fileName, st.st_size, isDir, statErrno, st.st_atime);
	    slAddHead(&list, el);
	    }
	}
    }
closedir(d);
slSort(&list, cmpFileInfo);
return list;
}

struct fileInfo *listDirX(char *dir, char *pattern, boolean fullPath)
/* Return list of files matching wildcard pattern with
 * extra info. If full path is true then the path will be
 * included in the name of each file. */
{
return listDirXExt(dir, pattern, fullPath, FALSE);
}

time_t fileModTime(char *pathName)
/* Return file last modification time.  The units of
 * these may vary from OS to OS, but you can depend on
 * later files having a larger time. */
{
struct stat st;
if (stat(pathName, &st) < 0)
    errAbort("stat failed in fileModTime: %s", pathName);
return st.st_mtime;
}


char *getHost()
/* Return host name. */
{
static char *hostName = NULL;
static char buf[128];
if (hostName == NULL)
    {
    hostName = getenv("HTTP_HOST");
    if (hostName == NULL)
        {
	hostName = getenv("HOST");
	if (hostName == NULL)
	    {
	    if (hostName == NULL)
		{
		static struct utsname unamebuf;
		if (uname(&unamebuf) >= 0)
		    hostName = unamebuf.nodename;
		else
		    hostName = "unknown";
		}
	    }
        }
    strncpy(buf, hostName, sizeof(buf));
    chopSuffix(buf);
    hostName = buf;
    }
return hostName;
}

char *mysqlHost()
/* Return host computer on network for mySQL database. */
{
boolean gotIt = FALSE;
static char *host = NULL;
if (!gotIt)
    {
    static char hostBuf[128];
    gotIt = TRUE;
    if (fileExists("mysqlHost"))
	{
	return (host = firstWordInFile("mysqlHost", hostBuf, sizeof(hostBuf)));
	}
    else
	return (host = getenv("MYSQLHOST"));
    }
return host;
}

char *semiUniqName(char *base)
/* Figure out a name likely to be unique.
 * Name will have no periods.  Returns a static
 * buffer, so best to clone result unless using
 * immediately. */
{
int pid = getpid();
int num = time(NULL)&0xFFFFF;
char host[512];
strcpy(host, getHost());
char *s = strchr(host, '.');
if (s != NULL)
     *s = 0;
subChar(host, '-', '_');
subChar(host, ':', '_');
static char name[PATH_LEN];
safef(name, sizeof(name), "%s_%s_%x_%x",
	base, host, pid, num);
return name;
}

char *rTempName(char *dir, char *base, char *suffix)
/* Make a temp name that's almost certainly unique. */
{
char *x;
static char fileName[PATH_LEN];
int i;
for (i=0;;++i)
    {
    x = semiUniqName(base);
    safef(fileName, sizeof(fileName), "%s/%s%d%s",
    	dir, x, i, suffix);
    if (!fileExists(fileName))
        break;
    }
return fileName;
}

static void eatSlashSlashInPath(char *path)
/* Convert multiple // to single // */
{
char *s, *d;
s = d = path;
char c, lastC = 0;
while ((c = *s++) != 0)
    {
    if (c == '/' && lastC == c)
        continue;
    *d++ = c;
    lastC = c;
    }
*d = 0;
}

static void eatExcessDotDotInPath(char *path)
/* If there's a /.. in path take it out.  Turns 
 *      'this/long/../dir/file' to 'this/dir/file
 * and
 *      'this/../file' to 'file'  
 *
 * and
 *      'this/long/..' to 'this'
 * and
 *      'this/..' to  ''   
 * and
 *       /this/..' to '/' */
{
/* Take out each /../ individually */
for (;;)
    {
    /* Find first bit that needs to be taken out. */
    char *excess= strstr(path, "/../");
    char *excessEnd = excess+4;
    if (excess == NULL || excess == path)
        break;

    /* Look for a '/' before this */
    char *excessStart = matchingCharBeforeInLimits(path, excess, '/');
    if (excessStart == NULL) /* Preceding '/' not found */
         excessStart = path;
    else 
         excessStart += 1;
    strcpy(excessStart, excessEnd);
    }

/* Take out final /.. if any */
if (endsWith(path, "/.."))
    {
    if (!sameString(path, "/.."))  /* We don't want to turn this to blank. */
	{
	int len = strlen(path);
	char *excessStart = matchingCharBeforeInLimits(path, path+len-3, '/');
	if (excessStart == NULL) /* Preceding '/' not found */
	     excessStart = path;
	else 
	     excessStart += 1;
	*excessStart = 0;
	}
    }
}

char *simplifyPathToDir(char *path)
/* Return path with ~ and .. taken out.  Also any // or trailing /.   
 * freeMem result when done. */
{
/* Expand ~ if any with result in newPath */
char newPath[PATH_LEN];
int newLen = 0;
char *s = path;
if (*s == '~')
    {
    char *homeDir = getenv("HOME");
    if (homeDir == NULL)
        errAbort("No HOME environment var defined after ~ in simplifyPathToDir");
    ++s;
    if (*s == '/')  /*    ~/something      */
        {
	++s;
	safef(newPath, sizeof(newPath), "%s/", homeDir);
	}
    else            /*   ~something        */
	{
	safef(newPath, sizeof(newPath), "%s/../", homeDir);
	}
    newLen = strlen(newPath);
    }
int remainingLen  = strlen(s);
if (newLen + remainingLen >= sizeof(newPath))
    errAbort("path too big in simplifyPathToDir");
strcpy(newPath+newLen, s);

/* Remove //, .. and trailing / */
eatSlashSlashInPath(newPath);
eatExcessDotDotInPath(newPath);
int lastPos = strlen(newPath)-1;
if (lastPos > 0 && newPath[lastPos] == '/')
    newPath[lastPos] = 0;

return cloneString(newPath);
}

#ifdef DEBUG
void simplifyPathToDirSelfTest()
{
/* First test some cases which should remain the same. */
assert(sameString(simplifyPathToDir(""),""));
assert(sameString(simplifyPathToDir("a"),"a"));
assert(sameString(simplifyPathToDir("a/b"),"a/b"));
assert(sameString(simplifyPathToDir("/"),"/"));
assert(sameString(simplifyPathToDir("/.."),"/.."));
assert(sameString(simplifyPathToDir("/../a"),"/../a"));

/* Now test removing trailing slash. */
assert(sameString(simplifyPathToDir("a/"),"a"));
assert(sameString(simplifyPathToDir("a/b/"),"a/b"));

/* Test .. removal. */
assert(sameString(simplifyPathToDir("a/.."),""));
assert(sameString(simplifyPathToDir("a/../"),""));
assert(sameString(simplifyPathToDir("a/../b"),"b"));
assert(sameString(simplifyPathToDir("/a/.."),"/"));
assert(sameString(simplifyPathToDir("/a/../"),"/"));
assert(sameString(simplifyPathToDir("/a/../b"),"/b"));
assert(sameString(simplifyPathToDir("a/b/.."),"a"));
assert(sameString(simplifyPathToDir("a/b/../"),"a"));
assert(sameString(simplifyPathToDir("a/b/../c"),"a/c"));
assert(sameString(simplifyPathToDir("a/../b/../c"),"c"));
assert(sameString(simplifyPathToDir("a/../b/../c/.."),""));
assert(sameString(simplifyPathToDir("/a/../b/../c/.."),"/"));

/* Test // removal */
assert(sameString(simplifyPathToDir("//"),"/"));
assert(sameString(simplifyPathToDir("//../"),"/.."));
assert(sameString(simplifyPathToDir("a//b///c"),"a/b/c"));
assert(sameString(simplifyPathToDir("a/b///"),"a/b"));
}
#endif /* DEBUG */

char *getUser()
/* Get user name */
{
uid_t uid = geteuid();
struct passwd *pw = getpwuid(uid);
if (pw == NULL)
    errnoAbort("getUser: can't get user name for uid %d", (int)uid);
return pw->pw_name;
}

int mustFork()
/* Fork or abort. */
{
int childId = fork();
if (childId == -1)
    errnoAbort("mustFork: Unable to fork");
return childId;
}

int rawKeyIn()
/* Read in an unbuffered, unechoed character from keyboard. */
{
struct termios attr;
tcflag_t old;
char c;

/* Set terminal to non-echoing non-buffered state. */
if (tcgetattr(STDIN_FILENO, &attr) != 0)
    errAbort("Couldn't do tcgetattr");
old = attr.c_lflag;
attr.c_lflag &= ~ICANON;
attr.c_lflag &= ~ECHO;
if (tcsetattr(STDIN_FILENO, TCSANOW, &attr) == -1)
    errAbort("Couldn't do tcsetattr");

/* Read one byte */
if (read(STDIN_FILENO,&c,1) != 1)
   errnoAbort("rawKeyIn: I/O error");

/* Put back terminal to how it was. */
attr.c_lflag = old;
if (tcsetattr(STDIN_FILENO, TCSANOW, &attr) == -1)
    errAbort("Couldn't do tcsetattr2");
return c;
}

boolean isPipe(int fd)
/* determine in an open file is a pipe  */
{
struct stat buf;
if (fstat(fd, &buf) < 0)
    errnoAbort("isPipe: fstat failed");
return S_ISFIFO(buf.st_mode);
}

static void execPStack(pid_t ppid)
/* exec pstack on the specified pid */
{
char *cmd[3], pidStr[32];
safef(pidStr, sizeof(pidStr), "%ld", (long)ppid);
cmd[0] = "pstack";
cmd[1] = pidStr;
cmd[2] = NULL;

// redirect stdout to stderr
if (dup2(2, 1) < 0)
    errAbort("dup2 failed");

execvp(cmd[0], cmd);
errAbort("exec failed: %s", cmd[0]);
}

void vaDumpStack(char *format, va_list args)
/* debugging function to run the pstack program on the current process. In
 * prints a message, following by a new line, and then the stack track.  Just
 * prints errors to stderr rather than aborts. For debugging purposes
 * only.  */
{
static boolean inDumpStack = FALSE;  // don't allow re-entry if called from error handler
if (inDumpStack)
    return;
inDumpStack = TRUE;

fflush(stdout);  // clear buffer before forking
vfprintf(stderr, format, args);
fputc('\n', stderr);
fflush(stderr);
pid_t ppid = getpid();
pid_t pid = fork();
if (pid < 0)
    {
    perror("can't fork pstack");
    return;
    }
if (pid == 0)
    execPStack(ppid);
int wstat;
if (waitpid(pid, &wstat, 0) < 0)
    perror("waitpid on pstack failed");
else
    {
    if (WIFEXITED(wstat))
        {
        if (WEXITSTATUS(wstat) != 0)
            fprintf(stderr, "pstack failed\n");
        }
    else if (WIFSIGNALED(wstat))
        fprintf(stderr, "pstack signaled %d\n", WTERMSIG(wstat));
    }
inDumpStack = FALSE;
}

void dumpStack(char *format, ...)
/* debugging function to run the pstack program on the current process. In
 * prints a message, following by a new line, and then the stack track.  Just
 * prints errors to stderr rather than aborts. For debugging purposes
 * only.  */
{
va_list args;
va_start(args, format);
vaDumpStack(format, args);
va_end(args);
}

boolean maybeTouchFile(char *fileName)
/* If file exists, set its access and mod times to now.  If it doesn't exist, create it.
 * Return FALSE if we have a problem doing so (e.g. when qateam is gdb'ing and code tries 
 * to touch some file owned by www). */
{
if (fileExists(fileName))
    {
    struct utimbuf ut;
    ut.actime = ut.modtime = clock1();
    int ret = utime(fileName, &ut);
    if (ret != 0)
	{
	warn("utime(%s) failed (ownership?)", fileName);
	return FALSE;
	}
    }
else
    {
    FILE *f = fopen(fileName, "w");
    if (f == NULL)
	return FALSE;
    else
	carefulClose(&f);
    }
return TRUE;
}
