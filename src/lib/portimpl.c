/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
#include "common.h"
#include "htmshell.h"
#include "portable.h"
#include "obscure.h"
#include "portimpl.h"

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

char *cgiSuffix()
{
setupWss();
return wss->cgiSuffix();
}
    
double machineSpeed()
/* Return relative speed of machine.  UCSC CSE dept. 1999 web server is 1.0 */
{
setupWss();
return wss->speed();
}

boolean fileExists(char *fileName)
/* Return TRUE if file exists (may replace this with non-
 * portable faster way some day). */
{
int fd;
if ((fd = open(fileName, O_RDONLY)) < 0)
    return FALSE;
close(fd);
return TRUE;
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
}

