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
#include "trackHub.h"

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
struct trackHub *hub = trackHubOpen(hubUrl, "");
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
