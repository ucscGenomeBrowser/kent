#include "gbGenome.h"
#include "gbRelease.h"
#include "gbDefs.h"
#include "localmem.h"

static char const rcsid[] = "$Id: gbGenome.c,v 1.1 2003/06/03 01:27:46 markd Exp $";

struct dbToSpecies
/* structure mapping database prefix to species (e.g. hg -> "Homo sapiens") */
{
    char *dbPrefix;           /* prefix of database (e.g. hg) */
    char **names;              /* list of species name, terminate by null */
    char *subSpeciesPrefix;   /* if not null, used to check for subspecies;
                               * should end in a blank */
};

static char *hgNames[] = {"Homo sapiens", NULL};
static char *mmNames[] = {"Mus musculus", NULL};
static char *rnNames[] = {"Rattus norvegicus", "Rattus sp.", NULL};
static char *ciNames[] = {"Ciona intestinalis", NULL};
static char *endNames[] = {NULL};

static struct dbToSpecies dbToSpeciesMap[] = {
    {"hg", hgNames, NULL},
    {"mm", mmNames, "Mus musculus "},
    {"rn", rnNames, "Rattus norvegicus "},
    {"ci", ciNames, NULL},
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
genome->organism = cloneString(dbMap->names[0]);
genome->dbMap = dbMap;
return genome;
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
    free(genome->organism);
    *genomePtr = NULL;
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

