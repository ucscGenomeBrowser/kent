/* portable.h - wrappers around things that vary from server
 * to server and operating system to operating system. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef PORTABLE_H
#define PORTABLE_H

#include <sys/types.h>
#include <sys/stat.h>

/* Return an alphabetized list of all files that match 
 * the wildcard pattern in directory. */
struct slName *listDir(char *dir, char *pattern);

struct fileInfo 
/* Info about a file. */
    {
    struct fileInfo  *next;	/* Next in list. */
    long size;		/* Size in bytes. */
    bool isDir;		/* True if file is a directory. */
    char name[1];	/* Allocated at run time. */
    };

/* Return a new fileInfo. */
struct fileInfo *newFileInfo(char *name, long size, bool isDir);

/* Return list of files matching wildcard pattern with
 * extra info. If full path is true then the path will be
 * included in the name of each file.  You can free the
 * resulting list with slFreeList. */
struct fileInfo *listDirX(char *dir, char *pattern, boolean fullPath);

/* Return current directory. */
char *getCurrentDir();

/* Set current directory.  Return FALSE if it fails. */
boolean setCurrentDir(char *newDir);

/* Make dir.  Returns TRUE on success.  Returns FALSE
 * if failed because directory exists.  Prints error
 * message and aborts on other error. */
boolean makeDir(char *dirName);

/* 1000 hz clock */
long clock1000();

/* A 1 hz clock. */
long clock1();

/* Make a temp name that's almost certainly unique. */
char *rTempName(char *dir, char *base, char *suffix);

/* This structure helps us generate temp names and use
 * them.  Since different servers locate where the cgi
 * runs from differently, and where the generated html
 * file runs from - this is necessary for portable code. */
struct tempName
	{
	char forCgi[128];
	char forHtml[128];
	};

/* Make a good name for a temp file. */
void makeTempName(struct tempName *tn, char *base, char *suffix);

/* Return directory to look for cgi in. */
char *cgiDir();

/* Return cgi suffix. */
char *cgiSuffix();

/* Return relative speed of machine.  UCSC CSE dept. 1999 web server is 1.0 */
double machineSpeed();

/* Return host computer on network for mySQL database. */
char *mysqlHost();

/* Get name of this machine. */
char *getHost();

/* Invoke the debugger. */
void uglyfBreak();

#endif /* PORTABLE_H */

