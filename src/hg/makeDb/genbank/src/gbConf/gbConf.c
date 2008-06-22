/* gbConf -  print contents of genbank.conf file for debugging purposes */
#include "common.h"
#include "options.h"
#include "verbose.h"
#include "hash.h"
#include "gbConf.h"
#include "portable.h"
#include <regex.h>

static char const rcsid[] = "$Id: gbConf.c,v 1.3 2008/06/22 19:22:01 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"defaulted", OPTION_BOOLEAN},
    {"serverFileCheck", OPTION_BOOLEAN},
    {"clusterFileCheck", OPTION_BOOLEAN},
    {"checkClusterMaster", OPTION_BOOLEAN},
    {NULL, 0}
};
static int errCnt = 0; // count of errors encountered

static void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n\n"
         "gbConf [options] genebank.conf\n"
         "\n"
         "Print contents of genbank.conf with variables expanded for debugging purposes.\n"
         "The results will not contain comments and will be alphabetically sorted\n"
         "by item name.\n"
         "\n"
         "Options:\n"
         "   -defaulted - fill in defaults\n"
         "   -serverFileCheck - check for existence of files that must be\n"
         "    on the build server.\n"
         "   -clusterFileCheck - check for existence of files that must be\n"
         "    on the cluster.\n"
         "   -checkClusterMaster - check cluster master directory for cluster files:\n"
         "       /cluster/bluearc/scratch/\n"
         "   -verbose=n  n >=2: print files that are present when doing checks\n",
         msg);
}

static char *parsePrefix(char *varName)
/* parse prefix before first dot.  WARNING: static return */
{
static char prefix[65];
char *dot = strchr(varName, '.');
if (dot == NULL)
    prefix[0] = '\0';  // empty string if no dot
else
    safencpy(prefix, sizeof(prefix), varName, (dot-varName));
return prefix;
}

static regex_t *mkGenomeDbRe()
/* build a regular expression to match genome databases */
{
regex_t *re;
AllocVar(re);
if (regcomp(re, "^[a-zA-Z]+[0-9]+$", REG_EXTENDED|REG_NOSUB) != 0)
    errAbort("mkGenomeDbRe: can't compile regexp");
return re;
}

static boolean isGenomeDb(char *str)
/* does a string looks like a genomeDb */
{
static regex_t *re = NULL;
if (re == NULL)
    re = mkGenomeDbRe();
return (regexec(re, str, 0, NULL, 0) == 0);
}

static struct slName *getGenomeDbs(struct gbConf *conf)
/* get list of genome databases from variable names */
{
// build hash of dbs
struct hash *dbSet = hashNew(20);
struct hashCookie cookie = hashFirst(conf->hash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    char *prefix = parsePrefix(hel->name);
    if (isGenomeDb(prefix))
        hashStore(dbSet, prefix);
    }

// convert to a list of dbs
struct slName *dbs = NULL;
cookie = hashFirst(dbSet);
while ((hel = hashNext(&cookie)) != NULL)
    slSafeAddHead(&dbs, slNameNew(hel->name));
hashFree(&dbSet);
slSort(&dbs, slNameCmp);
return dbs;
}

static struct hashEl *hashElCloneList(struct hashEl *hels)
/* clone a list of hashEl objects  */
{
struct hashEl *hels2 = NULL, *hel;
for (hel = hels; hel != NULL; hel = hel->next)
    {
    slSafeAddHead(&hels2, CloneVar(hel));
    }
slReverse(&hels2);
return hels2;
}

static void printGbConf(FILE *fh, struct gbConf *conf)
/* Print contents of genbank.conf file for debugging purposes */
{
struct hashEl *confEls = hashElListHash(conf->hash);
struct hashEl *confEl;

slSort(&confEls, hashElCmp);
for (confEl = confEls; confEl != NULL; confEl = confEl->next)
    fprintf(fh, "%s = %s\n", confEl->name, (char*)confEl->val);

hashElFreeList(&confEls);
}

struct prefixElems
/* entries in the gbConf for a given prefix */
{
    struct byPrefix *next;
    char *prefix;            // prefix before first `.'
    struct hashEl *elems;
};

