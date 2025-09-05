/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

/* trackHub - supports collections of tracks hosted on a remote site.
 * The basic layout of a data hub is:
 *        hub.txt - contains information about the hub itself
 *        genomes.txt - says which genomes are supported by hub
 *                 Contains file name of trackDb.txt for each genome
 *        trackDb.txt - contains a stanza for each track.  Stanzas
 *                 are in a subset of the usual trackDb format. 
 * How you use the routines here most commonly is as so:
 *     struct trackHub *hub = trackHubOpen(hubRaUrl);
 *     struct trackHubGenome *hubGenome = trackHubFindGenome(hub, "hg19");
 *     struct trackDb *tdbList = trackHubTracksForGenome(hub, hubGenome);
 *          // do something with tdbList
 *     trackHubClose(&hub);
 * Note that the tdbList returned does not have the parent/subtrack pointers set.
 * It is just a simple list of tracks, not a tree.  
 */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "udc.h"
#include "ra.h"
#include "filePath.h"
#include "htmlPage.h"
#include "trackDb.h"
#include "trackHub.h"
#include "errCatch.h"
#include "hgBam.h"
#include "bigWig.h"
#include "bigBed.h"
#include "barChartUi.h"
#include "hdb.h"
#include "chromInfo.h"
#include "grp.h"
#include "twoBit.h"
#include "dbDb.h"
#include "net.h"
#include "bbiFile.h"
#include "bPlusTree.h"
#include "hgFind.h"
#include "hubConnect.h"
#include "trix.h"
#include "vcf.h"
#include "vcfUi.h"
#include "htmshell.h"
#include "bigBedFind.h"
#include "customComposite.h"
#include "interactUi.h"
#include "bedTabix.h"
#include "hic.h"
#include "hui.h"
#include "chromAlias.h"
#include "trashDir.h"
#include "hgConfig.h"
#include "cartTrackDb.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

struct grp *trackHubGrps = NULL;   // global with grps loaded from track hubs
static struct hash *hubCladeHash;  // mapping of clade name to hub pointer
static struct hash *hubAssemblyHash; // mapping of assembly name to genome struct
static struct hash *hubAssemblyUndecoratedHash; // mapping of undecorated assembly name to genome struct
static struct hash *hubOrgHash;   // mapping from organism name to hub pointer
static struct trackHub *globalAssemblyHubList; // list of trackHubs in the user's cart
static struct hash *trackHubHash;

static void tdbListAddHubToGroup(char *hubName, struct trackDb *tdbList)
/* Prepend hub name to  group name for every tdb. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    char buffer[4096];

    char *grp = trackDbSetting(tdb, "group");
    if (grp == NULL)
        tdb->grp = cloneString(hubName);
    else
        {
        safef(buffer, sizeof buffer, "%s_%s", hubName, grp);
        tdb->grp = cloneString(buffer);
        }
    hashReplace(tdb->settingsHash, "group", tdb->grp);
    }
}



char *trackHubRelativeUrl(char *hubUrl, char *path)
/* Return full path (in URL form if it's a remote hub) given
 * path possibly relative to hubUrl. Do a freeMem of result
 * when done. */
{
/* If path itself is a URL then just return a copy of it. */
if (hasProtocol(path))
    return cloneString(path);

/* If it's a remote hub, let html path expander handle it. */
if (hasProtocol(hubUrl) && !startsWith("/gbdb",path))
    return expandUrlOnBase(hubUrl, path);

/* If we got to here hub is local, and so is path.  Do standard
 * path parsing. */
return pathRelativeToFile(hubUrl, path);
}

static void badGenomeStanza(struct lineFile *lf)
/* Put up semi-informative error message about a genome stanza being bad. */
{
errAbort("Genome stanza should have at least two lines, one with 'genome' and one with 'trackDb'\n"
         "Bad stanza format ending line %d of %s", lf->lineIx, lf->fileName);
}

char *trackHubCladeToGenome(char *clade) 
/* Given a track hub clade(hub name) return the default genome. */
{
if (hubCladeHash == NULL)
    return FALSE;
struct hashEl *hel = hashLookup(hubCladeHash, clade);
if (hel == NULL)
    return FALSE;
struct trackHub *trackHub = hel->val;
struct trackHubGenome *hubGenome = trackHub->genomeList;
for(; hubGenome; hubGenome=hubGenome->next)
    if (hubGenome->twoBitPath != NULL)
	return hubGenome->organism ;
return NULL;
}

struct trackHubGenome *trackHubGetGenomeUndecorated(char *database)
/* Get the genome structure for an undecorated genome name. */
{
if (hubAssemblyUndecoratedHash == NULL)
    return NULL;

struct hashEl *hel = hashLookup(hubAssemblyUndecoratedHash, database);

if (hel == NULL)
    return NULL;

return (struct trackHubGenome *)hel->val;
}

struct trackHubGenome *trackHubGetGenome(char *database)
/* get genome structure for an assembly in a trackHub */
{
if (hubAssemblyHash == NULL)
    errAbort("An error occurred while adding the custom track. This may be due to a mismatch"
		    " between the db= parameter and the genome selected at the top of the page."
		    " If you are having trouble resolving this error please contact us at"
		    " genome-www@soe.ucsc.edu.");

struct hashEl *hel = hashLookup(hubAssemblyHash, database);

if (hel == NULL)
    return NULL;

return (struct trackHubGenome *)hel->val;
}

boolean trackHubDatabase(char *database)
/* Is this an assembly from an Assembly Data hub? */
{
if (hubAssemblyHash == NULL)
    return FALSE;

return trackHubGetGenome(database) != NULL;
}

char *trackHubDatabaseToGenome(char *db)
/* get a database name that is either a genome database or a trackHub
 * database, return the genome assembly */
{
return trackHubDatabase(db) ? trackHubAssemblyField(db, "genome") : db;
}

char *trackHubAssemblyField(char *database, char *field)
/* Get data field from a assembly data hub. */
{
struct trackHubGenome *genome = trackHubGetGenome(database);

if (genome == NULL)
    return NULL;

char *ret = hashFindVal(genome->settingsHash, field);

return cloneString(ret);
}

static struct dbDb *makeDbDbFromAssemblyGenome(struct trackHubGenome *hubGenome)
/* Make a dbdb struture from a single assembly hub database. */
{
struct dbDb *db;

AllocVar(db);
db->genome = cloneString(hubGenome->organism);
db->organism = cloneString(hubGenome->organism);
db->name = cloneString(hubGenome->name);
db->active = TRUE;
if (hubGenome->description != NULL)
    db->description = cloneString(hubGenome->description);
else
    db->description = cloneString("");
char *orderKey = hashFindVal(hubGenome->settingsHash, "orderKey");
if (orderKey != NULL)
    db->orderKey = sqlUnsigned(orderKey);
db->defaultPos = cloneString(hubGenome->defaultPos);
return db;
}

struct dbDb *trackHubDbDbFromAssemblyDb(char *database)
/* Return a dbDb structure for just this database. */
{
struct trackHubGenome *genome = trackHubGetGenome(database);

if (genome == NULL)
    return NULL;

return makeDbDbFromAssemblyGenome(genome);
}

struct slPair *trackHubGetCladeLabels()
/* Get a list of labels describing the loaded assembly data hubs. */
{
if (globalAssemblyHubList == NULL)
    return NULL;

struct slPair *clade, *cladeList = NULL;

struct trackHub *trackHub = globalAssemblyHubList;

for(;trackHub; trackHub = trackHub->next)
    {
    AllocVar(clade);
    slAddHead(&cladeList, clade);

    clade->name = cloneString(trackHub->name);
    clade->val = cloneString(trackHub->shortLabel);
    }
return cladeList;
}

struct dbDb *trackHubGetPcrServers()
/* Look through attached trackHubs to see which of them have "isPcr" line in them. */
{
struct dbDb *db, *dbList = NULL;

if (globalAssemblyHubList != NULL)
    {
    struct trackHub *trackHub = globalAssemblyHubList;

    for(;trackHub; trackHub = trackHub->next)
	{
	struct trackHubGenome *hubGenome = trackHub->genomeList;
	for(; hubGenome; hubGenome = hubGenome->next)
	    {
            if (hashFindVal(hubGenome->settingsHash,"isPcr") != NULL)
		{
		db = makeDbDbFromAssemblyGenome(hubGenome);
		slAddHead(&dbList, db);
		}
	    }
	}
    }

slReverse(&dbList);
slSort(&dbList, hDbDbCmpOrderKey);
return dbList;
}

