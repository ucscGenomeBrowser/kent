#include "gbGenome.h"
#include "gbRelease.h"
#include "gbDefs.h"
#include "localmem.h"

static char const rcsid[] = "$Id: gbGenome.c,v 1.3 2003/08/04 07:48:11 markd Exp $";

struct dbToSpecies
/* structure mapping database prefix to species (e.g. hg -> "Homo sapiens") */
{
    char *dbPrefix;           /* prefix of database (e.g. hg) */
    char **names;             /* list of species name, terminate by null.
                               * first name is prefered. */
    char *subSpeciesPrefix;   /* if not null, used to check for subspecies;
                               * should end in a blank */
};

static char *hgNames[] = {"Homo sapiens", NULL};
static char *mmNames[] = {"Mus musculus", "Mus sp.", NULL};
static char *rnNames[] = {"Rattus norvegicus", "Rattus sp.", NULL};
static char *ciNames[] = {"Ciona intestinalis", NULL};

/* species currently unique to zoo */
static char *zooCatNames[] = {"Felis catus", "Felis sp.", NULL};
static char *zooChickenNames[] = {"Gallus gallus", "Gallus sp.", NULL};
static char *zooBaboonNames[] = {"Papio hamadryas", "Papio sp.", NULL};
static char *zooChimpNames[] = {"Pan troglodytes", NULL};
static char *zooCowNames[] = {"Bos taurus", "Bos sp.", NULL};
static char *zooDogNames[] = {"Canis familiaris", "Canis sp.", NULL};
static char *zooFuguNames[] = {"Takifugu rubripes", NULL};
static char *zooPigNames[] = {"Sus scrofa", "Sus sp.", NULL};
static char *zooTetraNames[] = {"Tetraodon nigroviridis", NULL};
static char *zooZebrafishNames[] = {"Danio rerio", NULL};

static char *endNames[] = {NULL};

static struct dbToSpecies dbToSpeciesMap[] = {
    {"hg", hgNames, NULL},
    {"mm", mmNames, "Mus musculus "},
    {"rn", rnNames, "Rattus norvegicus "},
    {"ci", ciNames, NULL},
    {"zooHuman", hgNames, NULL},
    {"zooCat", zooCatNames, NULL},
    {"zooChicken", zooChickenNames, NULL},
    {"zooBaboon", zooBaboonNames, NULL},
    {"zooChimp", zooChimpNames, "Pan troglodytes "},
    {"zooCow", zooCowNames, NULL},
    {"zooDog", zooDogNames, NULL},
    {"zooFugu", zooFuguNames, NULL},
    {"zooMouse", mmNames, "Mus musculus "},
    {"zooPig", zooPigNames, "Sus scrofa "},
    {"zooRat", rnNames, NULL},
    {"zooTetra", zooTetraNames, NULL},
    {"zooZebrafish", zooZebrafishNames, NULL},
    {NULL, endNames, NULL}
};

struct gbGenome* gbGenomeNew(char* database)
/* create a new gbGenome object */
{
struct dbToSpecies* dbMap;
struct gbGenome* genome;

for (dbMap = dbToSpeciesMap; dbMap->dbPrefix != NULL; dbMap++)
    {
    if (startsWith(dbMap->dbPrefix, database))
        break;
    }
if (dbMap->dbPrefix == NULL)
    errAbort("no species defined for database \"%s\"; edit %s to add definition",
             database, __FILE__);

AllocVar(genome);
genome->database = cloneString(database);
genome->organism = dbMap->names[0];
genome->dbMap = dbMap;
return genome;
}

char* gbGenomePreferedOrgName(char* organism)
/* determine the prefered organism name, if this organism is known,
 * otherwise NULL.  Used for sanity checks. Names are in static table,
 * so ptrs can be compared. */
{
/* caching last found helps speed search, since entries tend to be groups,
 * especially ESTs.  NULL is a valid cache entry, so need flag */
static boolean cacheEmpty = TRUE;
static struct dbToSpecies* dbMapCache = NULL;
static char organismCache[256];

if (cacheEmpty || !sameString(organism, organismCache))
    {
    struct dbToSpecies* foundDbMap = NULL;
    int i, j;
    for (i = 0; (dbToSpeciesMap[i].dbPrefix != NULL) && (foundDbMap == NULL);
         i++)
        {
        struct dbToSpecies* dbMap = &(dbToSpeciesMap[i]);
        for (j = 0; dbMap->names[j] != NULL; j++)
            {
            if (sameString (dbMap->names[j], organism))
                foundDbMap = dbMap;
            }
        if ((dbMap->subSpeciesPrefix != NULL)
            && startsWith(dbMap->subSpeciesPrefix, organism))
            foundDbMap = dbMap;
        }
    strcpy(organismCache, organism);
    dbMapCache = foundDbMap;
    cacheEmpty = FALSE;
    }

if (dbMapCache == NULL)
    return NULL;
else
    return dbMapCache->names[0];
}

unsigned gbGenomeOrgCat(struct gbGenome* genome, char* organism)
/* Compare a species to the one associated with this genome, returning
 * GB_NATIVE or GB_XENO, or 0 if genome is null. */
{
int i;
if (genome == NULL)
    return 0;
for (i = 0; genome->dbMap->names[i] != NULL; i++)
    {
    if (sameString(organism, genome->dbMap->names[i]))
        return GB_NATIVE;
    }

if ((genome->dbMap->subSpeciesPrefix != NULL)
    && startsWith(genome->dbMap->subSpeciesPrefix, organism))
    return GB_NATIVE;
return GB_XENO;
}

void gbGenomeFree(struct gbGenome** genomePtr)
/* free a genome object */
{
struct gbGenome* genome = *genomePtr;
if (genome != NULL)
    {
    free(genome->database);
    *genomePtr = NULL;
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