static struct prefixElems *prefixElemsNew(char *prefix)
/* construct a new prefixElems */
{
struct prefixElems *pe;
AllocVar(pe);
pe->prefix = cloneString(prefix);
return pe;
}

static struct prefixElems *prefixElemsObtain(struct hash *prefixMap,
                                             char *prefix)
/* construct a new prefixElems */
{
struct hashEl *hashEl = hashStore(prefixMap, prefix);
if (hashEl->val == NULL)
    hashEl->val = prefixElemsNew(prefix);
return hashEl->val;
}

static void addByPrefix(struct hash *prefixMap,
                        struct hashEl *confEl)
/* add an entry by prefix */
{
struct prefixElems *prefixElems = prefixElemsObtain(prefixMap, parsePrefix(confEl->name));
slAddHead(&prefixElems->elems, confEl);
}

static struct hash* splitByPrefix(struct gbConf *conf)
/* split by prefix in to has of prefixElems  */
{
struct hash *prefixMap = hashNew(18);
struct hashEl *elems = hashElListHash(conf->hash);
struct hashEl *confEl;
while ((confEl = slPopHead(&elems)) != NULL)
    addByPrefix(prefixMap, confEl);
return prefixMap;
}

static boolean haveExplictDef(struct prefixElems *dbElems,
                              struct hashEl *defEl)
/* is this default in a genome's defs? */
{
static int defaultPreSize = 7;  // strlen("default")
struct hashEl *dbElem;
for (dbElem = dbElems->elems; dbElem != NULL; dbElem = dbElem->next)
    {
    if (endsWith(dbElem->name, defEl->name+defaultPreSize))
        return TRUE;
    }
return FALSE;
}

static struct hashEl *mkDefault(struct prefixElems *dbElems,
                                struct hashEl *defEl)
/* generate a database-specific value from a default */
{
char name[128];
safecpy(name, sizeof(name), dbElems->prefix);
safecat(name, sizeof(name), strchr(defEl->name, '.'));

struct hashEl *dbDefEl;
AllocVar(dbDefEl);
dbDefEl->name = cloneString(name);
dbDefEl->val = defEl->val;
return dbDefEl;
}

static struct hashEl *genomeDbGetDefaults(struct prefixElems *dbElems,
                                          struct prefixElems *defaultElems)
/* Get defaults for missing values in a genome */
{
struct hashEl *confEls = NULL;
struct hashEl *defEl;
for (defEl = defaultElems->elems; defEl != NULL; defEl = defEl->next)
    {
    if (!haveExplictDef(dbElems, defEl))
        slSafeAddHead(&confEls, mkDefault(dbElems, defEl));
    }
return confEls;
}

static struct hashEl *buildDefaulted(struct gbConf *conf)
/* build list of conf elements, with defaults filled in. A bit of work and
 * guessing because the default mechanism is designed around explicitly asking
 * for values */
{
struct hash* prefixMap = splitByPrefix(conf);
struct prefixElems *defaultElems = hashMustFindVal(prefixMap, "default");
struct hashCookie cookie = hashFirst(prefixMap);
struct hashEl *prefixMapEl;
struct hashEl *confEls = NULL;

// build list of entries
while ((prefixMapEl = hashNext(&cookie)) != NULL)
    {
    struct prefixElems *prefixElems = prefixMapEl->val;
    confEls = slCat(confEls, hashElCloneList(prefixElems->elems));
    if (isGenomeDb(prefixElems->prefix))
        confEls = slCat(confEls, genomeDbGetDefaults(prefixElems, defaultElems));
    }
return confEls;
}

static void printGbConfDefaults(FILE *fh, struct gbConf *conf)
/* Print contents of genbank.conf with defaults filled in */
{
struct hashEl *confEls = buildDefaulted(conf);
struct hashEl *confEl;
slSort(&confEls, hashElCmp);
for (confEl = confEls; confEl != NULL; confEl = confEl->next)
    fprintf(fh, "%s = %s\n", confEl->name, (char*)confEl->val);
}