static struct dbDb *getDbDbs(char *clade, boolean blatEnabled)
/* Get a list of struct dbDbs from track hubs.  Only get blat enabled ones if asked */
{
struct dbDb *db, *dbList = NULL;

if (globalAssemblyHubList != NULL)
    {
    struct trackHub *trackHub = globalAssemblyHubList;

    for(;trackHub; trackHub = trackHub->next)
	{
	if ((clade != NULL) && differentString(clade, trackHub->name))
	    continue;

	struct trackHubGenome *hubGenome = trackHub->genomeList;
	for(; hubGenome; hubGenome = hubGenome->next)
	    {
	    boolean blatCheck = !blatEnabled ||
		((hashFindVal(hubGenome->settingsHash,"transBlat") != NULL) || 
		(hashFindVal(hubGenome->settingsHash,"blat") != NULL));
	    if ( blatCheck && (hubGenome->twoBitPath != NULL))
		{
		db = makeDbDbFromAssemblyGenome(hubGenome);
		slAddHead(&dbList, db);
		}
	    }
	}
    }

slReverse(&dbList);
slSort(&dbList, hDbDbCmpOrderKey);
return dbList;
}

struct dbDb *trackHubGetBlatDbDbs()
/* Get a list of connected track hubs that have blat servers */
{
return getDbDbs(NULL, TRUE);
}

struct dbDb *trackHubGetDbDbs(char *clade)
/* Get a list of dbDb structures for all the tracks in this clade/hub. */
{
return getDbDbs(clade, FALSE);
}

struct slPair *trackHubDbDbToValueLabel(struct dbDb *hubDbDbList)
/* Given a trackHub (list of) track hub dbDb which may be missing some info,
 * return an slPair of value and label suitable for making a select/menu option. */
{
struct dbDb *dbDb;
struct slPair *pairList = NULL;
for (dbDb = hubDbDbList;  dbDb != NULL;  dbDb = dbDb->next)
    {
    char *db = dbDb->name;
    if (isEmpty(db))
        db = dbDb->genome;
    char *label = dbDb->description;
    if (isEmpty(label))
        label = trackHubSkipHubName(db);
    slAddHead(&pairList, slPairNew(db, cloneString(label)));
    }
slReverse(&pairList);
return pairList;
}

struct slName *trackHubAllChromNames(char *database)
/* Return a list of all the chrom names in this assembly hub database. */
/* Free with slFreeList. */
{
struct trackHubGenome *genome = trackHubGetGenome(database);
if (genome == NULL)
    return NULL;

struct slName *chromList = twoBitSeqNames(genome->twoBitPath);

return chromList;
}

int trackHubChromCount(char *database)
/* Return number of chromosomes in a assembly data hub. */
{
struct slName *chromList = trackHubAllChromNames(database);

int num = slCount(chromList);
slFreeList(&chromList);
return  num;
}

char *trackHubDefaultChrom(char *database)
/* Return the default chromosome for this track hub assembly. */
{
struct slName *chromList = trackHubAllChromNames(database);

if (chromList == NULL)
    return NULL;

char *defaultName = cloneString( chromList->name);
slFreeList(&chromList);

return defaultName;
}

static struct twoBitFile *openTwoBit(struct trackHubGenome *genome)
/* Open a twoBit file that may or may not have a bpt index. */
{
if (genome->twoBitBptUrl)
    return twoBitOpenExternalBptIndex(genome->twoBitPath, genome->twoBitBptUrl);
else
    return twoBitOpen(genome->twoBitPath);
}

struct chromInfo *trackHubMaybeChromInfo(char *database, char *chrom)
/* Return a chromInfo structure for just this chrom in this database. 
 * Return NULL if chrom doesn't exist. */
{
struct trackHubGenome *genome = trackHubGetGenome(database);
if (genome == NULL)
    return NULL;

if (genome->tbf == NULL)
    genome->tbf = openTwoBit(genome);
if (!twoBitIsSequence(genome->tbf, chrom))
    return NULL;

struct chromInfo *ci;
AllocVar(ci);
ci->chrom = cloneString(chrom);
ci->fileName = genome->twoBitPath;
ci->size = twoBitSeqSize(genome->tbf, chrom);

return ci;
}

struct chromInfo *trackHubChromInfo(char *database, char *chrom)
/* Return a chromInfo structure for just this chrom in this database. 
 * errAbort if chrom doesn't exist. */
{
struct chromInfo *ci = trackHubMaybeChromInfo(database, chrom);

if (ci == NULL)
    errAbort("%s is not in %s", chrom, database);

return ci;
}

static char *assemblyHubGenomeSetting(char *database, char *tagName)
/* see if this assembly hub has specified tagName, return url if present
 * returns NULL when not present
 */
{
struct trackHubGenome *genome = trackHubGetGenome(database);
if (genome == NULL)
    return NULL;
char *fileName = hashFindVal(genome->settingsHash, tagName);
char *absFileName  = NULL;
if (fileName)
    absFileName  = trackHubRelativeUrl((genome->trackHub)->url, fileName);
if  (absFileName)
    {
    hashReplace(genome->settingsHash, tagName, absFileName);
    fileName = absFileName;
    }
return fileName;
}

char *trackHubChromSizes(char *database)
/* see if this assembly hub has a chrom.sizes file, return url if present
 * returns NULL when not present
 */
{
return assemblyHubGenomeSetting(database, "chromSizes");
}

char *trackHubAliasBbFile(char *database)
/* see if this assembly hub has an alias bigBed file, return url if present
 * returns NULL when not present
 */
{
return assemblyHubGenomeSetting(database, "chromAliasBb");
}

char *trackHubAliasFile(char *database)
/* see if this assembly hub has an alias file, return url if present
 * returns NULL when not present
 */
{
return assemblyHubGenomeSetting(database, "chromAlias");
}

struct chromInfo *trackHubAllChromInfo(char *database)
/* Return a chromInfo structure for all the chroms in this database. */
{
struct trackHubGenome *genome = trackHubGetGenome(database);
if (genome == NULL)
    return NULL;

if (genome->tbf == NULL)
    genome->tbf = openTwoBit(genome);
struct chromInfo *ci, *ciList = NULL;
struct slName *chromList = twoBitSeqNames(genome->twoBitPath);

for(; chromList; chromList = chromList->next)
    {
    AllocVar(ci);
    ci->chrom = cloneString(chromList->name);
    ci->fileName = cloneString(genome->twoBitPath);
    ci->size = twoBitSeqSize(genome->tbf, chromList->name);
    slAddHead(&ciList, ci);
    }
slFreeList(&chromList);
return ciList;
}

static char *getRequiredGrpSetting(struct hash *hash, char *name, struct lineFile *lf)
/* Grab a group setting out of the group hash.  errAbort if not found. */
{
char *str;
if ((str = hashFindVal(hash, name)) == NULL) 
    errAbort("missing required setting '%s' for group on line %d in file %s\n",
	name, lf->lineIx, lf->fileName);
return str;
}

struct grp *readGroupRa(char *groupFileName)
/* Read in the ra file that describes the groups in an assembly hub. */
{
if (groupFileName == NULL)
    return NULL;
struct hash *ra;
struct grp *list = NULL;
struct lineFile *lf = udcWrapShortLineFile(groupFileName, NULL, MAX_HUB_GROUP_FILE_SIZE);
while ((ra = raNextRecord(lf)) != NULL)
    {
    char *str;
    struct grp *grp;
    AllocVar(grp);
    slAddHead(&list, grp);

    grp->name = cloneString(getRequiredGrpSetting(ra, "name", lf));
    grp->label = cloneString(getRequiredGrpSetting(ra, "label", lf));

    grp->priority = BIGDOUBLE;
    str = hashFindVal(ra, "priority");
    if (str != NULL)
        grp->priority = atof(str);

    str = hashFindVal(ra, "defaultIsClosed");
    if ((str != NULL) && (sameString("on",str) || sameString("1", str)))
        grp->defaultIsClosed = 1;
    hashFree(&ra);
    }
if (list)
    slSort(&list, grpCmpPriorityLabel);

lineFileClose(&lf);

return list;
}

