/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "portable.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

static char *cdwPrefix = "cdw";
static struct optionSpec options[] = {
   {"prod", OPTION_BOOLEAN},
};

void getDbNameForWeb(char *httpHost, char *prefix, char *buf, int bufSize)
/* Parse out -user from hgwdev-user.this.that or cirm-01-user */
{
// Figure out starts and end of user name within string.
int prefixLen = strlen(prefix);
char *userStart = httpHost + prefixLen;
char *userEnd = strchr(userStart, '.');
if (userEnd == NULL)
     userEnd = userStart + strlen(userStart);  // Point to end of string if no . after user


// Copy start to end into a new string and zero terminate it
int userSize = userEnd - userStart;
char user[userSize+1];
memcpy(user, userStart, userSize);
user[userSize] = 0;

// Construct database name
safef(buf, bufSize, "%s_%s", cdwPrefix, user);
}

char *cdwGetDbName(char *buf, int bufSize)
/* Return the database name to use depending on whether we are in sandbox or not. 
 * buf and bufSize should be big enough to hold the result,  say 128 chars or so. */
{
char *httpHost = getenv("HTTP_HOST");
if (httpHost == NULL)  // Command line
    {
    if (optionExists("prod"))
        safef(buf, bufSize, "%s", cdwPrefix);
    else
	{
	char *user = getenv("USER");
	assert(user != NULL);
	safef(buf, bufSize, "%s_%s", cdwPrefix, user);
	}
    }
else
    {
    char *devPrefix = "hgwdev-";
    char *prodPrefix = "cirm-01-";
    if (startsWith(devPrefix, httpHost)) 
        {
	getDbNameForWeb(httpHost, devPrefix, buf, bufSize);
	}
    else if (startsWith(prodPrefix, httpHost))
        {
	getDbNameForWeb(httpHost, prodPrefix, buf, bufSize);
	}
    else
        {
	safef(buf, sizeof(buf), "%s", cdwPrefix);
	}
    }
return buf;
}

void freen(char *fileName)
/* Test something */
{
char buf[128];
printf("As command line %s\n", cdwGetDbName(buf, sizeof(buf)));
setenv("HTTP_HOST", "hgwdev-kent.cse.ucsc.edu", TRUE);
printf("As web in sandbox %s\n", cdwGetDbName(buf, sizeof(buf)));
setenv("HTTP_HOST", "hgwdev.cse.ucsc.edu", TRUE);
printf("As web in alpha site %s\n", cdwGetDbName(buf, sizeof(buf)));
setenv("HTTP_HOST", "cirm-01", TRUE);
printf("As web in production site %s\n", cdwGetDbName(buf, sizeof(buf)));
setenv("HTTP_HOST", "cirm-01-ceisenhart", TRUE);
printf("As web in Chris's production sandbox %s\n", cdwGetDbName(buf, sizeof(buf)));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}

