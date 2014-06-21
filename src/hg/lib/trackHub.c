/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "htmshell.h"
#include "bigBedFind.h"

static struct hash *hubCladeHash;  // mapping of clade name to hub pointer
static struct hash *hubAssemblyHash; // mapping of assembly name to genome struct
static struct hash *hubOrgHash;   // mapping from organism name to hub pointer
static struct trackHub *globalAssemblyHubList; // list of trackHubs in the user's cart
static struct hash *trackHubHash;

char *trackHubRelativeUrl(char *hubUrl, char *path)
/* Return full path (in URL form if it's a remote hub) given
 * path possibly relative to hubUrl. Do a freeMem of result
 * when done. */
{
/* If path itself is a URL then just return a copy of it. */
if (hasProtocol(path))
    return cloneString(path);

/* If it's a remote hub, let html path expander handle it. */
if (hasProtocol(hubUrl))
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

struct trackHubGenome *trackHubGetGenome(char *database)
{
if (hubAssemblyHash == NULL)
    errAbort("requesting hub genome with no hubs loaded");

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

struct dbDb *trackHubGetDbDbs(char *clade)
/* Get a list of dbDb structures for all the tracks in this clade/hub. */
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
	    if (hubGenome->twoBitPath != NULL)
		{
		db = makeDbDbFromAssemblyGenome(hubGenome);
		slAddHead(&dbList, db);
		}
	    }
	}
    }

slSort(&dbList, hDbDbCmpOrderKey);
slReverse(&dbList);
return dbList;
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