struct grp *trackHubLoadGroups(char *database)
/* Load the grp structures for this track hub database. */
{
struct trackHubGenome *genome = trackHubGetGenome(database);
if (genome == NULL)
    return NULL;
struct grp *list = readGroupRa(genome->groups);
return list;
}

char *trackHubGenomeNameToDb(char *genome)
/* Return assembly name given a genome name if one exists, otherwise NULL. */
{
struct hashEl *hel;
if ((hubOrgHash != NULL) && (hel = hashLookup(hubOrgHash, genome)) != NULL)
    {
    struct trackHub *hub = hel->val;
    struct trackHubGenome *genomeList = hub->genomeList;

    for(; genomeList; genomeList=genomeList->next)
	if ((genomeList->organism != NULL ) && 
	    sameString(genomeList->organism, genome))
	    return genomeList->name;
    }
return NULL;
}

char *trackHubAssemblyClade(char *genome)
/* Return the clade/hub_name that contains this genome. */
{
struct hashEl *hel;
if ((hubOrgHash != NULL) && (hel = hashLookup(hubOrgHash, genome)) != NULL)
    {
    struct trackHub *hub = hel->val;

    return cloneString(hub->name);
    }
return NULL;
}

static void deleteAssembly(char *name, struct trackHubGenome *genome, struct trackHub *hub)
/* delete this assembly from the assembly caches */
{
hashRemove(hubCladeHash, hub->name);
slRemoveEl(&globalAssemblyHubList, hub);

hashRemove(hubOrgHash, genome->organism);

hashRemove(hubAssemblyHash, genome->name);
}

static void addAssembly(char *name, struct trackHubGenome *genome, struct trackHub *hub)
/* Add a new assembly hub database to our global list. */
{
struct hashEl *hel;

if (hubCladeHash == NULL)
    hubCladeHash = newHash(5);
if ((hel = hashLookup(hubCladeHash, hub->name)) == NULL)
    {
    hashAdd(hubCladeHash, hub->name, hub);
    slAddHead(&globalAssemblyHubList, hub);
    }

if (hubOrgHash == NULL)
    hubOrgHash = newHash(5);
if ((hel = hashLookup(hubOrgHash, genome->organism)) == NULL)
    {
    hashAdd(hubOrgHash, genome->organism, hub);
    }

if (hubAssemblyHash == NULL)
    hubAssemblyHash = newHash(5);
if ((hel = hashLookup(hubAssemblyHash, genome->name)) == NULL)
    hashAdd(hubAssemblyHash, genome->name, genome);

if (hubAssemblyUndecoratedHash == NULL)
    hubAssemblyUndecoratedHash = newHash(5);
if ((hel = hashLookup(hubAssemblyUndecoratedHash, trackHubSkipHubName(genome->name))) == NULL)
    hashAdd(hubAssemblyUndecoratedHash, trackHubSkipHubName(genome->name), genome);
}

static char *addHubName(char *base, char *hubName)
{
if (base == NULL)
    return NULL;

char buffer[4096];

if (isNotEmpty(hubName))
    safef(buffer, sizeof(buffer), "%s_%s", hubName, base);
else
    safef(buffer, sizeof(buffer), "%s", base);

return cloneString(buffer);
}

static int genomeOrderKeyCmp(const void *va, const void *vb)
/* Compare to sort based on order key */
{
const struct trackHubGenome *a = *((struct trackHubGenome **)va);
const struct trackHubGenome *b = *((struct trackHubGenome **)vb);

if (b->orderKey > a->orderKey) return -1;
else if (b->orderKey < a->orderKey) return 1;
else return 0;
}

static struct trackHubGenome *trackHubGenomeReadRa(char *url, struct trackHub *hub, char *singleFile)
/* Read in a genome.ra format url and return it as a list of trackHubGenomes. 
 * Also add it to hash, which is keyed by genome. */
{
struct lineFile *lf = udcWrapShortLineFile(url, NULL, MAX_HUB_GENOME_FILE_SIZE);
struct trackHubGenome *list = NULL, *el;
struct hash *hash = hub->genomeHash;

struct hash *ra;
while ((ra = raNextRecord(lf)) != NULL)
    {
    // allow that trackDb+hub+genome is in one single file
    if (hashFindVal(ra, "hub"))
        continue;
    if (hashFindVal(ra, "track"))
        break;

    char *twoBitPath = hReplaceGbdb(hashFindVal(ra, "twoBitPath"));
    char *twoBitBptUrl = hReplaceGbdb(hashFindVal(ra, "twoBitBptUrl"));
    char *genome, *trackDb;
    if (twoBitPath != NULL)
	genome = addHubName(hashFindVal(ra, "genome"), hub->name);
    else
        genome = hashFindVal(ra, "genome");
    if (hub->defaultDb == NULL)
	hub->defaultDb = genome;
    if (genome == NULL)
        badGenomeStanza(lf);
    if (hasWhiteSpace(genome))
        errAbort("Bad genome name: \"%s\". Only alpha-numeric characters and \"_\" are allowed ([A-Za-z0-9_]).", genome);
    if (hashLookup(hash, genome) != NULL)
        errAbort("Duplicate genome %s in stanza ending line %d of %s",
		genome, lf->lineIx, lf->fileName);
    if (singleFile == NULL)
        {
        trackDb = hashFindVal(ra, "trackDb");
        if (trackDb == NULL)
            badGenomeStanza(lf);
        }
    else
        trackDb = singleFile;
    AllocVar(el);
    el->name = cloneString(genome);
    el->trackDbFile = trackHubRelativeUrl(url, trackDb);
    el->trackHub = hub;
    hashAdd(hash, el->name, el);
    hashAdd(hash, hubConnectSkipHubPrefix(el->name), el);
    slAddHead(&list, el);
    char *orderKey = hashFindVal(ra, "orderKey");
    if (orderKey != NULL)
	el->orderKey = sqlUnsigned(orderKey);

    char *groups = hashFindVal(ra, "groups");
    if (twoBitPath != NULL)
	{
	el->description  = hashFindVal(ra, "description");
	char *organism = hashFindVal(ra, "organism");
	if (organism == NULL)
	    errAbort("must have 'organism' set in assembly hub in stanza ending line %d of %s",
		     lf->lineIx, lf->fileName);
	el->organism  = addHubName(organism, hub->name);
	hashReplace(ra, "organism", el->organism);
	el->defaultPos  = hashFindVal(ra, "defaultPos");
	if (el->defaultPos == NULL)
	    errAbort("must have 'defaultPos' set in assembly hub in stanza ending line %d of %s",
		     lf->lineIx, lf->fileName);
	el->twoBitPath = trackHubRelativeUrl(url, twoBitPath);
        if (twoBitBptUrl != NULL)
            el->twoBitBptUrl = trackHubRelativeUrl(url, twoBitBptUrl);

	char *htmlPath = hashFindVal(ra, "htmlPath");
	if (htmlPath != NULL)
	    hashReplace(ra, "htmlPath",trackHubRelativeUrl(url, htmlPath));
	if (groups != NULL)
	    el->groups = trackHubRelativeUrl(url, groups);
	addAssembly(genome, el, hub);
	}
    else
        {
	if (groups != NULL)
            {
	    el->groups = trackHubRelativeUrl(url, groups);
            }
        }
    el->settingsHash = ra;
    hashAdd(ra, "hubName", hub->shortLabel);
    el->chromAuthority = hashFindVal(ra, "chromAuthority");
    }

/* Clean up and go home. */
lineFileClose(&lf);
slReverse(&list);
slSort(&list, genomeOrderKeyCmp);
return list;
}

char *trackHubSetting(struct trackHub *hub, char *name)
/* Return setting if it exists, otherwise NULL. */
{
return hashFindVal(hub->settings, name);
}

char *trackHubRequiredSetting(struct trackHub *hub, char *name)
/* Return named setting.  Abort with error message if not found. */
{
char *val = trackHubSetting(hub, name);
if (val == NULL)
    errAbort("Missing required setting '%s' from %s", name, hub->url);
return val;
}

