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

static char const rcsid[] = "$Id: osunix.c,v 1.32 2008/06/27 18:46:53 markd Exp $";


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
    errnoAbort("can't get current directory");
return dir;
}

void setCurrentDir(char *newDir)
/* Set current directory.  Abort if it fails. */
{
if (chdir(newDir) != 0)
    errnoAbort("can't to set current directory: %s", newDir);
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

struct fileInfo *newFileInfo(char *name, off_t size, bool isDir)
/* Return a new fileInfo. */
{
int len = strlen(name);
struct fileInfo *fi = needMem(sizeof(*fi) + len);
fi->size = size;
fi->isDir = isDir;
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


struct fileInfo *listDirX(char *dir, char *pattern, boolean fullPath)
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
	    strcpy(pathName+fileNameOffset, fileName);
	    if (stat(pathName, &st) < 0)
		errAbort("stat failed in listDirX");
	    if (S_ISDIR(st.st_mode))
		isDir = TRUE;
	    if (fullPath)
		fileName = pathName;
	    el = newFileInfo(fileName, st.st_size, isDir);
	    slAddHead(&list, el);
	    }
	}
    }
closedir(d);
slSort(&list, cmpFileInfo);
return list;
}

unsigned long fileModTime(char *pathName)
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

char *getUser()
/* Get user name */
{
uid_t uid = geteuid();
struct passwd *pw = getpwuid(uid);
if (pw == NULL)
    errnoAbort("can't get user name for uid %d", (int)uid);
return pw->pw_name;
}

int mustFork()
/* Fork or abort. */
{
int childId = fork();
if (childId == -1)
    errnoAbort("Unable to fork");
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
   errnoAbort("I/O error");

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
    errnoAbort("fstat failed");
return S_ISFIFO(buf.st_mode);
}
