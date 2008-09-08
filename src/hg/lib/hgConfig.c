#include <stdio.h>

static char const rcsid[] = "$Id: hgConfig.c,v 1.20 2008/09/08 18:20:37 markd Exp $";

#include "common.h"
#include "hgConfig.h"
#include "hash.h"
#include "cheapcgi.h"
#include "portable.h"
#include "linefile.h"
#include "customTrack.h"
#include <sys/types.h>
#include <sys/stat.h>

/* the file to read the global configuration info from */
#define GLOBAL_CONFIG_PATH "."
#define GLOBAL_CONFIG_FILE "hg.conf"
//#define GLOBAL_CONFIG_FILE "/usr/local/apache/cgi-bin/hg.conf"
/* the file to read the user configuration info from, starting at the user's home */
#define USER_CONFIG_FILE ".hg.conf"

// forwards
static void parseConfigFile(char *filename, int depth);

/* the hash holding the config options */
static struct hash* cfgOptionsHash = 0;

static boolean isBrowserCgi()
/* test if this is a browser CGI */
{
#ifndef GBROWSE
/* this long complicated test is needed because, cgiSpoof may have already
 * been called thus we have to look a little deeper to seem if were are really
 * a cgi we do this looking for cgiSpoof in the QUERY_STRING, if it exists.
 * If not a cgi, read from home directory, e.g. ~/.hg.conf */
static boolean firstTime = TRUE;
static boolean result = FALSE;
if (firstTime)
    {
    result = ((cgiIsOnWeb()) && !((getenv("QUERY_STRING") != NULL) && (strstr(getenv("QUERY_STRING"), "cgiSpoof") != NULL)));
    firstTime = FALSE;
    }
return result;
#else
return FALSE;
#endif
}

static void checkConfigPerms(char *filename)
/* get that we are either a CGI or that the config file is only readable by 
 * the user, or doesn't exist.  Specifying HGDB_CONF also disables perms
 * check to make debugging and having CGIs run loaders easier */
{
struct stat statBuf;
if ((!isBrowserCgi()) && isEmpty(getenv("HGDB_CONF"))
    && (stat(filename, &statBuf) == 0))
    {
    if ((statBuf.st_mode & (S_IRWXG|S_IRWXO)) != 0)
        errAbort("config file %s allows group or other access, must only allow user access",
                 filename);
    }
}

static void getConfigFile(char filename[PATH_LEN])
/* get path to .hg.conf file to use */
{
if (!isEmpty(getenv("HGDB_CONF")))
    {
    // use environment variable
    strcpy(filename, getenv("HGDB_CONF"));
    }
else if (isBrowserCgi())
    {
    /* on the web, read from global config file */
    safef(filename, PATH_LEN, "%s/%s",
	  GLOBAL_CONFIG_PATH, GLOBAL_CONFIG_FILE);
    }
else
    {
    /*use one in home */
    safef(filename, PATH_LEN, "%s/%s",
          getenv("HOME"), USER_CONFIG_FILE);
    }
}

static void parseConfigInclude(struct lineFile *lf, int depth, char *line)
/* open and parse an included config file */
{
if (depth > 10)
    errAbort("maximum config include depth exceeded: %s:%d",
             lf->fileName, lf->lineIx);

// parse out file name
char line2[PATH_LEN+64];
safecpy(line2, sizeof(line2), line);  // copy so we can make a useful error message
char *p = line2;
(void)nextWord(&p);
char *relfile = nextWord(&p);
if ((relfile == NULL) || (p != NULL))
    errAbort("invalid format for config include: %s:%d: %s",
             lf->fileName, lf->lineIx, line);

// construct relative to current file path, unless include
// file is absolute
char incfile[PATH_LEN];
safecpy(incfile, sizeof(incfile),  lf->fileName);
char *dirp = strrchr(incfile, '/');
if ((dirp != NULL) && (relfile[0] != '/'))
    {
    // construct relative path
    *(dirp+1) ='\0';
    safecat(incfile, sizeof(incfile), relfile);
    }
else
    {
    // use as-is
    safecpy(incfile, sizeof(incfile),  relfile);
    }
parseConfigFile(incfile, depth+1);
}

static void parseConfigLine(struct lineFile *lf, int depth, char *line)
/* parse one non-comment, non-blank line of the config file.
 * If keyword=value, put in global hash, otherwise process
 * includes. */
{
/* parse the key/value pair */
char *name = trimSpaces(line);
if (startsWithWord("include", line))
    parseConfigInclude(lf, depth, line);
else
    {
    char *value = strchr(name, '=');
    if (value == NULL)
        errAbort("invalid format in config file %s:%d: %s",
                 lf->fileName, lf->lineIx, line);
    *value++ = '\0';
    name = trimSpaces(name);
    value = trimSpaces(value);
    hashAdd(cfgOptionsHash, name, cloneString(value));
    /* Set environment variables to enable sql tracing and/or profiling */
    if (sameString(name, "JKSQL_TRACE") || sameString(name, "JKSQL_PROF"))
        envUpdate(name, value);
    }
}