struct trackHub *grabHashedHub(char *hubName)
/* see if a trackHub with this name is in the cache */
{
if ( trackHubHash == NULL)
    trackHubHash = newHash(5);

return  (struct trackHub *)hashFindVal(trackHubHash, hubName); 
}

static void cacheHub(struct trackHub *hub)
{
/* put this trackHub in the trackHub hash */
if ( trackHubHash == NULL)
    trackHubHash = newHash(5);

hashAdd(trackHubHash, hub->name, hub);
}

void uncacheHub(struct trackHub *hub)
/* take this trackHub out of the trackHub hash */
{
if ( trackHubHash == NULL)
    return;

hashMustRemove(trackHubHash, hub->name);
}

struct trackHub *trackHubOpen(char *url, char *hubName)
/* Open up a track hub from url.  Reads and parses hub.txt and the genomesFile. 
 * The hubName is generally just the asciified ID number. */
{
struct trackHub *hub = grabHashedHub(hubName);

if (hub != NULL)
    return hub;

struct lineFile *lf = udcWrapShortLineFile(url, NULL, MAX_HUB_TRACKDB_FILE_SIZE);
struct hash *hubRa = raNextRecord(lf);
if (hubRa == NULL)
    errAbort("empty %s in trackHubOpen", url);
// no errAbort when more records in hub.txt file: user can stuff
// trackDb into it

/* Allocate hub and fill in settings field and url. */
AllocVar(hub);
hub->url = cloneString(url);
hub->name = cloneString(hubName);
hub->settings = hubRa;

/* Fill in required fields from settings. */
trackHubRequiredSetting(hub, "hub");
trackHubRequiredSetting(hub, "email");
hub->shortLabel = trackHubRequiredSetting(hub, "shortLabel");
hub->longLabel = trackHubRequiredSetting(hub, "longLabel");

boolean isOneFile = (trackHubSetting(hub, "useOneFile") != NULL);
char *ourFile = NULL;

if (isOneFile)
    {
    ourFile = url;
    char *root = strrchr(url, '/');
    if (root)
        ourFile = root + 1;
    hub->genomesFile = cloneString(ourFile);
    }
else
    hub->genomesFile = trackHubRequiredSetting(hub, "genomesFile");

hub->email =  trackHubSetting(hub, "email");
hub->version = trackHubSetting(hub, "version"); // default to current version
hub->level = trackHubSetting(hub, "level");     // "core" or "all"
char *descriptionUrl = trackHubSetting(hub, "descriptionUrl");
if (descriptionUrl != NULL)
    hub->descriptionUrl = trackHubRelativeUrl(hub->url, descriptionUrl);

lineFileClose(&lf);
char *genomesUrl = trackHubRelativeUrl(hub->url, hub->genomesFile);

hub->genomeHash = hashNew(8);
hub->genomeList = trackHubGenomeReadRa(genomesUrl, hub, ourFile);
freez(&genomesUrl);

cacheHub(hub);
return hub;
}

void trackHubClose(struct trackHub **pHub)
/* Close up and free resources from hub. */
{
struct trackHub *hub = *pHub;
if (hub != NULL)
    {
    trackHubGenomeFreeList(hub);
    freeMem(hub->url);
    hashFree(&hub->settings);
    hashFree(&hub->genomeHash);
    uncacheHub(hub);
    freez(pHub);
    }
}

void trackHubGenomeFree(struct trackHubGenome **pGenome)
/* Free up genome info. */
{
struct trackHubGenome *genome = *pGenome;
if (genome != NULL)
    {
    freeMem(genome->name);
    freeMem(genome->trackDbFile);
    freez(pGenome);
    }
}

void trackHubGenomeFreeList(struct trackHub *hub)
/* Free a list of dynamically allocated trackHubGenome's */
{
struct trackHubGenome *el, *next;

for (el = hub->genomeList; el != NULL; el = next)
    {
    next = el->next;
    if (el->twoBitPath != NULL)
	deleteAssembly(el->name, el, hub);
    trackHubGenomeFree(&el);
    }
hub->genomeList = NULL;
}

static char *requiredSetting(struct trackHub *hub, struct trackHubGenome *genome,
	struct trackDb *tdb, char *setting)
/* Fetch setting or give an error message, a little more specific than the
 * error message from trackDbRequiredSetting(). */
{
char *val = trackDbSetting(tdb, setting);
if (val == NULL)
    errAbort("Missing required '%s' setting in hub %s genome %s track %s", setting,
    	hub->url, genome->name, tdb->track);
return val;
}

static void forbidSetting(struct trackHub *hub, struct trackHubGenome *genome,
    struct trackDb *tdb, char *setting)
/* Abort if forbidden setting found. */
{
if (trackDbSetting(tdb, setting))
    errAbort("Forbidden setting '%s' in hub %s genome %s track %s", setting,
        hub->url, genome->name, tdb->track);
}

static void expandOneUrl(struct hash *settingsHash, char *hubUrl, char *variable)
{
struct hashEl *hel = hashLookup(settingsHash, variable);
if (hel != NULL)
    {
    char *oldVal = hel->val;
    hel->val = trackHubRelativeUrl(hubUrl, oldVal);
    freeMem(oldVal);
    }
}

static void expandBigDataUrl(struct trackHub *hub, struct trackHubGenome *genome,
	struct trackDb *tdb)
/* Expand bigDataUrls so that no longer relative to genome->trackDbFile */
{
struct hashEl *hel;
struct hashCookie cookie = hashFirst(tdb->settingsHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    char *name = hel->name;
    if (trackSettingIsFile(name))
	expandOneUrl(tdb->settingsHash, genome->trackDbFile, name);
    }
}

struct trackHubGenome *trackHubFindGenome(struct trackHub *hub, char *genomeName)
/* Return trackHubGenome of given name associated with hub.  Return NULL if no
 * such genome.  Check genomeName without hub prefix to see if this hub
 * is attached to an assembly hub.*/
{
struct trackHubGenome *ret = hashFindVal(hub->genomeHash, genomeName);

if (ret == NULL)
    ret = hashFindVal(hub->genomeHash, hubConnectSkipHubPrefix(genomeName));
return ret;
}

static void requireBarChartBars(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb)
/* Fetch setting(s) or give an error message */
{
if (!trackDbSetting(tdb, BAR_CHART_CATEGORY_URL) && !trackDbSetting(tdb, BAR_CHART_CATEGORY_LABELS))
    errAbort("BarChart track '%s' is missing either %s or %s setting. Please add one of those settings to the appropriate stanza", tdb->track, BAR_CHART_CATEGORY_LABELS, BAR_CHART_CATEGORY_URL);
}

static void validateOneTrack( struct trackHub *hub, 
    struct trackHubGenome *genome, struct trackDb *tdb)
