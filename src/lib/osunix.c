/* Some wrappers around operating-system specific stuff. */

#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/utsname.h>
#include "common.h"
#include "portable.h"
#include "portimpl.h"


/* Return how long the named file is in bytes. 
 * Return -1 if no such file. */
long fileSize(char *fileName)
{
int fd;
long size;
fd = open(fileName, O_RDONLY, 0);
if (fd < 0)
    return -1;
size = lseek(fd, 0L, 2);
close(fd);
return size;
}


long clock1000()
/* A millisecond clock. */
{
static double scale = 1000.0/CLOCKS_PER_SEC;
return round(scale*clock());
}

long clock1()
/* A seconds clock. */
{
return clock()/CLOCKS_PER_SEC;
}

void uglyfBreak()
/* Go into debugger. */
{
static char *nullPt = NULL;
nullPt[0] = 0;
}

char *getCurrentDir()
/* Return current directory. */
{
static char dir[512];

if (getcwd( dir, sizeof(dir) ) == NULL )
    {
    warn("No current directory");
    return NULL;
    }
return dir;
}

boolean setCurrentDir(char *newDir)
/* Set current directory.  Return FALSE if it fails. */
{
if (chdir(newDir) != 0)
    {
    warn("Unable to set dir %s", newDir);
    return FALSE;
    }
return TRUE;
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

struct fileInfo *newFileInfo(char *name, long size, bool isDir)
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

char *rTempName(char *dir, char *base, char *suffix)
/* Make a temp name that's almost certainly unique. */
{
char midder[256];
int pid = getpid();
int num = time(NULL);
static char fileName[512];
char host[512];
char *s;

strcpy(host, getHost());
s = strchr(host, '.');
if (s != NULL)
     *s = 0;
for (;;)
   {
   sprintf(fileName, "%s/%s_%s_%d_%d%s", dir, base, host, pid, num, suffix);
   if (!fileExists(fileName))
       break;
   num += 1;
   }
return fileName;
}


