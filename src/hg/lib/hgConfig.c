#include <stdio.h>

#include "common.h"
#include "hash.h"
#include "cheapcgi.h"
#include <sys/types.h>
#include <sys/stat.h>

/* the file to read the global configuration info from */
#define GLOBAL_CONFIG_PATH "."
#define GLOBAL_CONFIG_FILE "hg.conf"
//#define GLOBAL_CONFIG_FILE "/usr/local/apache/cgi-bin/hg.conf"
/* the file to read the user configuration info from, starting at the user's home */
#define USER_CONFIG_FILE ".hg.conf"
/* the line buffer size */
#define BUFFER_SIZE 128

/* the hash holding the config options */
static struct hash* cfgOptionsHash = 0;

static void initConfig()
/* create and initilize the config hash */
{
FILE* file;
char filename[BUFFER_SIZE];
char line[BUFFER_SIZE];
char name[BUFFER_SIZE];
char value[BUFFER_SIZE];
struct stat statBuf;

cfgOptionsHash = newHash(6);

/* this long complicated test is needed because, cgiSpoof may have already been called
 * thus we have to look a little deeper to seem if were are really a cgi
 * we do this looking for cgiSpoof in the QUERY_STRING, if it exists*/
if(!cgiIsOnWeb() ||	/* not a cgi, read from home director, e.g. ~/.hg.conf */
		(getenv("QUERY_STRING") != 0 && strstr(getenv("QUERY_STRING"), "cgiSpoof") != 0))
    {
    sprintf(filename, "%s/%s", getenv("HOME"), USER_CONFIG_FILE);
    /* ensure that the file only readable by the user */
    if (stat(filename, &statBuf) == 0)
	{
	if ((statBuf.st_mode & (S_IRWXG|S_IRWXO)) != 0)
	    errAbort("config file %s allows group or other access, must only allow user access",
		     filename);
	}
    }
else	/* on the web, read from global config file */
    {
    sprintf(filename, "%s/%s", GLOBAL_CONFIG_PATH, GLOBAL_CONFIG_FILE);
    }

/* parse; if the file is not there or can't be read, leave the hash empty */
if((file = fopen(filename, "r")) != 0)
    {
    /* while there are lines to read */
    while(fgets(line, BUFFER_SIZE, file))
	{
	/* if it's not a comment */
	if(line[0] != '#')
	    {
	    /* read the key/value pair */
	    if (sscanf(line, "%[^=]=%[^\n]", name, value) == 2)
		hashAdd(cfgOptionsHash, name, cloneString(value));
	    }
	}
    }
}

char* cfgOption(char* name)
/* Return the option with the given name. */
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