/* Validate a track's trackDb entry. */
{
/* Check for existence of fields required in all tracks */
requiredSetting(hub, genome, tdb, "shortLabel");
char *shortLabel  = trackDbSetting(tdb, "shortLabel");
memSwapChar(shortLabel, strlen(shortLabel), '\t', ' ');
requiredSetting(hub, genome, tdb, "longLabel");
char *longLabel  = trackDbSetting(tdb, "longLabel");
memSwapChar(longLabel, strlen(longLabel), '\t', ' ');

/* Forbid any dangerous settings that should not be allowed */
forbidSetting(hub, genome, tdb, "idInUrlSql");

// subtracks is not NULL if a track said we were its parent
if (tdb->subtracks != NULL)
    {
    boolean isSuper = FALSE;
    char *superTrack = trackDbSetting(tdb, "superTrack");
    if ((superTrack != NULL) && startsWith("on", superTrack))
	isSuper = TRUE;

    if (!(trackDbSetting(tdb, "compositeTrack") ||
          trackDbSetting(tdb, "container") || 
	  isSuper))
        {
	errAbort("Parent track %s is not compositeTrack, container, or superTrack in hub %s genome %s", 
		tdb->track, hub->url, genome->name);
	}
    }
else
    {
    /* Check type field. */
    char *type = requiredSetting(hub, genome, tdb, "type");
    char *quickLifted = trackDbSetting(tdb, "quickLifted");
    if (! ( isCustomComposite(tdb) || (quickLifted != NULL)))
        {
        if (startsWithWord("mathWig", type) )
            {
            requiredSetting(hub, genome, tdb, "mathDataUrl");
            }
        else 
            {
            /* RMH: Added bigRmsk to support RepeatMasker data in bigBed track hub format */
            if (!(startsWithWord("wig", type)||  startsWithWord("bedGraph", type)))
                {
                if (!(startsWithWord("bigWig", type) ||
                  startsWithWord("bigBed", type) ||
#ifdef USE_HAL
                  startsWithWord("pslSnake", type) ||
                  startsWithWord("halSnake", type) ||
#endif
                  startsWithWord("vcfTabix", type) ||
                  startsWithWord("vcfPhasedTrio", type) ||
                  startsWithWord("bigPsl", type) ||
                  startsWithWord("bigMaf", type) ||
                  startsWithWord("longTabix", type) ||
                  startsWithWord("bigGenePred", type) ||
                  startsWithWord("bigNarrowPeak", type) ||
                  startsWithWord("bigChain", type) ||
                  startsWithWord("bigLolly", type) ||
                  startsWithWord("bigBaseView", type) ||
                  startsWithWord("bigRmsk", type) ||
                  startsWithWord("bigBarChart", type) ||
                  startsWithWord("bigInteract", type) ||
                  startsWithWord("bigMethyl", type) ||
                  startsWithWord("hic", type) ||
                  startsWithWord("bigDbSnp", type) ||
                  startsWithWord("instaPort", type) ||
                  startsWithWord("bam", type)))
                    {
                    errAbort("Unsupported type '%s' in hub %s genome %s track %s", type,
                        hub->url, genome->name, tdb->track);
                    }
                requiredSetting(hub, genome, tdb, "bigDataUrl");
                }
            }

        if (sameString("barChart", type) || sameString("bigBarChart", type))
            requireBarChartBars(hub, genome, tdb);
        if (sameString("vcfPhasedTrio", type))
            requiredSetting(hub, genome, tdb, VCF_PHASED_CHILD_SAMPLE_SETTING);
        }
    }
}

static void markContainers( struct trackHub *hub, 
    struct trackHubGenome *genome, struct trackDb *tdbList)
/* Mark containers that are parents, or have them. */
{
struct hash *hash = hashNew(0);
struct trackDb *tdb;

// add all the track names to a hash
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (hashLookup(hash, tdb->track))
        errAbort("Track %s appears more than once in genome %s. " 
                "Track identifiers have to be unique. Please check your track hub files, "
                "especially the 'track' lines. "
                "The most likely reason for this error is that you duplicated a "
                "'track' identifier. Hub URL: %s", tdb->track, genome->name, hub->url);
    hashAdd(hash, tdb->track, tdb);
    }

// go through and find the container tracks
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    char *parentLine = trackDbLocalSetting(tdb, "parent");

    // maybe it's a child of a supertrack?
    if (parentLine == NULL)
	{
	parentLine = trackDbLocalSetting(tdb, "superTrack");
	if ((parentLine != NULL) && startsWith("on", parentLine))
	    parentLine = NULL;
	}

    if (parentLine != NULL)
         {
	 char *parentName = cloneFirstWord(parentLine);
	 struct trackDb *parent = hashFindVal(hash, parentName);
	 if (parent == NULL)
	    errAbort("Parent %s of track %s doesn't exist in hub %s genome %s", parentName,
		tdb->track, hub->url, genome->name);
	 // mark the parent as a container
	 parent->subtracks = tdb;

	 // ugh...do this so requiredSetting looks at parent
	 // in the case of views.  We clear this after 
	 // validating anyway
	 tdb->parent = parent;

	 freeMem(parentName);
	 }
    }
hashFree(&hash);
}

static void validateTracks( struct trackHub *hub, struct trackHubGenome *genome,
    struct trackDb *tdbList)
/* Make sure a hub track list has the right settings and its parents exist. */
{
// mark the containers by setting their subtracks pointer
markContainers(hub, genome, tdbList);

/* Loop through list checking tags */
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        validateOneTrack(hub, genome, tdb);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        tdb->errMessage = cloneString(errCatch->message->string);
        }

    // clear these two pointers which we set in markContainers
    tdb->subtracks = NULL;
    tdb->parent = NULL;
    }
}

struct trackDb *trackHubTracksForGenome(struct trackHub *hub, struct trackHubGenome *genome, struct dyString *incFiles, boolean *foundFirstGenome)
/* Get list of tracks associated with genome.  Check that it only is composed of legal
 * types.  Do a few other quick checks to catch errors early. If incFiles is not NULL,
 * put the list of included files in there.  Only the first example of a genome 
 * gets to populate groups, the others get a group for the trackHub.  */
{
struct lineFile *lf = udcWrapShortLineFile(genome->trackDbFile, NULL, MAX_HUB_TRACKDB_FILE_SIZE);
struct trackDb *tdbList = trackDbFromOpenRa(lf, NULL, incFiles);
lineFileClose(&lf);

char *tabMetaName = hashFindVal(genome->settingsHash, "metaTab");
char *absTabName  = NULL;
if (tabMetaName)
    absTabName  = trackHubRelativeUrl(hub->url, tabMetaName);

char *tagStormName = hashFindVal(genome->settingsHash, "metaDb");
char *absStormName  = NULL;
if (tagStormName)
    absStormName  = trackHubRelativeUrl(hub->url, tagStormName);

/* Make bigDataUrls more absolute rather than relative to genome.ra dir */
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    expandBigDataUrl(hub, genome, tdb);
    if  (absStormName)
        hashReplace(tdb->settingsHash, "metaDb", absStormName);
    if  (absTabName)
        hashReplace(tdb->settingsHash, "metaTab", absTabName);
    }

validateTracks(hub, genome, tdbList);

trackDbAddTableField(tdbList);
if (!isEmpty(hub->name))
    trackHubAddNamePrefix(hub->name, tdbList);

if ((genome->twoBitPath != NULL) && (*foundFirstGenome == FALSE))
    *foundFirstGenome = TRUE;
else if ((genome->groups != NULL) && hubsCanAddGroups())
    tdbListAddHubToGroup(hub->name, tdbList);
else
    trackHubAddGroupName(hub->name, tdbList);


for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    trackDbFieldsFromSettings(tdb);
    trackDbPolish(tdb);
    }
return tdbList;
}

static void reprefixString(char **pString, char *prefix)
/* Replace *pString with prefix + *pString, freeing
 * whatever was in *pString before. */
{
char *oldName = *pString;
*pString = catTwoStrings(prefix, oldName);
freeMem(oldName);
}

static void addPrefixToSetting(struct hash *settings, char *key, char *prefix)
/* Given a settings hash, which is string valued.  Old values will be freed. */
{
struct hashEl *hel = hashLookup(settings, key);
if (hel != NULL)
    reprefixString((char **)&hel->val, prefix);
}

static void trackDbListAddNamePrefix(struct trackDb *tdbList, char *prefix)
/* Surgically alter tdbList so that it works as if every track was
 * renamed so as to add a prefix to it's name. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    addPrefixToSetting(tdb->settingsHash, "track", prefix);
    addPrefixToSetting(tdb->settingsHash, "parent", prefix);
    reprefixString(&tdb->track, prefix);
    if (tdb->table != NULL)
        reprefixString(&tdb->table, prefix);
    }
}

void trackHubAddNamePrefix(char *hubName, struct trackDb *tdbList)
/* For a hub named "hub_1" add the prefix "hub_1_" to each track and parent field. */
{
char namePrefix[PATH_LEN];
safef(namePrefix, sizeof(namePrefix), "%s_", hubName);
trackDbListAddNamePrefix(tdbList, namePrefix);
}

char *trackHubGetHubName(char *name)
/* Get the hub_#_ prefix in a hub name. */
{
if ((name == NULL) || !startsWith("hub_", name))
    return NULL;

char *ret = cloneString(name);
char *under = strchr(&ret[4], '_');

if (under)
    {
    *under = 0;
    return ret;
    }

return NULL;
}

char *trackHubSkipHubName(char *name)
/* Skip the hub_#_ prefix in a hub name. */
{
if ((name == NULL) || !startsWith("hub_", name))
    return name;
return strchr(&name[4], '_') + 1;
}

