/* gbConf -  print contents of genbank.conf file for debugging purposes */
#include "common.h"
#include "options.h"
#include "hash.h"
#include "gbConf.h"
#include <regex.h>

static char const rcsid[] = "$Id: gbConf.c,v 1.2 2008/06/04 19:30:44 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"defaulted", OPTION_BOOLEAN},
    {NULL, 0}
};

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
         "   -defaulted - fill in defaults\n",
         msg);
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
char prefix[65];
char *dot = strchr(confEl->name, '.');
if (dot == NULL)
    prefix[0] = '\0';  // empty string if no dot
else
    safencpy(prefix, sizeof(prefix), confEl->name, (dot-confEl->name));

struct prefixElems *prefixElems = prefixElemsObtain(prefixMap, prefix);
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

static void gbConfDump(FILE *fh, char *confFile, boolean defaulted)
/* dump contents of genbank.conf file in different ways */
{
struct gbConf *conf = gbConfNew(confFile);
if (defaulted)
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

gbConfDump(stdout, argv[1], optionExists("defaulted"));

return 0;
}



/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
