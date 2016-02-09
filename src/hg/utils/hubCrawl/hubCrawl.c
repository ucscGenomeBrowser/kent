
/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "axt.h"
#include "common.h"
#include "bigWig.h"
#include "bigBed.h"
#include "dystring.h"
#include "errCatch.h"
#include "hgBam.h"
#include "htmshell.h"
#include "htmlPage.h"
#include "hui.h"
#include "net.h"
#include "options.h"
#include "trackDb.h"
#include "trackHub.h"
#include "udc.h"
#include "vcf.h"

static int cacheTime = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubCrawl - Crawl a track data hub and output search strings\n"
  "usage:\n"
  "   hubCrawl http://yourHost/yourDir/hub.txt\n"
  "options:\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs.\n"
  "   -cacheTime=N - set cache refresh time in seconds, default %d\n"
  "   -verbose=2            - output verbosely\n"
  , cacheTime
  );
}

static struct optionSpec options[] = {
   {"udcDir", OPTION_STRING},
   {"cacheTime", OPTION_INT},
   {NULL, 0},
};

int trackHubCrawl(char *hubUrl)
/* Crawl a track data hub and output strings useful in a search */
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *hub = NULL;
int retVal = 0;

if (errCatchStart(errCatch))
    {
    hub = trackHubOpen(hubUrl, "hub_0");
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    retVal = 1;
    fprintf(stderr, "%s\n", errCatch->message->string);
    }
errCatchFree(&errCatch);

if (hub == NULL)
    return 1;

FILE *searchFp =stdout;
struct trackHubGenome *genome;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    fprintf(searchFp, "%s\t%s\n", hub->url, trackHubSkipHubName(genome->name));
    if (isNotEmpty(genome->organism) && differentString(genome->organism, genome->name))
        fprintf(searchFp, "%s\t%s\n", hub->url, trackHubSkipHubName(genome->organism));
    if (isNotEmpty(genome->description))
        fprintf(searchFp, "%s\t%s\n", hub->url, genome->description);
    struct hashEl *hel = NULL;
    if (genome->settingsHash && (hel = hashLookup(genome->settingsHash, "scientificName")) != NULL)
        {
        char *sciName = (char *)(hel->val);
        if (differentString(sciName, genome->name))
            fprintf(searchFp, "%s\t%s\n", hub->url, sciName);
        }
    }
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

trackHubClose(&hub);
return retVal;
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 2)
    usage();


udcSetCacheTimeout(cacheTime);
// UDC cache dir: first check for hg.conf setting, then override with command line option if given.
setUdcCacheDir();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

if (trackHubCrawl(argv[1]))
    {
    return 1;
    }
return 0;
}