void trackHubAddGroupName(char *hubName, struct trackDb *tdbList)
/* Add group tag that references the hubs symbolic name. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    tdb->grp = cloneString(hubName);
    hashReplace(tdb->settingsHash, "group", tdb->grp);
    }
}

void trackHubAddOneDescription(char *trackDbFile, struct trackDb *tdb)
/* Fetch tdb->track's html description and store in tdb->html. */
{
/* html setting should always be set because we set it at load time */
char *htmlName = trackDbSetting(tdb, "html");
if (htmlName == NULL)
    return;

char *simpleName = hubConnectSkipHubPrefix(htmlName);
char *url = trackHubRelativeUrl(trackDbFile, simpleName);
char buffer[10*1024];
char *fixedUrl = url;
if (!endsWith(url, ".html"))
    {
    safef(buffer, sizeof buffer, "%s.html", url);
    fixedUrl = buffer;
    }
tdb->html = udcFileReadAllIfExists(fixedUrl, NULL, 0, NULL);
freez(&url);
}

void trackHubAddDescription(char *trackDbFile, struct trackDb *tdb)
/* Fetch tdb->track's html description (or nearest ancestor's non-empty description)
 * and store in tdb->html. */
{
trackHubAddOneDescription(trackDbFile, tdb);
if (isEmpty(tdb->html))
    {
    struct trackDb *parent;
    for (parent = tdb->parent;  isEmpty(tdb->html) && parent != NULL;  parent = parent->parent)
	{
	trackHubAddOneDescription(trackDbFile, parent);
	if (isNotEmpty(parent->html))
	    tdb->html = cloneString(parent->html);
	}
    }
}


void trackHubFixName(char *name)
/* Change all characters other than alphanumeric, dash, and underbar
 * to underbar. */
{
if (name == NULL)
    return;

char *in = name;
char c;

for(; (c = *in) != 0; in++)
    {
    if (c == ' ')
	break;

    if (!(isalnum(c) || c == '-' || c == '_'))
	*in = '_';
    }
}

static void polishOneTrack( struct trackHub *hub, struct trackDb *bt,
    struct hash *hash)
/* Get rid of special characters in track name, squirrel away a copy
 * of the original name for html retrieval, make sure there aren't 
 * two tracks with the same name. */
{
char *polished = trackDbLocalSetting(bt, "polished");
if (polished != NULL)
    return;

trackDbAddSetting(bt, "polished", "polished");

char *htmlName = trackDbSetting(bt, "html");
/* if the user didn't specify an html variable, set it to be the original
 * track name */
if (htmlName == NULL)
    trackDbAddSetting(bt, "html", bt->track);

trackHubFixName(bt->track);

if (hashLookup(hash, bt->track) != NULL)
    errAbort("more than one track called %s in hub %s\n", bt->track, hub->url);
hashStore(hash, bt->track);
}

void trackHubPolishTrackNames(struct trackHub *hub, struct trackDb *tdbList)
/* Remove all the special characters from trackHub track names. */
{
struct trackDb *next, *tdb;
struct hash *nameHash = hashNew(5);

for (tdb = tdbList; tdb != NULL; tdb = next)
    {
    if (tdb->parent != NULL)
	polishOneTrack(hub, tdb->parent, nameHash);
    next = tdb->next;
    polishOneTrack(hub, tdb, nameHash);
    if (tdb->subtracks != NULL)
	{
	trackHubPolishTrackNames(hub, tdb->subtracks);
	}
    }
}





void trackHubFindPos(struct cart *cart, char *db, char *term, struct hgPositions *hgp, boolean measureTiming)
/* Look for term in track hubs.  Update hgp if found */
{
findBigBedPosInTdbList(cart, db, hubCollectTracks(db, NULL), term, hgp, NULL, measureTiming);
}

static void parseBlatPcrParams(char *database, char *type, char *setting,
                               char **pHost, char **pPort, char **pGenomeDataDir)
/* parser parameters for either blat or pcr */
{
char *conf = trimSpaces(cloneString(setting));
int numWords = chopByWhite(conf, NULL, 5);
if ((numWords < 2) || (numWords > 4))
    errAbort("invalid configuration for hub %s server, expect 2 or 4 arguments: %s", type, setting);
char *words[4];
chopByWhite(conf, words, numWords);

*pHost = words[0];
*pPort = words[1];
if (numWords > 2)
    {
    if (!sameString(words[2], "dynamic"))
        errAbort("invalid configuration for hub %s server, third argument should be 'dynamic' or omitted, got: %s", type, words[2]);
    *pGenomeDataDir = words[3];
    }
else
    *pGenomeDataDir = NULL;
}

boolean trackHubGetPcrParams(char *database, char **pHost, char **pPort, char **pGenomeDataDir)
/* Get the isPcr params from a trackHub genome. */
{
char *type = "isPcr";
char *setting = trackHubAssemblyField(database, type);
parseBlatPcrParams(database, type, setting, pHost, pPort, pGenomeDataDir);
return TRUE;
}

boolean trackHubGetBlatParams(char *database, boolean isTrans, char **pHost, char **pPort, char **pGenomeDataDir)
{
char *type = isTrans ? "transBlat" : "blat";
char *setting = trackHubAssemblyField(database, type);
if (setting == NULL)
    return FALSE;
parseBlatPcrParams(database, type, setting, pHost, pPort, pGenomeDataDir);
return TRUE;
}

void hubCheckBigDataUrl(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb)
/* Check remote file exists and is of correct type. Wrap this in error catcher */
{
char *relativeUrl = trackDbSetting(tdb, "bigDataUrl");
if (relativeUrl != NULL)
    {
    char *type = trackDbRequiredSetting(tdb, "type");
    char *bigDataUrl = trackHubRelativeUrl(genome->trackDbFile, relativeUrl);

    char *bigDataIndex = NULL;
    char *relIdxUrl = trackDbSetting(tdb, "bigDataIndex");
    if (relIdxUrl != NULL)
        bigDataIndex = trackHubRelativeUrl(genome->trackDbFile, relIdxUrl);

    verbose(2, "checking %s.%s type %s at %s\n", genome->name, tdb->track, type, bigDataUrl);
    if (startsWithWord("bigWig", type))
        {
        /* Just open and close to verify file exists and is correct type. */
        struct bbiFile *bbi = bigWigFileOpen(bigDataUrl);
        bbiFileClose(&bbi);
        }
    /* RMH: Added support for bigRmsk track hub data type */
    else if (startsWithWord("bigNarrowPeak", type) || startsWithWord("bigBed", type) ||
                startsWithWord("bigGenePred", type)  || startsWithWord("bigPsl", type)||
                startsWithWord("bigChain", type)|| startsWithWord("bigMaf", type) ||
                startsWithWord("bigBarChart", type) || startsWithWord("bigInteract", type) ||
                startsWithWord("bigLolly", type) || startsWithWord("bigRmsk",type))
        {
        /* Just open and close to verify file exists and is correct type. */
        struct bbiFile *bbi = bigBedFileOpen(bigDataUrl);
        char *typeString = cloneString(type);
        nextWord(&typeString);
        if (startsWithWord("bigBed", type) && (typeString != NULL))
            {
            unsigned numFields = sqlUnsigned(nextWord(&typeString));
            if (numFields > bbi->fieldCount)
                errAbort("fewer fields in bigBed (%d) than in type statement (%d) for track %s with bigDataUrl %s", bbi->fieldCount, numFields, trackHubSkipHubName(tdb->track), bigDataUrl);
            }
        bbiFileClose(&bbi);
        }
    else if (startsWithWord("vcfTabix", type) || startsWithWord("vcfPhasedTrio", type))
        {
        /* Just open and close to verify file exists and is correct type. */
        struct vcfFile *vcf = vcfTabixFileAndIndexMayOpen(bigDataUrl, bigDataIndex, NULL, 0, 0, 1, 1);
        if (vcf == NULL)
            // Warnings already indicated whether the tabix file is missing etc.
            errAbort("Couldn't open %s and/or its tabix index (.tbi) file.  "
                     "See http://genome.ucsc.edu/goldenPath/help/vcf.html",
                     bigDataUrl);
        vcfFileFree(&vcf);
        }
    else if (startsWithWord("bam", type))
        {
        bamFileAndIndexMustExist(bigDataUrl, bigDataIndex);
        }
    else if (startsWithWord("longTabix", type))
        {
        struct bedTabixFile *btf = bedTabixFileMayOpen(bigDataUrl, NULL, 0, 0);
        if (btf == NULL)
            errAbort("Couldn't open %s and/or its tabix index (.tbi) file.", bigDataUrl);
        bedTabixFileClose(&btf);
        }
#ifdef USE_HAL
    else if (startsWithWord("halSnake", type))
        {
        char *errString;
        int handle = halOpenLOD(bigDataUrl, &errString);
        if (handle < 0)
            errAbort("HAL open error: %s", errString);
        if (halClose(handle, &errString) < 0)
            errAbort("HAL close error: %s", errString);
        }
#endif
    else if (startsWithWord("hic", type))
        {
        struct hicMeta *header;
        char *errString = hicLoadHeader(bigDataUrl, &header, genome->name);
        if (errString != NULL)
            errAbort("hic file error: %s", errString);
        }
    else
        errAbort("unrecognized type %s in genome %s track %s", type, genome->name, tdb->track);
    freez(&bigDataUrl);
    }
}

