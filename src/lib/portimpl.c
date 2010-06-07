/* Implementation file for some portability stuff mostly aimed
 * at making the same code run under different web servers.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "htmshell.h"
#include "portable.h"
#include "obscure.h"
#include "portimpl.h"
#include <dirent.h>

static char const rcsid[] = "$Id: portimpl.c,v 1.16 2010/04/21 19:23:47 galt Exp $";

static struct webServerSpecific *wss = NULL;

static void setupWss()
{
if (wss == NULL)
    {
    char *s = getenv("SERVER_SOFTWARE");
    wss = &wssDefault;
    if (s == NULL)
        {
	wss = &wssCommandLine;
        }
    else
        {
        if (strncmp(wssMicrosoftII.name, s, strlen(wssMicrosoftII.name)) == 0)
            wss = &wssMicrosoftII;
        else if (strncmp(wssMicrosoftPWS.name, s, strlen(wssMicrosoftPWS.name)) == 0)
            wss = &wssMicrosoftPWS;
	else 
	    {
	    char *t = getenv("HTTP_HOST");
	    if (t != NULL)
		{
		if (sameWord(t, "Crunx"))
		    wss = &wssLinux;
		else if (endsWith(t, "brc.mcw.edu"))
		    wss = &wssBrcMcw;
		}
	    }
        }
    }
}

void makeTempName(struct tempName *tn, char *base, char *suffix)
/* Figure out a temp name, and how CGI and HTML will access it. */
{
setupWss();
wss->makeTempName(tn,base,suffix);
}

char *cgiDir()
{
setupWss();
return wss->cgiDir();
}

char *trashDir()
/* Return the relative path to trash directory for CGI binaries */
{
setupWss();
return wss->trashDir();
}

double machineSpeed()
/* Return relative speed of machine.  UCSC CSE dept. 1999 web server is 1.0 */
{
setupWss();
return wss->speed();
}

void envUpdate(char *name, char *value)
/* Update an environment string */
{
int size = strlen(name) + strlen(value) + 2;
char *s = needMem(size);
safef(s, size, "%s=%s", name, value);
putenv(s);
}

void mkdirTrashDirectory(char *prefix)
/*	create the specified trash directory if it doesn't exist */
{
struct stat buf;
char trashDirName[128];
safef(trashDirName, sizeof(trashDirName), "%s/%s", trashDir(), prefix);
if (stat(trashDirName,&buf))
    {
    int result = mkdir (trashDirName, S_IRWXU | S_IRWXG | S_IRWXO);
    if (0 != result)
	errnoAbort("failed to create directory %s", trashDirName);
    }
}


void makeDirsOnPath(char *pathName)
/* Create directory specified by pathName.  If pathName contains
 * slashes, create directory at each level of path if it doesn't
 * already exist.  Abort with error message if there's a problem.
 * (It's not considered a problem for the directory to already
 * exist. ) */
{
/* Save a copy of current directory. */
char *curDir = cloneString(getCurrentDir());

/* Return current directory.  Abort if it fails. */
/* Make local copy of pathName. */
int len = strlen(pathName);
char pathCopy[len+1];
strcpy(pathCopy, pathName);

/* Start at root if it's an absolute path name. */
char *s = pathCopy, *e;
if (pathCopy[0] == '/')
    {
    setCurrentDir("/");
    ++s;
    }

/* Step through it one slash at a time - changing directory if possible
 * else making directory if possible, else dying. */
for (; !isEmpty(s); s = e)
    {
    /* Find end of this section and terminate string there. */
    e = strchr(s, '/');
    if (e != NULL)
       *e++ = 0;

    if (s[0]!=0) /* Tolerate double-slash in path, everyone else does it. */
	/* Cd there.  If that fails mkdir there and then cd there. */
	if (!maybeSetCurrentDir(s))
	    {
	    if (!makeDir(s))
		{
		break;
		}
	    setCurrentDir(s);
	    }
    }
setCurrentDir(curDir);
freeMem(curDir);
}

