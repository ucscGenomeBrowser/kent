#include "gbGenome.h"
#include "gbRelease.h"
#include "gbDefs.h"
#include "localmem.h"

static char const rcsid[] = "$Id: gbGenome.c,v 1.67 2008/08/01 17:31:13 braney Exp $";

struct dbToSpecies
/* structure mapping database prefix to species (e.g. hg -> "Homo sapiens"). */
{
    char *dbPrefix;           /* prefix of database (e.g. hg) */
    char **names;             /* list of species name, terminate by null.
                               * first name is prefered. */
};

static char *hgNames[] = {"Homo sapiens", NULL};
static char *mmNames[] = {"Mus musculus", "Mus sp.", 
                          "Mus musculus bactrianus",
                          "Mus musculus castaneus",
                          "Mus musculus domesticus",
                          "Mus musculus molossinus",
                          "Mus musculus musculus",
                          "Mus musculus wagneri", NULL};
static char *rnNames[] = {"Rattus norvegicus", "Rattus sp.", NULL};
static char *ciNames[] = {"Ciona intestinalis", NULL};
static char *cioSavNames[] = {"Ciona savignyi", NULL};
static char *strPurNames[] = {"Strongylocentrotus purpuratus", NULL};
static char *frNames[] = {"Takifugu rubripes", NULL};
static char *dmNames[] = {"Drosophila melanogaster", "Drosophila sp.", NULL};
static char *dpNames[] = {"Drosophila pseudoobscura", NULL};
static char *sacCerNames[] = {"Saccharomyces cerevisiae", NULL};
static char *panTroNames[] = {"Pan troglodytes", "Pan troglodytes troglodytes", 
                              "Pan troglodytes verus", NULL};
static char *ponAbeNames[] = {"Pongo abelii", "Pongo pygmaeus",
		"Pongo pygmaeus pygmaeus", "Pongo pygmaeus abelii", NULL};
static char *rheMacNames[] = {"Macaca mulatta", NULL};
static char *monDomNames[] = {"Monodelphis domestica", NULL};
static char *galGalNames[] = {"Gallus gallus", "Gallus sp.", NULL};
static char *ceNames[] = {"Caenorhabditis elegans", NULL};
static char *cbNames[] = {"Caenorhabditis briggsae", NULL};
static char *caeRemNames[] = {"Caenorhabditis remanei", NULL};
static char *caeJapNames[] = {"Caenorhabditis japonica", NULL};
static char *caePbNames[] = {"Caenorhabditis brenneri", NULL};
static char *calJacNames[] = {"Callithrix jacchus", NULL};
static char *danRerNames[] = {"Danio rerio", NULL};
static char *echTelNames[] = {"Echinops telfairi", NULL};
static char *oryCunNames[] = {"Oryctolagus cuniculus", NULL};
static char *cavPorNames[] = {"Cavia porcellus", NULL};
static char *loxAfrNames[] = {"Loxodonta africana", NULL};
static char *dasNovNames[] = {"Dasypus novemcinctus", NULL};
static char *canFamNames[] = {"Canis familiaris", "Canis sp.",
                              "Canis lupus familiaris",
			      "Canis lupus", NULL};
static char *felCatNames[] = {"Felis catus", NULL};
static char *droYakNames[] = {"Drosophila yakuba", NULL};
static char *droAnaNames[] = {"Drosophila ananassae", NULL};
static char *droMojNames[] = {"Drosophila mojavensis", NULL};
static char *droVirNames[] = {"Drosophila virilis", NULL};
static char *droEreNames[] = {"Drosophila erecta", NULL};
static char *droSimNames[] = {"Drosophila simulans", NULL};
static char *droGriNames[] = {"Drosophila grimshawi", NULL};
static char *droPerNames[] = {"Drosophila persimilis", NULL};
static char *droSecNames[] = {"Drosophila sechellia", NULL};
static char *anoGamNames[] = {"Anopheles gambiae", NULL};
static char *apiMelNames[] = {"Apis mellifera", NULL};
static char *triCasNames[] = {"Tribolium castaneum", NULL};
static char *tetNigNames[] = {"Tetraodon nigroviridis", NULL};
static char *bosTauNames[] = {"Bos taurus", NULL};
static char *xenTroNames[] = {"Xenopus tropicalis", 
                              "Xenopus (Silurana) tropicalis", NULL};