static void parseConfigFile(char *filename, int depth)
/* open and parse a config file */
{
checkConfigPerms(filename);
struct lineFile *lf = lineFileOpen(filename, TRUE);
char *line;
while(lineFileNext(lf, &line, NULL))
    {
    // check for comment or blank line
    char *p = skipLeadingSpaces(line);
    if (!((p[0] == '#') || (p[0] == '\0'))) 
        parseConfigLine(lf, depth, line);
    }
lineFileClose(&lf);
}

static void addConfigIfUndef(char *prefix, char *suffix, char *value)
/* if the specified config item doesn't exist, add it */
{
char name[256];
safef(name, sizeof(name), "%s.%s", prefix, suffix);
if (cfgOption(name) == NULL)
    hashAdd(cfgOptionsHash, name, cloneString(value));
}

static boolean haveProfile(char *profileName)
/* see if we appear to have the profile.  Can't do this in
 * actual profile code, since it wants the config object that is being
 * hacked */
{
return (cfgOption2(profileName, "host") != NULL) && (cfgOption2(profileName, "user") != NULL)
    && (cfgOption2(profileName, "password") != NULL);

}


static void hackConfigProfiles()
/* Add in some pre-defined profile mappings if needed.  This was added to make
 * older conf files compatible with the db profile paradigm */
{
/* add mapping of customTrash database to profile if we have customTracks 
 * profile defined */
if (haveProfile(CUSTOM_TRACKS_PROFILE))
    addConfigIfUndef(CUSTOM_TRASH, "profile", CUSTOM_TRACKS_PROFILE);
}

static void initConfig()
/* create and initilize the config hash */
{
char filename[PATH_LEN];
cfgOptionsHash = newHash(6);
getConfigFile(filename);
parseConfigFile(filename, 0);
hackConfigProfiles();
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

char *cfgOptionDefault(char* name, char* def)
/* Return the option with the given name or the given default 
 * if it doesn't exist. */
{
char *val = cfgOption(name);
if (val == NULL)
    val = def;
return val;
}

boolean cfgOptionBooleanDefault(char* name, boolean def)
/* return the boolean value for option with the given name or the given
 * default if it doesn't exist. Booleans of yes/no/on/off/true/false are
 * recognized */
{
char *val = cfgOptionDefault(name, NULL);
if (val == NULL)
    return def;
if (sameString(val, "yes") || sameString(val, "on") || sameString(val, "true"))
    return TRUE;
else if (sameString(val, "no") || sameString(val, "off")|| sameString(val, "false"))
    return FALSE;
else
    {
    errAbort("hg.conf value for %s: \"%s\", expected one of yes, no, on, off, true, false",
             name, val);
    return FALSE;
    }
}

char *cfgOption2(char *prefix, char *suffix)
/* Return the option with the given two-part name, formed from prefix.suffix.
 * Return NULL if it doesn't exist. */
{
/* initilize the config hash if it is not */
if(cfgOptionsHash == NULL)
    initConfig();
char name[256];
safef(name, sizeof(name), "%s.%s", prefix, suffix);
return hashFindVal(cfgOptionsHash, name);
}

char* cfgOptionDefault2(char *prefix, char *suffix, char* def)
/* Return the option with the given two-part name, formed from prefix.suffix,
 * or the given default if it doesn't exist. */
{
char *val = cfgOption2(prefix, suffix);
if (val == NULL)
    val = def;
return val;
}

char *cfgOptionEnv(char *envName, char* name)
/* get a configuration optional value, from either the environment or the cfg
 * file, with the env take precedence.  Return NULL if not found */
{
char *val = getenv(envName);
if (val == NULL || (strlen(val) == 0))
    val = cfgOption(name);
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

struct slName *cfgNames()
/* get list of names in config file. slFreeList when finished */
{
if(cfgOptionsHash == NULL)
    initConfig();
struct slName *names = NULL;
struct hashCookie cookie = hashFirst(cfgOptionsHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    slSafeAddHead(&names, slNameNew(hel->name));
return names;
}

unsigned long cfgModTime()
/* Return modification time of config file */
{
char filename[PATH_LEN];
getConfigFile(filename);
return fileModTime(filename);
}
