#include "common.h"
#include "dystring.h"
#include "trackDb.h"
#include "bigWig.h"
#include "bigBed.h"
#include "errCatch.h"
#include "vcf.h"
#include "hgBam.h"
#include "net.h"
#include "htmshell.h"
#include "trackHub.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

static int hubCheckTrack(struct trackHub *hub, struct trackHubGenome *genome, 
    struct trackDb *tdb, struct dyString *errors, FILE *searchFp)
/* Make sure that track is ok. */
{
int retVal = 0;
struct errCatch *errCatch = errCatchNew();

if (errCatchStart(errCatch))
    {
#ifdef NOTNOW   // at the moment we're not getting text from the HTML
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
#endif
	{
	char *relativeUrl = trackDbSetting(tdb, "bigDataUrl");

	if (relativeUrl != NULL)
	    {
	    char *type = trackDbRequiredSetting(tdb, "type");
	    char *bigDataUrl = trackHubRelativeUrl(genome->trackDbFile, relativeUrl);
	    verbose(2, "checking %s.%s type %s at %s\n", genome->name, tdb->track, type, bigDataUrl);
	    if (startsWithWord("bigWig", type))
		{
		/* Just open and close to verify file exists and is correct type. */
		struct bbiFile *bbi = bigWigFileOpen(bigDataUrl);
		bbiFileClose(&bbi);
		}
	    else if (startsWithWord("bigBed", type) || startsWithWord("bigGenePred", type))
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
#ifdef USE_HAL
	    else if (startsWithWord("halSnake", type))
		{
		int handle = halOpenLOD(bigDataUrl);
		halClose(handle);
		}
#endif
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
