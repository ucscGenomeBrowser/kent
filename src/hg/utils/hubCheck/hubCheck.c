/* hubCheck - Check a track data hub for integrity.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "udc.h"
#include "ra.h"
#include "filePath.h"
#include "htmlPage.h"
#include "trackDb.h"
#include "bigWig.h"
#include "bigBed.h"
#include "dnaseq.h"
#include "bamFile.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubCheck - Check a track data hub for integrity.\n"
  "usage:\n"
  "   hubCheck XXX\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {NULL, 0},
};

struct trackHub 
/* A track hub. */
    {
    struct trackHub *next;

    char *url;		/* URL of hub.ra file. */
    struct trackHubGenome *genomeList;	/* List of associated genomes. */
    struct hash *genomeHash;	/* Hash of genomeList keyed by genome name. */
    struct hash *settings;	/* Settings from hub.ra file. */

    /* Required settings picked out for convenience. All allocated in settings hash */
    char *shortLabel;	/* Hub short label. Not allocated here. */
    char *longLabel;	/* Hub long label. Not allocated here. */
    char *genomesFile;	/* URL to genome.ra file. Not allocated here. */
    };

struct trackHubGenome
/* A genome serviced within a track hub. */
    {
    struct trackHubGenome *next;
    char *name;	/* Something like hg18 or mm9 - a UCSC assembly database name. */
    char *trackDbFile;	/* The base trackDb.ra file. */
    };

static boolean hasProtocol(char *urlOrPath)
/* Return TRUE if it looks like it has http://, ftp:// etc. */
{
return stringIn("://", urlOrPath) != NULL;
}

char *trackHubRelativeUrl(char *hubUrl, char *path)
/* Return full path (in URL form if it's a remote hub) given
 * path possibly relative to hub. Do a freeMem of result
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

struct trackHubGenome *trackHubGenomeReadRa(char *url, struct hash *hash)
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
    errAbort("Missing required setting %s from %s", name, hub->url);
return val;
}

struct trackHub *trackHubOpen(char *url)
/* Open up a track hub from url.  Reads and parses hub.ra and the genomesFile. */
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
    freeMem(hub->url);
    hashFree(&hub->settings);
    hashFree(&hub->genomeHash);
    // free genomeList here. 
    freez(pHub);
    }
}

static char *requiredSetting(struct trackHub *hub, struct trackHubGenome *genome,
	struct trackDb *tdb, char *setting)
/* Fetch setting or give an error message, a little more specific than the
 * error message from trackDbRequiredSetting(). */
{
char *val = trackDbSetting(tdb, setting);
if (val == NULL)
    errAbort("Missing required %s setting in hub %s genome %s track %s", setting,
    	hub->url, genome->name, tdb->track);
return val;
}

static void checkTagsLegal(struct trackHub *hub, struct trackHubGenome *genome,
	struct trackDb *tdb)
/* Make sure that tdb has all the required tags and is of a supported type. */
{
/* Check for existence of fields required in all tracks */
requiredSetting(hub, genome, tdb, "shortLabel");
requiredSetting(hub, genome, tdb, "longLabel");

/* Further checks depend whether it is a container. */
if (tdb->subtracks != NULL)
    {
    if (trackDbSetting(tdb, "compositeTrack"))
        {
	}
    else if (trackDbSetting(tdb, "container"))
        {
	}
    else
        {
	errAbort("Parent track %s is not compositeTrack or container in hub %s genome %s", 
		tdb->track, hub->url, genome->name);
	}
    }
else
    {
    /* Check type field. */
    char *type = requiredSetting(hub, genome, tdb, "type");
    if (startsWithWord("bigWig", type))
	;
    else if (startsWithWord("bigBed", type))
	;
    else if (startsWithWord("bam", type))
	;
    else
	errAbort("Unsupported type %s in hub %s genome %s track %s", type,
	    hub->url, genome->name, tdb->track);

    requiredSetting(hub, genome, tdb, "bigDataUrl");
    }

}

struct trackDb *trackHubTracksForGenome(struct trackHub *hub, struct trackHubGenome *genome)
/* Get list of tracks associated with genome.  Check that it only is composed of legal
 * types.  Do a few other quick checks to catch errors early. */
{
struct lineFile *lf = udcWrapShortLineFile(genome->trackDbFile, NULL, 16*1024*1024);
struct trackDb *tdbList = trackDbFromOpenRa(lf);
lineFileClose(&lf);

/* Connect up subtracks and parents.  Note this loop does not actually move tracks
 * from list to parent subtracks, it just uses the field as a marker. Just do this
 * so when doing error checking can distinguish between container tracks and others.
 * This does have the pleasant side effect of making good error messages for
 * non-existant parents. */
struct trackDb *tdb;
struct hash *hash = hashNew(0);
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    hashAdd(hash, tdb->track, tdb);
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    char *parentName = trackDbLocalSetting(tdb, "parent");
    if (parentName != NULL)
         {
	 struct trackDb *parent = hashFindVal(hash, parentName);
	 if (parent == NULL)
	    errAbort("Parent %s of track %s doesn't exist in hub %s genome %s", parentName,
		tdb->track, hub->url, genome->name);
	 tdb->parent = parent;
	 parent->subtracks = tdb;
	 }
    }
hashFree(&hash);

/* Loop through list checking tags and removing ad-hoc use of parent and subtracks tags. */
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    checkTagsLegal(hub, genome, tdb);
    tdb->parent = tdb->subtracks = NULL;
    }
return tdbList;
}


void hubCheckTrack(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb)
/* Make sure that track is ok. */
{
char *relativeUrl = trackDbSetting(tdb, "bigDataUrl");
if (relativeUrl != NULL)
    {
    char *bigDataUrl = trackHubRelativeUrl(genome->trackDbFile, relativeUrl);
    char *type = trackDbRequiredSetting(tdb, "type");
    verbose(1, "checking %s.%s type %s at %s\n", genome->name, tdb->track, type, bigDataUrl);

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

}

void hubCheckGenome(struct trackHub *hub, struct trackHubGenome *genome)
/* Check out genome within hub. */
{
struct trackDb *tdbList = trackHubTracksForGenome(hub, genome);
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    hubCheckTrack(hub, genome, tdb);
verbose(1, "%d tracks in %s\n", slCount(tdbList), genome->name);
}

void hubCheck(char *hubUrl)
/* hubCheck - Check a track data hub for integrity. */
{
struct trackHub *hub = trackHubOpen(hubUrl);
verbose(1, "hub %s\nshortLabel %s\nlongLabel %s\n", hubUrl, hub->shortLabel, hub->longLabel);
verbose(1, "%s has %d elements\n", hub->genomesFile, slCount(hub->genomeList));
struct trackHubGenome *genome;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    hubCheckGenome(hub, genome);
    }
trackHubClose(&hub);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));
hubCheck(argv[1]);
return 0;
}
