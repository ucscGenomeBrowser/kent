/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include "common.h"
#include "portable.h"


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
	if (wildMatch(pattern, fileName))
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
    }
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
	if (wildMatch(pattern, fileName))
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