struct chromInfo *trackHubMaybeChromInfo(char *database, char *chrom)
/* Return a chromInfo structure for just this chrom in this database. 
 * Return NULL if chrom doesn't exist. */
{
struct trackHubGenome *genome = trackHubGetGenome(database);
if (genome == NULL)
    return NULL;

if (genome->tbf == NULL)
    genome->tbf = twoBitOpen(genome->twoBitPath);
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

struct chromInfo *trackHubAllChromInfo(char *database)
/* Return a chromInfo structure for all the chroms in this database. */
{
struct trackHubGenome *genome = trackHubGetGenome(database);
if (genome == NULL)
    return NULL;

if (genome->tbf == NULL)
    genome->tbf = twoBitOpen(genome->twoBitPath);
struct chromInfo *ci, *ciList = NULL;
struct slName *chromList = twoBitSeqNames(genome->twoBitPath);

for(; chromList; chromList = chromList->next)
    {
    AllocVar(ci);
    ci->chrom = cloneString(chromList->name);
    ci->fileName = genome->twoBitPath;
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

static struct grp *readGroupRa(char *groupFileName)
/* Read in the ra file that describes the groups in an assembly hub. */
{
if (groupFileName == NULL)
    return NULL;
struct hash *ra;
struct grp *list = NULL;
struct lineFile *lf = udcWrapShortLineFile(groupFileName, NULL, 16*1024*1024);
while ((ra = raNextRecord(lf)) != NULL)
    {
    struct grp *grp;
    AllocVar(grp);
    slAddHead(&list, grp);

    grp->name = cloneString(getRequiredGrpSetting(ra, "name", lf));
    grp->label = cloneString(getRequiredGrpSetting(ra, "label", lf));
    grp->priority = atof(getRequiredGrpSetting(ra, "priority", lf));
    grp->defaultIsClosed = sqlUnsigned(getRequiredGrpSetting(ra,"defaultIsClosed",lf));
    hashFree(&ra);
    }
if (list)
    slReverse(&list);
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
}

static char *addHubName(char *base, char *hubName)
{
if (base == NULL)
    return NULL;

char buffer[4096];

safef(buffer, sizeof(buffer), "%s_%s", hubName, base);

return cloneString(buffer);
}

static struct trackHubGenome *trackHubGenomeReadRa(char *url, struct trackHub *hub)
/* Read in a genome.ra format url and return it as a list of trackHubGenomes. 
 * Also add it to hash, which is keyed by genome. */
{
struct lineFile *lf = udcWrapShortLineFile(url, NULL, 64*1024*1024);
struct trackHubGenome *list = NULL, *el;
struct hash *hash = hub->genomeHash;

struct hash *ra;
while ((ra = raNextRecord(lf)) != NULL)
    {
    char *twoBitPath = hashFindVal(ra, "twoBitPath");
    char *genome;
    if (twoBitPath != NULL)
	genome = addHubName(hashFindVal(ra, "genome"), hub->name);
    else
	genome = hashFindVal(ra, "genome");
    if (hub->defaultDb == NULL)
	hub->defaultDb = genome;
    if (genome == NULL)
        badGenomeStanza(lf);
    if (hashLookup(hash, genome) != NULL)
        errAbort("Duplicate genome %s in stanza ending line %d of %s",
		genome, lf->lineIx, lf->fileName);
    char *trackDb = hashFindVal(ra, "trackDb");
    if (trackDb == NULL)
        badGenomeStanza(lf);
    AllocVar(el);
    el->name = cloneString(genome);
    el->trackDbFile = trackHubRelativeUrl(url, trackDb);
    el->trackHub = hub;
    hashAdd(hash, el->name, el);
    slAddHead(&list, el);
    char *groups = hashFindVal(ra, "groups");
    if (twoBitPath != NULL)
	{
	//printf("reading genome %s twoBitPath %s\n", genome, el->twoBitPath);
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

	char *htmlPath = hashFindVal(ra, "htmlPath");
	if (htmlPath != NULL)
	    hashReplace(ra, "htmlPath",trackHubRelativeUrl(url, htmlPath));
	if (groups != NULL)
	    el->groups = trackHubRelativeUrl(url, groups);
	addAssembly(genome, el, hub);
	}
    el->settingsHash = ra;
    hashAdd(ra, "hubName", hub->shortLabel);
    }

/* Clean up and go home. */
lineFileClose(&lf);
slReverse(&list);
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

static struct trackHub *grabHashedHub(char *hubName)
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

struct lineFile *lf = udcWrapShortLineFile(url, NULL, 256*1024);
struct hash *hubRa = raNextRecord(lf);
if (hubRa == NULL)
    errAbort("empty %s in trackHubOpen", url);
if (raNextRecord(lf) != NULL)
    errAbort("multiple records in %s", url);

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
hub->genomesFile = trackHubRequiredSetting(hub, "genomesFile");
char *descriptionUrl = trackHubSetting(hub, "descriptionUrl");
if (descriptionUrl != NULL)
    hub->descriptionUrl = trackHubRelativeUrl(hub->url, descriptionUrl);

lineFileClose(&lf);
char *genomesUrl = trackHubRelativeUrl(hub->url, hub->genomesFile);

hub->genomeHash = hashNew(8);
hub->genomeList = trackHubGenomeReadRa(genomesUrl, hub);
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

static void expandBigDataUrl(struct trackHub *hub, struct trackHubGenome *genome,
	struct trackDb *tdb)
/* Expand bigDataUrls so that no longer relative to genome->trackDbFile */
{
struct hashEl *hel = hashLookup(tdb->settingsHash, "bigDataUrl");
if (hel != NULL)
    {
    char *oldVal = hel->val;
    hel->val = trackHubRelativeUrl(genome->trackDbFile, oldVal);
    freeMem(oldVal);
    }

hel = hashLookup(tdb->settingsHash, "searchTrix");
if (hel != NULL)
    {
    char *oldVal = hel->val;
    hel->val = trackHubRelativeUrl(genome->trackDbFile, oldVal);
    freeMem(oldVal);
    }
}

struct trackHubGenome *trackHubFindGenome(struct trackHub *hub, char *genomeName)
/* Return trackHubGenome of given name associated with hub.  Return NULL if no
 * such genome. */
{
return hashFindVal(hub->genomeHash, genomeName);
}

static void validateOneTrack( struct trackHub *hub, 
    struct trackHubGenome *genome, struct trackDb *tdb)
/* Validate a track's trackDb entry. */
{
/* Check for existence of fields required in all tracks */
requiredSetting(hub, genome, tdb, "shortLabel");
requiredSetting(hub, genome, tdb, "longLabel");

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
    if (!(startsWithWord("bigWig", type) ||
          startsWithWord("bigBed", type) ||
#ifdef USE_HAL
          startsWithWord("halSnake", type) ||
#endif
          startsWithWord("vcfTabix", type) ||
          startsWithWord("bam", type)))
	{
	errAbort("Unsupported type '%s' in hub %s genome %s track %s", type,
	    hub->url, genome->name, tdb->track);
	}

    requiredSetting(hub, genome, tdb, "bigDataUrl");
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
    hashAdd(hash, tdb->track, tdb);

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
    validateOneTrack(hub, genome, tdb);

    // clear these two pointers which we set in markContainers
    tdb->subtracks = NULL;
    tdb->parent = NULL;
    }
}

struct trackDb *trackHubTracksForGenome(struct trackHub *hub, struct trackHubGenome *genome)
/* Get list of tracks associated with genome.  Check that it only is composed of legal
 * types.  Do a few other quick checks to catch errors early. */
{
struct lineFile *lf = udcWrapShortLineFile(genome->trackDbFile, NULL, 64*1024*1024);
struct trackDb *tdbList = trackDbFromOpenRa(lf, NULL);
lineFileClose(&lf);

/* Make bigDataUrls more absolute rather than relative to genome.ra dir */
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    expandBigDataUrl(hub, genome, tdb);

validateTracks(hub, genome, tdbList);

trackDbAddTableField(tdbList);
if (!isEmpty(hub->name))
    trackHubAddNamePrefix(hub->name, tdbList);
if (genome->twoBitPath == NULL)
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

static void addOneDescription(char *trackDbFile, struct trackDb *tdb)
/* Fetch tdb->track's html description and store in tdb->html. */
{
/* html setting should always be set because we set it at load time */
char *htmlName = trackDbSetting(tdb, "html");
if (htmlName == NULL)
    return;

char *simpleName = hubConnectSkipHubPrefix(htmlName);
char *url = trackHubRelativeUrl(trackDbFile, simpleName);
char buffer[10*1024];
safef(buffer, sizeof buffer, "%s.html", url);
tdb->html = netReadTextFileIfExists(buffer);
freez(&url);
}

void trackHubAddDescription(char *trackDbFile, struct trackDb *tdb)
/* Fetch tdb->track's html description (or nearest ancestor's non-empty description)
 * and store in tdb->html. */
{
addOneDescription(trackDbFile, tdb);
if (isEmpty(tdb->html))
    {
    struct trackDb *parent;
    for (parent = tdb->parent;  isEmpty(tdb->html) && parent != NULL;  parent = parent->parent)
	{
	addOneDescription(trackDbFile, parent);
	if (isNotEmpty(parent->html))
	    tdb->html = cloneString(parent->html);
	}
    }
}

static int hubCheckTrack(struct trackHub *hub, struct trackHubGenome *genome, 
    struct trackDb *tdb, struct dyString *errors, FILE *searchFp)
/* Make sure that track is ok. */
{
int retVal = 0;
struct errCatch *errCatch = errCatchNew();

if (errCatchStart(errCatch))
    {
    if (searchFp != NULL)
	{
	addOneDescription(genome->trackDbFile, tdb);
	if (tdb->html != NULL)
	    {
	    char *stripHtml =htmlTextStripTags(tdb->html);
	    strSwapChar(stripHtml, '\n', ' ');
	    strSwapChar(stripHtml, '\t', ' ');
	    strSwapChar(stripHtml, '\r', ' ');
	    strSwapChar(stripHtml, ')', ' ');
	    strSwapChar(stripHtml, '(', ' ');
	    strSwapChar(stripHtml, '[', ' ');
	    strSwapChar(stripHtml, ']', ' ');
	    fprintf(searchFp, "%s.%s\t%s\t%s\t%s\n",hub->url, tdb->track, 
		tdb->shortLabel, tdb->longLabel, stripHtml);
	    }
	else
	    fprintf(searchFp, "%s.%s\t%s\t%s\n",hub->url, tdb->track, 
		tdb->shortLabel, tdb->longLabel);
	}
    else 
	{
	char *relativeUrl = trackDbSetting(tdb, "bigDataUrl");
	char *type = trackDbRequiredSetting(tdb, "type");

	if (relativeUrl != NULL)
	    {
	    char *bigDataUrl = trackHubRelativeUrl(genome->trackDbFile, relativeUrl);
	    verbose(2, "checking %s.%s type %s at %s\n", genome->name, tdb->track, type, bigDataUrl);
	    if (startsWithWord("bigWig", type))
		{
		/* Just open and close to verify file exists and is correct type. */
		struct bbiFile *bbi = bigWigFileOpen(bigDataUrl);
		bbiFileClose(&bbi);
		}
	    else if (startsWithWord("bigBed", type))
		{
		/* Just open and close to verify file exists and is correct type. */
		struct bbiFile *bbi = bigBedFileOpen(bigDataUrl);
		bbiFileClose(&bbi);
		}
	    else if (startsWithWord("vcfTabix", type))
		{
		/* Just open and close to verify file exists and is correct type. */
		struct vcfFile *vcf = vcfTabixFileMayOpen(bigDataUrl, NULL, 0, 0, 1, 1);
		if (vcf == NULL)
		    // Warnings already indicated whether the tabix file is missing etc.
		    errAbort("Couldn't open %s and/or its tabix index (.tbi) file.  "
			     "See http://genome.ucsc.edu/goldenPath/help/vcf.html",
			     bigDataUrl);
		vcfFileFree(&vcf);
		}
	    else if (startsWithWord("bam", type))
		{
		bamFileAndIndexMustExist(bigDataUrl);
		}
	    else
		errAbort("unrecognized type %s in genome %s track %s", type, genome->name, tdb->track);
	    freez(&bigDataUrl);
	    }
	}
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    retVal = 1;
    dyStringPrintf(errors, "%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

return retVal;
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
char *polished = trackDbSetting(bt, "polished");
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

static int hubCheckGenome(struct trackHub *hub, struct trackHubGenome *genome,
    struct dyString *errors, boolean checkTracks, FILE *searchFp)
/* Check out genome within hub. */
{
struct errCatch *errCatch = errCatchNew();
struct trackDb *tdbList = NULL;
int retVal = 0;

if (errCatchStart(errCatch))
    {
    tdbList = trackHubTracksForGenome(hub, genome);
    trackHubPolishTrackNames(hub, tdbList);
    }
errCatchEnd(errCatch);

if (errCatch->gotError)
    {
    retVal = 1;
    dyStringPrintf(errors, "%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

if (!checkTracks)
    return retVal;

struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    retVal |= hubCheckTrack(hub, genome, tdb, errors, searchFp);
verbose(2, "%d tracks in %s\n", slCount(tdbList), genome->name);

return retVal;
}

int trackHubCheck(char *hubUrl, struct dyString *errors, 
    boolean checkTracks, FILE *searchFp)
/* hubCheck - Check a track data hub for integrity. Put errors in dyString.
 *      return 0 if hub has no errors, 1 otherwise 
 *      if checkTracks is TRUE, individual tracks are checked
 */

{
struct errCatch *errCatch = errCatchNew();
struct trackHub *hub = NULL;
int retVal = 0;

if (errCatchStart(errCatch))
    hub = trackHubOpen(hubUrl, "hub_0");
errCatchEnd(errCatch);

if (errCatch->gotError)
    {
    retVal = 1;
    dyStringPrintf(errors, "%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

if (hub == NULL)
    return 1;

verbose(2, "hub %s\nshortLabel %s\nlongLabel %s\n", hubUrl, hub->shortLabel, hub->longLabel);
verbose(2, "%s has %d elements\n", hub->genomesFile, slCount(hub->genomeList));

if (searchFp != NULL)
    {
    struct trackHubGenome *genomeList = hub->genomeList;

    for(; genomeList ; genomeList = genomeList->next)
	fprintf(searchFp, "%s\t%s\n",hub->url,  trackHubSkipHubName(genomeList->name));
    fprintf(searchFp, "%s\t%s\t%s\n",hub->url, hub->shortLabel, hub->longLabel);

    if (hub->descriptionUrl != NULL)
	{
	char *html = netReadTextFileIfExists(hub->descriptionUrl);
	char *stripHtml =htmlTextStripTags(html);
	strSwapChar(stripHtml, '\n', ' ');
	strSwapChar(stripHtml, '\t', ' ');
	strSwapChar(stripHtml, '\015', ' ');
	strSwapChar(stripHtml, ')', ' ');
	strSwapChar(stripHtml, '(', ' ');
	strSwapChar(stripHtml, '[', ' ');
	strSwapChar(stripHtml, ']', ' ');
	fprintf(searchFp, "%s\t%s\n",hub->url,  stripHtml);
	}

    return 0;
    }

struct trackHubGenome *genome;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    retVal |= hubCheckGenome(hub, genome, errors, checkTracks, NULL);
    }
trackHubClose(&hub);

return retVal;
}



void trackHubFindPos(char *db, char *term, struct hgPositions *hgp)
/* Look for term in track hubs.  Update hgp if found */
{
struct trackDb *tdbList = NULL;
if (trackHubDatabase(db))
    {
    struct trackHubGenome *genome = trackHubGetGenome(db);
    tdbList = trackHubTracksForGenome(genome->trackHub, genome);
    }
else
    tdbList = hubCollectTracks(db, NULL);

findBigBedPosInTdbList(db, tdbList, term, hgp);
}

boolean trackHubGetBlatParams(char *database, boolean isTrans, char **pHost, char **pPort)
{
char *hostPort;

if (isTrans)
    {
    hostPort = trackHubAssemblyField(database, "transBlat");
    }
else
    {
    hostPort = trackHubAssemblyField(database, "blat");
    }

if (hostPort == NULL)
    return FALSE;
   
hostPort = cloneString(hostPort);

*pHost = nextWord(&hostPort);
if (hostPort == NULL)
    return FALSE;
*pPort = hostPort;

return TRUE;
}