static void outHubHeader(FILE *f, char *db)
// output a track hub header
{
fprintf(f,"hub quickLiftHub%s\n\
shortLabel Quicklift from %s\n\
longLabel Quicklift from %s\n\
useOneFile on\n\
email genome-www@soe.ucsc.edu\n\n", db, db, db);
fprintf(f,"genome %s\n\n", db);
}

static char *getHubName(struct cart *cart, char *db)
// get the name of the hub to use for quickLifted tracks
{
struct tempName hubTn;
char buffer[4096];
#define quickLiftCartName     "hubQuickLift"
safef(buffer, sizeof buffer, "%s-%s", quickLiftCartName, db);
char *hubName = cartOptionalString(cart, buffer);
int fd = -1;

// we don't reuse userdata paths since they are in save sessions
if ((hubName != NULL) && strstr(hubName, "userdata"))
    hubName = NULL;

if ((hubName == NULL) || ((fd = open(hubName, 0)) < 0))
    {
    trashDirDateFile(&hubTn, "quickLift", "hub", ".txt");
    hubName = cloneString(hubTn.forCgi);
    cartSetString(cart, buffer, hubName);
    }

return hubName;
}

static char *vettedTracks[] =
/* tracks that have been tested with quickLift */
{
"decipherContainer",
"decipherSnvs",
"omimLocation",
"omimAvSnp",
"ncbiRefSeq",
"clinvar",
"clinvarSubLolly",
"pubsBlat",
"pubsMarkerBand",
"pubsMarkerSnp",
"pubsMarkerGene",
//"gtexGeneV8",
};

static boolean isVetted(char *track)
/* Is this a track that's been tested with quickLift?  If not we don't want to do the special name handling on the track. */
{
if (startsWith("wgEncodeGencode", track))
    return TRUE;
static bool inited = FALSE;
static struct hash *vettedHash = NULL;

if (!inited)
    {
    vettedHash = newHash(10);

    int ii;
    int len = sizeof(vettedTracks) / sizeof(char *);

    for(ii = 0; ii < len; ii++)
        hashStore(vettedHash, vettedTracks[ii]);
    }

return hashLookup(vettedHash, track) != NULL;
}

static bool isParentVisible(struct cart *cart, struct trackDb *tdb)
// Are this track's parents visible?
{
if (tdb->parent == NULL)
    return TRUE;
        
if (!isParentVisible(cart, tdb->parent))
    return FALSE;
        
char *cartVis = cartOptionalString(cart, tdb->parent->track);
boolean vis;
if (cartVis != NULL)
    vis =  differentString(cartVis, "hide");
else if (tdbIsSuperTrack(tdb->parent))
    vis = tdb->parent->isShow;
else
    vis = tdb->parent->visibility != tvHide;

return vis;
}

static bool subtrackEnabledInTdb(struct trackDb *subTdb)
/* Return TRUE unless the subtrack was declared with "subTrack ... off". */
{
bool enabled = TRUE;
char *words[2];
char *setting;
if ((setting = trackDbLocalSetting(subTdb, "parent")) != NULL)
    {
    if (chopLine(cloneString(setting), words) >= 2)
        if (sameString(words[1], "off"))
            enabled = FALSE;
    }
else
    return subTdb->visibility != tvHide;

return enabled;
}
static bool isSubtrackVisible(struct cart *cart, struct trackDb *tdb)
/* Has this subtrack not been deselected in hgTrackUi or declared with
 *  * "subTrack ... off"?  -- assumes composite track is visible. */
{
boolean overrideComposite = (NULL != cartOptionalString(cart, tdb->track));
bool enabledInTdb = subtrackEnabledInTdb(tdb);
char option[1024];
safef(option, sizeof(option), "%s_sel", tdb->track);
boolean enabled = cartUsualBoolean(cart, option, enabledInTdb);
if (overrideComposite)
    enabled = TRUE;
return enabled;
}       
            
        
static void dumpTdbAndChildren(struct cart *cart, struct dyString *dy, struct trackDb *tdb)
/* Put a trackDb entry into a dyString, stepping up the tree for some variables. */
{
struct hashCookie cookie = hashFirst(tdb->settingsHash);
struct hashEl *hel;

char *cartVis = cartOptionalString(cart, tdb->track);
if (cartVis != NULL)
    tdb->visibility = hTvFromString(cartVis);
dyStringPrintf(dy, "visibility %s\n", hStringFromTv(tdb->visibility));

if (tdbIsSuperTrack(tdb->parent))
    dyStringPrintf(dy, "parent %s\n", tdb->parent->track);

while ((hel = hashNext(&cookie)) != NULL)
    {   
    if (sameString(hel->name, "parent"))
        {
        char buffer[1024];

        safef(buffer, sizeof buffer, "%s_sel", tdb->track);
        char *cartSelected = cartOptionalString(cart, tdb->track);
        if (cartSelected != NULL)
            {
            char *str = "off";
            if (sameString(cartSelected, "1"))
                str = "on";
            dyStringPrintf(dy, "parent %s %s\n", trackHubSkipHubName(tdb->parent->track), str);
            }
        else
            dyStringPrintf(dy, "%s %s\n", hel->name, trackHubSkipHubName(((char *)hel->val)));
        }
    else if (sameString(hel->name, "html"))
        dyStringPrintf(dy, "%s %s\n", hel->name, trackHubSkipHubName((char *)hel->val));
    else if (differentString(hel->name, "track") && differentString(hel->name, "visibility"))
        dyStringPrintf(dy, "%s %s\n", hel->name, ((char *)hel->val));
    }

if (tdb->subtracks)
    {
    for (tdb = tdb->subtracks; tdb; tdb = tdb->next)
        {
        char *track =  trackHubSkipHubName(tdb->track);
        dyStringPrintf(dy, "\ntrack %s\nquickLifted on\n", track);
        if (!isVetted(track))
            dyStringPrintf(dy, "avoidHandler on\n");
        dumpTdbAndChildren(cart, dy, tdb);
        }
    }
}



struct dyString *trackDbString(struct cart *cart, struct trackDb *tdb)
/* Convert a trackDb entry into a dyString. */
{
struct dyString *dy;

// add a note that the name based handler shouldn't be used on this track
// add a note that this is a quickLifted track so the browser will accept tracks that aren't big*
dy = dyStringNew(200);
char *track =  trackHubSkipHubName(tdb->track);
dyStringPrintf(dy, "track %s\nquickLifted on\n", track);

if (tdbIsSuperTrack(tdb))
    {
    dyStringPrintf(dy, "superTrack on show\n");
    }

if (!isVetted(track))
    dyStringPrintf(dy, "avoidHandler on\n");
    
dumpTdbAndChildren(cart, dy, tdb);

return dy;
}

