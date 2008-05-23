#include <stdio.h>

static char const rcsid[] = "$Id: hgConfig.c,v 1.16 2008/05/23 22:14:58 angie Exp $";

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include "portable.h"
#include "linefile.h"
#include <sys/types.h>
#include <sys/stat.h>

/* the file to read the global configuration info from */
#define GLOBAL_CONFIG_PATH "."
#define GLOBAL_CONFIG_FILE "hg.conf"
//#define GLOBAL_CONFIG_FILE "/usr/local/apache/cgi-bin/hg.conf"
/* the file to read the user configuration info from, starting at the user's home */
#define USER_CONFIG_FILE ".hg.conf"

/* the hash holding the config options */
static struct hash* cfgOptionsHash = 0;

static void getConfigFile(char filename[PATH_LEN])
/* get path to .hg.conf file to use */
{
/* this long complicated test is needed because, cgiSpoof may have already
 * been called thus we have to look a little deeper to seem if were are really
 * a cgi we do this looking for cgiSpoof in the QUERY_STRING, if it exists.
 * If not a cgi, read from home directory, e.g. ~/.hg.conf */
#ifndef GBROWSE
if(!cgiIsOnWeb() ||
   (getenv("QUERY_STRING") != 0 && strstr(getenv("QUERY_STRING"), "cgiSpoof") != 0))
    {
    struct stat statBuf;
    /* Check for explictly specified file in env, otherwise use one in home */
    if (getenv("HGDB_CONF") != NULL)
        strcpy(filename, getenv("HGDB_CONF"));
    else
        safef(filename, PATH_LEN, "%s/%s",
	      getenv("HOME"), USER_CONFIG_FILE);
    /* ensure that the file only readable by the user */
    if (stat(filename, &statBuf) == 0)
	{
	if ((statBuf.st_mode & (S_IRWXG|S_IRWXO)) != 0)
	    errAbort("config file %s allows group or other access, must only allow user access",
		     filename);
	}
    }
else	/* on the web, read from global config file */
#endif /* GBROWSE */
    {
    safef(filename, PATH_LEN, "%s/%s",
	  GLOBAL_CONFIG_PATH, GLOBAL_CONFIG_FILE);
    }
}

static void initConfig()
/* create and initilize the config hash */
{
struct lineFile *lf;
char filename[PATH_LEN];
char *line, *name, *value;

cfgOptionsHash = newHash(6);

getConfigFile(filename);

/* parse; if the file is not there or can't be read, leave the hash empty */
if((lf = lineFileMayOpen(filename, TRUE)) != 0)
    {
    /* while there are lines to read */
    while(lineFileNext(lf, &line, NULL))
	{
	/* if it's not a comment */
	if(line[0] != '#')
	    {
	    /* parse the key/value pair */
            value = strchr(line, '=');
	    if (value != NULL)
                {
                *value++ = '\0';
                name = trimSpaces(line);
                value = trimSpaces(value);
		hashAdd(cfgOptionsHash, name, cloneString(value));
                /* Set enviroment variables to enable sql tracing and/or profiling */
                if (sameString(name, "JKSQL_TRACE") || sameString(name, "JKSQL_PROF"))
                    envUpdate(name, value);
                }
	    }
	}
    lineFileClose(&lf);
    }
}

char* cfgOption(char* name)
/* Return the option with the given name.  Return NULL if
 * it doesn't exist. */
{
/* initilize the config hash if it is not */
if(cfgOptionsHash == NULL)
    initConfig();
return hashFindVal(cfgOptionsHash, name);
}

char* cfgOptionDefault(char* name, char* def)
/* Return the option with the given name or the given default 
 * if it doesn't exist. */
{
char *val = cfgOption(name);
if (val == NULL)
    val = def;
return val;
}

char *cfgVal(char *name)
/* Return option with given name.  Squawk and die if it
 * doesn't exist. */
{
char *val = cfgOption(name);
if (val == NULL)
    errAbort("%s doesn't exist in hg.conf file", name);
return val;
}

unsigned long cfgModTime()
/* Return modification time of config file */
{
char filename[PATH_LEN];
getConfigFile(filename);
return fileModTime(filename);
}
