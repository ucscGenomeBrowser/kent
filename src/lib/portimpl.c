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

static char const rcsid[] = "$Id: portimpl.c,v 1.13 2006/06/19 23:15:09 hiram Exp $";

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
DIR *dirOpen;
char trashDirName[128];
safef(trashDirName, sizeof(trashDirName), "%s/%s", trashDir(), prefix);
dirOpen = opendir(trashDirName);
if ((DIR *)NULL == dirOpen)
    {
    int result = mkdir (trashDirName, S_IRWXU | S_IRWXG | S_IRWXO);
    if (0 != result)
	errnoAbort("failed to create directory %s", trashDirName);
    }
else
    closedir(dirOpen);
}