static boolean validateOneTdb(char *db, struct trackDb *tdb, struct trackDb **badList)
/* Make sure the tdb is a track type we grok. */
{
if (sameString("cytoBandIdeo", trackHubSkipHubName(tdb->track)) || 
    !( startsWith("bigBed", tdb->type) || \
       startsWith("bigWig", tdb->type) || \
       startsWith("bigDbSnp", tdb->type) || \
       startsWith("bigGenePred", tdb->type) || \
       startsWith("gvf", tdb->type) || \
       startsWith("genePred", tdb->type) || \
       startsWith("narrowPeak", tdb->type) || \
       startsWith("bigLolly", tdb->type) || \
       sameString("bed", tdb->type) ||
       startsWith("bed ", tdb->type)))
    {
    slAddHead(badList, tdb);
    return FALSE;
    }

// make sure we have a bigDataUrl
if (startsWith("bigBed", tdb->type) || \
       startsWith("bigWig", tdb->type))
    {
    char *fileName = cloneString(trackDbSetting(tdb, "bigDataUrl"));

    if (fileName == NULL)
        {
        struct sqlConnection *conn = hAllocConnTrack(db, tdb);
        fileName = bbiNameFromSettingOrTable(tdb, conn, tdb->table);
        hashAdd(tdb->settingsHash, "bigDataUrl", fileName);
        hFreeConn(&conn);
        }
    }

return TRUE;
}

static struct trackDb * validateTdbChildren(struct cart *cart, char *db, struct trackDb *tdb, struct trackDb **badList)
/* return a list of the children that can be quick lifted */
{
struct trackDb *validTdbs = NULL;
struct trackDb *nextTdb;
unsigned count = 0;

if (tdb->subtracks)  // this is a view, descend again
    {
    struct trackDb *view = tdb;
    for (; view; view = nextTdb)
        {
        nextTdb = view->next;
        view->subtracks = validateTdbChildren(cart, db,view->subtracks, badList);

        if (view->subtracks != NULL)
            {
            slAddHead(&validTdbs, view);
            if (view->visibility)
                count++;
            }
        }
    }
else
    {
    for(; tdb; tdb = nextTdb)
        {
        nextTdb = tdb->next;
        if (validateOneTdb(db, tdb, badList))
            {
            slAddHead(&validTdbs, tdb);
            if (isSubtrackVisible(cart, tdb))
                count++;
            }
        }
    }
if (count)
    return validTdbs;

return NULL;
}

static boolean validateTdb(struct cart *cart, char *db, struct trackDb *tdb, struct trackDb **badList)
// make sure we only output track types that can
// be quickLifted.  Return true if we any tracks survive
{
if (tdb->subtracks)
    {
    tdb->subtracks = validateTdbChildren(cart, db, tdb->subtracks, badList);

    if (tdb->subtracks == NULL)
        return FALSE;
    return TRUE;
    }

return validateOneTdb(db, tdb, badList);
}

static void outTrack(FILE *f, struct cart *cart, struct trackDb *tdb, unsigned priority)
/* Set priority and output track to hub. */
{
char buffer[1024];

safef(buffer, sizeof buffer, "%d", priority);
hashReplace(tdb->settingsHash, "priority", cloneString(buffer));

struct dyString *dy = trackDbString(cart, tdb);
fprintf(f, "%s\n", dy->string);
}

static boolean checkCartVisibility(struct cart *cart, struct trackDb *tdb)
{
char *cartVis = cartOptionalString(cart, tdb->track);
if (cartVis != NULL)
    tdb->visibility = hTvFromString(cartVis);
return (tdb->visibility != tvHide);
}

static void walkTree(FILE *f, char *db, struct cart *cart,  struct trackDb *tdb, struct dyString *visDy, struct trackDb **badList)
/* walk tree looking for visible tracks to output to hub. */
{
unsigned priority = 1;
struct hash *haveSuper = newHash(0);
struct trackDb *tdbNext = NULL;

for(; tdb; tdb = tdbNext)
    {
    tdbNext = tdb->next;

    boolean isVisible =  FALSE;

    if (tdb->parent == NULL)
        isVisible = checkCartVisibility(cart, tdb);
    else if (isParentVisible(cart, tdb) &&  isSubtrackVisible(cart, tdb)) // child of supertrack
        {
        if (hashLookup(haveSuper, tdb->parent->track) == NULL)  // output yet?
            {
            //if (checkCartVisibility(cart, tdb->parent))
                {
                tdb->parent->visibility = hTvFromString("tvShow");
                outTrack(f, cart, tdb->parent, priority++);

                hashStore(haveSuper, tdb->parent->track);
                }
            }
        isVisible = checkCartVisibility(cart, tdb);
        }

    if (isVisible && validateTdb(cart, db, tdb, badList))
        {
        hashRemove(tdb->settingsHash, "superTrack");   // this gets inherited by subTracks(?)

        // is this a custom track?
        char *tdbType = trackDbSetting(tdb, "tdbType");
        if (tdbType != NULL)
            {
            hashReplace(tdb->settingsHash, "type", tdbType);
            hashReplace(tdb->settingsHash, "shortLabel", trackDbSetting(tdb, "name"));
            hashReplace(tdb->settingsHash, "longLabel", trackDbSetting(tdb, "description"));
            }

        outTrack(f, cart, tdb, priority++);
        }
    }
}

static int cmpPriority(const void *va, const void *vb)
/* Compare to sort based on priority; use shortLabel as secondary sort key. */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
float dif = 0;

dif = a->groupPriority - b->groupPriority;
if (dif == 0)
    dif = a->priority - b->priority;
if (dif < 0)
   return -1;
else if (dif == 0.0)
    /* secondary sort on label */
    return strcasecmp(a->shortLabel, b->shortLabel);
else
   return 1;
}

char *trackHubBuild(char *db, struct cart *cart, struct dyString *visDy, struct trackDb **badList)
/* Build a track hub using trackDb and the cart. */
{
struct  trackDb *tdbList, *tdb;
struct grp *grpList;
cartTrackDbInit(cart, &tdbList, &grpList, FALSE);

struct hash *groupHash = newHash(0);
struct grp *grp;
for(grp = grpList; grp; grp = grp->next)
    hashAdd(groupHash, grp->name, grp);

for(tdb = tdbList; tdb; tdb = tdb->next)
    {
    grp = hashFindVal(groupHash, tdb->grp);
    tdb->groupPriority = grp->priority;
    }
slSort(&tdbList, cmpPriority);

char *filename = getHubName(cart, db);

FILE *f = mustOpen(filename, "w");
chmod(filename, 0666);
outHubHeader(f, trackHubSkipHubName(db));

walkTree(f, db, cart, tdbList, visDy, badList);
fclose(f);

return cloneString(filename);
}

struct grp *trackHubGetGrps()
/* Get the groups defined by attached track hubs. */
{
return trackHubGrps;
}

struct trackDb *trackHubAddTracksGenome(struct trackHubGenome *hubGenome)
/* Load up stuff from data hub and return list. */
{
/* Load trackDb.ra file and make it into proper trackDb tree */
struct trackDb *tdbList = NULL;

if (hubGenome != NULL)
    {
    boolean doCache = trackDbCacheOn();

    if (doCache)
        {
        // we have to open the trackDb file to get the udc cache to check for an update
        struct udcFile *checkCache = udcFileMayOpen(hubGenome->trackDbFile, NULL);
        if (checkCache != NULL)
            {
            time_t time = udcUpdateTime(checkCache);
            udcFileClose(&checkCache);

            struct trackDb *cacheTdb = trackDbHubCache(hubGenome->trackDbFile, time);

            if (cacheTdb != NULL)
                return cacheTdb;
            }

        memCheckPoint(); // we want to know how much memory is used to build the tdbList
        }

    struct dyString *incFiles = newDyString(4096);
    boolean foundFirstGenome = FALSE;
    tdbList = trackHubTracksForGenome(hubGenome->trackHub, hubGenome, incFiles, &foundFirstGenome);
    tdbList = trackDbLinkUpGenerations(tdbList);
    tdbList = trackDbPolishAfterLinkup(tdbList, hubGenome->name);
    trackDbPrioritizeContainerItems(tdbList);
    trackHubPolishTrackNames(hubGenome->trackHub, tdbList);

    if (doCache)
        trackDbHubCloneTdbListToSharedMem(hubGenome->trackDbFile, tdbList, memCheckPoint(), incFiles->string);
    }
return tdbList;
}