static boolean matchingFilesExist(char *val, char *replacePre, char *replaceVal)
/*  check for existing files specified by variable value. "no", or
 * missing files returns false */
{
if (sameString(val, "no"))
    return FALSE;
char path[PATH_LEN];

if ((replacePre != NULL) && startsWith(replacePre, val))
    {
    safecpy(path, sizeof(path), replaceVal);
    safecat(path, sizeof(path), val+strlen(replacePre));
    }
else
    safecpy(path, sizeof(path), val);

char *slash = strrchr(path, '/');
if (slash == NULL)
    return fileExists(path);

*slash = '\0';
struct slName *hits = listDir(path, slash+1);
boolean have = (hits != NULL);
slFreeList(&hits);
return have;
}

static void checkForDbFile(struct gbConf *conf, char *db, char *baseName,
                           boolean varRequired, boolean fileRequired,
                           char *replacePre, char *replaceVal)
/* check for existing of a file in the genome db with the specified
 * base name.  Can be glob patterns or simple file names. */
{
char *val = gbConfGetDb(conf, db, baseName);
if (val == NULL)
    {
    if (varRequired)
        {
        fprintf(stderr, "Error: missing required variable: %s.%s\n", db, baseName);
        errCnt++;
        }
    }
else if (!matchingFilesExist(val, replacePre, replaceVal))
    {
    if (fileRequired)
        {
        fprintf(stderr, "Error: missing required files: %s.%s = %s\n",
                db, baseName, val);
        errCnt++;
        }
    else
        verbose(2, "optional files don't exist: %s.%s = %s\n",
                db, baseName, val);
    }
else
    {
    verbose(2, "%s files exist: %s.%s = %s\n", (fileRequired ? "required" : "optional"),
            db, baseName, val);
    }
}

static void checkServerFiles(struct slName *dbs, struct gbConf *conf)
/* check for server files */
{
struct slName *db;
for (db = dbs; db != NULL; db = db->next)
    {
    checkForDbFile(conf, db->name, "serverGenome", TRUE, TRUE, NULL, NULL);
    checkForDbFile(conf, db->name, "lift", TRUE, FALSE, NULL, NULL);
    checkForDbFile(conf, db->name, "hapRegions", FALSE, TRUE, NULL, NULL);
    }
}

static void checkClusterFiles(struct slName *dbs, struct gbConf *conf,
                              boolean checkClusterMaster)
/* check for cluster files */
{
char *replacePre = NULL, *replaceVal = NULL;
if (checkClusterMaster)
    {
    replacePre = "/scratch/";
    replaceVal = "/cluster/bluearc/scratch/";
    }

struct slName *db;
for (db = dbs; db != NULL; db = db->next)
    {
    checkForDbFile(conf, db->name, "clusterGenome", TRUE, TRUE, replacePre, replaceVal);
    checkForDbFile(conf, db->name, "ooc", TRUE, FALSE, replacePre, replaceVal);
    }
}
static void checkFiles(struct gbConf *conf, boolean serverFileCheck, boolean clusterFileCheck,
                       boolean checkClusterMaster)
/* check for existence of files specified in variables */
{
struct slName *dbs = getGenomeDbs(conf);
if (serverFileCheck)
    checkServerFiles(dbs, conf);
if (clusterFileCheck || checkClusterMaster)
    checkClusterFiles(dbs, conf, checkClusterMaster);
slFreeList(&dbs);
}

static void gbConfDump(FILE *fh, char *confFile, boolean defaulted,
                       boolean serverFileCheck, boolean clusterFileCheck,
                       boolean checkClusterMaster)
/* dump contents of genbank.conf file in different ways, including checking
 * for existence of certain files. */
{
struct gbConf *conf = gbConfNew(confFile);
if (serverFileCheck || clusterFileCheck  || checkClusterMaster)
    checkFiles(conf, serverFileCheck, clusterFileCheck, checkClusterMaster);
else if (defaulted)
    printGbConfDefaults(fh, conf);
else
    printGbConf(fh, conf);
gbConfFree(&conf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 2)
    usage("wrong # args");

gbConfDump(stdout, argv[1], optionExists("defaulted"),
           optionExists("serverFileCheck"), optionExists("clusterFileCheck"),
           optionExists("checkClusterMaster"));
return ((errCnt == 0) ? 0 : 1);
}
