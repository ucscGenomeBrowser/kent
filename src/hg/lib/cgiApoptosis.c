/* functions to auto-kill CGIs after a certain amount of time has passed */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include <utime.h>
#include <signal.h>
#include "htmlPage.h"
#include "hgConfig.h"
#include "sqlNum.h"
#include "jksql.h"
#include "hdb.h"
#include "portable.h"
#include "cheapcgi.h"

static unsigned long expireSeconds = 0;
static boolean lazarus = FALSE;

void lazarusLives(unsigned long newExpireSeconds)
/* Long running process requests more time */
{
lazarus = TRUE;
expireSeconds = newExpireSeconds;
}

static void cgiApoptosisHandler(int status)
/* signal handler for SIGALRM for CGI expiration */
{
(void)(status); // skip gcc warning "unused variable"
if (lazarus)
    {
    (void) alarm(expireSeconds);    /* CGI timeout */
    lazarus = FALSE;
    return;
    }
if (expireSeconds > 0)
    {
    /* want to see this error message in the apache error_log also */
    fprintf(stderr, "cgiApoptosis: %lu seconds\n", expireSeconds);
    /* most of our CGIs post a polite non-fatal message with this errAbort */
    errAbort("procedures have exceeded timeout: %lu seconds, function has ended.\n", expireSeconds);
    }
exit(0);
}

void cgiApoptosisSetup()
/* setting up cgi auto killing after x minutes. 
 * This must be called before any mysql connections have been setup, 
 * as otherwise the child thread will close all mysql connections for 
 * the parent. */
{
static boolean beenHere = FALSE;
if (beenHere)  /* one at a time please */
    return;
beenHere = TRUE;

char *expireTime = cfgOptionDefault("browser.cgiExpireMinutes", "20");
unsigned expireMinutes = sqlUnsigned(expireTime);
expireSeconds = expireMinutes * 60;

char trashFile[PATH_LEN];
safef(trashFile, sizeof(trashFile), "%s/registration.txt", trashDir());

/* trashFile does not exist during command line execution */
if(fileExists(trashFile))	/* update access time for trashFile */
    {
    struct utimbuf ut;
    struct stat mystat;
    ZeroVar(&mystat);
    if (stat(trashFile,&mystat)==0)
	{
	ut.actime = clock1();
	ut.modtime = mystat.st_mtime;
	}
    else
	{
	ut.actime = ut.modtime = clock1();
	}
    (void) utime(trashFile, &ut);
    if (expireSeconds > 0)
	{
	(void) signal(SIGALRM, cgiApoptosisHandler);
	(void) alarm(expireSeconds);	/* CGI timeout */
	}
    return;
    }

char *scriptName = cgiScriptName();
char *ip = getenv("SERVER_ADDR");
if (scriptName && ip)  /* will not be true from command line execution */
    {
    FILE *f = fopen(trashFile, "w");
    if (f)		/* rigamarole only if we can get a trash file */
	{
	time_t now = time(NULL);
	char *localTime;
	extern char *tzname[2];
	struct tm *tm = localtime(&now);
	localTime = sqlUnixTimeToDate(&now,FALSE); /* FALSE == localtime */
	fprintf(f, "%s, %s, %s %s, %s\n", scriptName, ip, localTime,
	    tm->tm_isdst ? tzname[1] : tzname[0], trashFile);
	fclose(f);

	chmod(trashFile, 0666);
	pid_t pid0 = fork();
	if (0 == pid0)	/* in child */
	    {
	    close(STDOUT_FILENO); /* do not hang up Apache finish for parent, but apache will still wait */
	    expireSeconds = 0;	/* no error message from this exit */
	    (void) signal(SIGALRM, cgiApoptosisHandler);
	    (void) alarm(6);	/* timeout here in 6 seconds */
#include "versionInfo.h"
	    char url[1024];
            char *browserName = "browser.v";
            if (hIsBrowserbox())
                browserName = "browserbox.v";
            if (hIsGbic())
                browserName = "browserGbic.v";

	    safef(url, sizeof(url), "%s%s%s%s%s%s%s", "http://",
	"genomewiki.", "ucsc.edu/", "cgi-bin/useCount?", "version=", browserName,
		CGI_VERSION);

	    /* 6 second alarm will exit this page fetch if it does not work */
	    (void) htmlPageGetWithCookies(url, NULL); /* ignore return */

            exit(0);
            }	/* child of fork has done exit(0) normally or via alarm */
	}		/* trash file open OK */
    if (expireSeconds > 0)
	{
	(void) signal(SIGALRM, cgiApoptosisHandler);
	(void) alarm(expireSeconds);	/* CGI timeout */
	}
    }			/* an actual CGI binary */
}			/* phoneHome()	*/