static char *anoCarNames[] = {"Anolis carolinensis", NULL};
static char *gasAcuNames[] = {"Gasterosteus aculeatus", NULL};
static char *oryLatNames[] = {"Oryzias latipes", NULL};
static char *equCabNames[] = {"Equus caballus", NULL};
static char *ornAnaNames[] = {"Ornithorhynchus anatinus", NULL};
static char *petMarNames[] = {"Petromyzon marinus", NULL};
static char *braFloNames[] = {"Branchiostoma floridae", NULL};
static char *priPacNames[] = {"Pristionchus pacificus", NULL};
/* hypothetical ancestor, will never match native */
static char *canHgNames[] = {"Boreoeutheria ancestor", NULL};
static char *taeGutNames[] = {"Taeniopygia guttata", NULL};

static char *endNames[] = {NULL};

static struct dbToSpecies dbToSpeciesMap[] = {
    {"hg", hgNames},
    {"mm", mmNames},
    {"rn", rnNames},
    {"ci", ciNames},
    {"cioSav", cioSavNames},
    {"fr", frNames},
    {"dm", dmNames},
    {"dp", dpNames},
    {"sacCer", sacCerNames},
    {"panTro", panTroNames},
    {"ponAbe", ponAbeNames},
    {"rheMac", rheMacNames},
    {"monDom", monDomNames},
    {"galGal", galGalNames},
    {"ce", ceNames},
    {"cb", cbNames},
    {"caeRem", caeRemNames},
    {"caeJap", caeJapNames},
    {"caePb", caePbNames},
    {"caeRei", caeRemNames}, /* db spelling mistake, should be Rem */
    {"calJac", calJacNames},
    {"danRer", danRerNames},
    {"canFam", canFamNames},
    {"felCat", felCatNames},
    {"loxAfr", loxAfrNames},
    {"dasNov", dasNovNames},
    {"echTel", echTelNames},
    {"oryCun", oryCunNames},
    {"cavPor", cavPorNames},
    {"equCab", equCabNames},
    {"droYak", droYakNames},
    {"droAna", droAnaNames},
    {"droMoj", droMojNames},
    {"droVir", droVirNames},
    {"droEre", droEreNames},
    {"droSim", droSimNames},
    {"droGri", droGriNames},
    {"droPer", droPerNames},
    {"droSec", droSecNames},
    {"anoGam", anoGamNames},
    {"apiMel", apiMelNames},
    {"triCas", triCasNames},
    {"tetNig", tetNigNames},
    {"bosTau", bosTauNames},
    {"xenTro", xenTroNames},
    {"anoCar", anoCarNames},
    {"gasAcu", gasAcuNames},
    {"oryLat", oryLatNames},
    {"ornAna", ornAnaNames},
    {"petMar", petMarNames},
    {"braFlo", braFloNames},
    {"priPac", priPacNames},
    {"canHg",  canHgNames},
    {"strPur", strPurNames},
    {"taeGut", taeGutNames},
    {NULL, endNames}
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

static struct dbToSpecies *speciesSearch(char* organism)
/* search by species name */
{
int i, j;
for (i = 0; dbToSpeciesMap[i].dbPrefix != NULL; i++)
    {
    struct dbToSpecies* dbMap = &(dbToSpeciesMap[i]);
    for (j = 0; dbMap->names[j] != NULL; j++)
        {
        if (sameString(dbMap->names[j], organism))
            return dbMap;
        }
    }
return NULL;
}

char* gbGenomePreferedOrgName(char* organism)
/* determine the prefered organism name, if this organism is known,
 * otherwise NULL.  Used for sanity checks. */
{
/* caching last found helps speed search, since entries tend to be groups,
 * especially ESTs.  NULL is a valid cache entry, so need flag */
static boolean cacheEmpty = TRUE;
static struct dbToSpecies* dbMapCache = NULL;
static char organismCache[256];

if (cacheEmpty || !sameString(organism, organismCache))
    {
    dbMapCache = speciesSearch(organism);
    strcpy(organismCache, organism);
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
/* hack: for panTro2 and beyond, treat human as native */
if (sameString(organism, "Homo sapiens")
    && startsWith("panTro", genome->database)
    && !sameString(genome->database, "panTro1"))
    return GB_NATIVE;

for (i = 0; genome->dbMap->names[i] != NULL; i++)
    {
    if (sameString(organism, genome->dbMap->names[i]))
        return GB_NATIVE;
    }

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

