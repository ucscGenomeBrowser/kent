/* hubCheck - Check a track data hub for integrity.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "udc.h"
#include "ra.h"
#include "filePath.h"
#include "htmlPage.h"

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
    char *genome;	/* Something like hg18 or mm9 - a UCSC assembly database name. */
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
    el->genome = cloneString(genome);
    el->trackDbFile = trackHubRelativeUrl(url, trackDb);
    hashAdd(hash, el->genome, el);
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
uglyf("genomesUrl is %s\n", genomesUrl);

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

void hubCheck(char *hubUrl)
/* hubCheck - Check a track data hub for integrity. */
{
struct trackHub *hub = trackHubOpen(hubUrl);
uglyf("hub %s\nshortLabel %s\n", hubUrl, hub->shortLabel);
uglyf("%s has %d elements\n", hub->genomesFile, slCount(hub->genomeList));
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
