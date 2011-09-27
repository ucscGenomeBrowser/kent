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

static boolean hasProtocol(char *urlOrPath)
/* Return TRUE if it looks like it has http://, ftp:// etc. */
{
return stringIn("://", urlOrPath) != NULL;
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
if (hasProtocol(hubUrl))
    return htmlExpandUrl(hubUrl, path);

/* If we got to here hub is local, and so is path.  Do standard
 * path parsing. */
return pathRelativeToFile(hubUrl, path);
}

static void badGenomeStanza(struct lineFile *lf)
/* Put up semi-informative error message about a genome stanza being bad. */
{
errAbort("Genome stanza should have exactly two lines, one with 'genome' and one with 'trackDb'\n"
         "Bad stanza format ending line %d of %s", lf->lineIx, lf->fileName);
}

static struct trackHubGenome *trackHubGenomeReadRa(char *url, struct hash *hash)
/* Read in a genome.ra format url and return it as a list of trackHubGenomes. 
 * Also add it to hash, which is keyed by genome. */
{
struct lineFile *lf = udcWrapShortLineFile(url, NULL, 16*1024*1024);
struct trackHubGenome *list = NULL, *el;

struct hash *ra;
while ((ra = raNextRecord(lf)) != NULL)
    {
    if (ra->elCount != 2)
	badGenomeStanza(lf);
    char *genome = hashFindVal(ra, "genome");
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
    hashAdd(hash, el->name, el);
    slAddHead(&list, el);
    hashFree(&ra);
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

struct trackHub *trackHubOpen(char *url, char *hubName)
/* Open up a track hub from url.  Reads and parses hub.txt and the genomesFile. 
 * The hubName is generally just the asciified ID number. */
{
struct lineFile *lf = udcWrapShortLineFile(url, NULL, 256*1024);
struct hash *hubRa = raNextRecord(lf);
if (hubRa == NULL)
    errAbort("empty %s in trackHubOpen", url);
if (raNextRecord(lf) != NULL)
    errAbort("multiple records in %s", url);

/* Allocate hub and fill in settings field and url. */
struct trackHub *hub;
AllocVar(hub);
hub->url = cloneString(url);
hub->name = cloneString(hubName);
hub->settings = hubRa;

/* Fill in required fields from settings. */
hub->shortLabel = trackHubRequiredSetting(hub, "shortLabel");
hub->longLabel = trackHubRequiredSetting(hub, "longLabel");
hub->genomesFile = trackHubRequiredSetting(hub, "genomesFile");

lineFileClose(&lf);
char *genomesUrl = trackHubRelativeUrl(hub->url, hub->genomesFile);

hub->genomeHash = hashNew(8);
hub->genomeList = trackHubGenomeReadRa(genomesUrl, hub->genomeHash);
freez(&genomesUrl);

return hub;
}

void trackHubClose(struct trackHub **pHub)
/* Close up and free resources from hub. */
{
struct trackHub *hub = *pHub;
if (hub != NULL)
    {
    trackHubGenomeFreeList(&hub->genomeList);
    freeMem(hub->url);
    hashFree(&hub->settings);
    hashFree(&hub->genomeHash);
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

void trackHubGenomeFreeList(struct trackHubGenome **pList)
/* Free a list of dynamically allocated trackHubGenome's */
{
struct trackHubGenome *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    trackHubGenomeFree(&el);
    }
*pList = NULL;
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
}

struct trackHubGenome *trackHubFindGenome(struct trackHub *hub, char *genomeName)
/* Return trackHubGenome of given name associated with hub.  Return NULL if no
 * such genome. */
{
return hashFindVal(hub->genomeHash, genomeName);
}

static void validateOneTrack( struct trackHub *hub, 
    struct trackHubGenome *genome, struct trackDb *tdb)
{
/* Check for existence of fields required in all tracks */
requiredSetting(hub, genome, tdb, "shortLabel");
requiredSetting(hub, genome, tdb, "longLabel");

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
/* mark containers that are parents, or have them */
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
/* make sure a hub track list has the right settings and its parents exist */
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
struct lineFile *lf = udcWrapShortLineFile(genome->trackDbFile, NULL, 16*1024*1024);
struct trackDb *tdbList = trackDbFromOpenRa(lf, NULL);
lineFileClose(&lf);

/* Make bigDataUrls more absolute rather than relative to genome.ra dir */
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    expandBigDataUrl(hub, genome, tdb);

validateTracks(hub, genome, tdbList);

trackDbAddTableField(tdbList);
trackHubAddNamePrefix(hub->name, tdbList);
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

static int hubCheckTrack(struct trackHub *hub, struct trackHubGenome *genome, 
    struct trackDb *tdb, struct dyString *errors)
/* Make sure that track is ok. */
{
struct errCatch *errCatch = errCatchNew();
char *relativeUrl = trackDbSetting(tdb, "bigDataUrl");
int retVal = 0;

if (relativeUrl != NULL)
    {
    if (errCatchStart(errCatch))
	{
	char *bigDataUrl = trackHubRelativeUrl(genome->trackDbFile, relativeUrl);
	char *type = trackDbRequiredSetting(tdb, "type");
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
	else if (startsWithWord("bam", type))
	    {
	    /* For bam files, the following call checks both main file and index. */
	    bamFileExists(bigDataUrl);
	    }
	else
	    errAbort("unrecognized type %s in genome %s track %s", type, genome->name, tdb->track);
	freez(&bigDataUrl);
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	{
	retVal = 1;
	dyStringPrintf(errors, "%s", errCatch->message->string);
	}
    errCatchFree(&errCatch);
    }

return retVal;
}

static int hubCheckGenome(struct trackHub *hub, struct trackHubGenome *genome,
    struct dyString *errors)
/* Check out genome within hub. */
{
struct errCatch *errCatch = errCatchNew();
struct trackDb *tdbList = NULL;
int retVal = 0;

if (errCatchStart(errCatch))
    tdbList = trackHubTracksForGenome(hub, genome);
errCatchEnd(errCatch);

if (errCatch->gotError)
    {
    retVal = 1;
    dyStringPrintf(errors, "%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    retVal |= hubCheckTrack(hub, genome, tdb, errors);
verbose(2, "%d tracks in %s\n", slCount(tdbList), genome->name);

return retVal;
}

int trackHubCheck(char *hubUrl, struct dyString *errors)
/* hubCheck - Check a track data hub for integrity. Put errors in dyString.
 *      return 0 if hub has no errors, 1 otherwise */
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *hub = NULL;
int retVal = 0;

if (errCatchStart(errCatch))
    hub = trackHubOpen(hubUrl, "");
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
struct trackHubGenome *genome;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    retVal |= hubCheckGenome(hub, genome, errors);
    }
trackHubClose(&hub);

return retVal;
}
