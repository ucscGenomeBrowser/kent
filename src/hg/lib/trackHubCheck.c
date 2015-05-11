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
#include "htmlPage.h"
#include "trackHub.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

static int hubCheckTrackSettings(struct trackHub *hub, struct trackHubGenome *genome, 
                                struct trackDb *tdb, struct trackHubCheckOptions *options,
                                struct dyString *errors)
/* Check trackDb settings are valid to spec */
{
//char *version = hashFindVal(hub->settings, "version");
//char *level = hashFindVal(hub->settings, "level");
int retVal = 0;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    }
if (errCatch->gotError)
    {
    retVal = 1;
    dyStringPrintf(errors, "%s", errCatch->message->string);
    }
errCatchFree(&errCatch);
return retVal;
}

static int hubCheckTrackFile(struct trackHub *hub, struct trackHubGenome *genome, 
                        struct trackDb *tdb, struct dyString *errors)
/* Make sure that track is ok. */
{
int retVal = 0;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
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
		char *typeString = cloneString(type);
		nextWord(&typeString);
		if (typeString != NULL)
		    {
		    unsigned numFields = sqlUnsigned(nextWord(&typeString));
		    if (numFields > bbi->fieldCount)
			errAbort("fewer fields in bigBed (%d) than in type statement (%d) for track %s with bigDataUrl %s\n", bbi->fieldCount, numFields, trackHubSkipHubName(tdb->track), bigDataUrl);
		    }
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
		char *errString;
		int handle = halOpenLOD(bigDataUrl, &errString);
		if (handle < 0)
		    errAbort("HAL open error: %s\n", errString);
		if (halClose(handle, &errString) < 0)
		    errAbort("HAL close error: %s\n", errString);
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
                struct trackHubCheckOptions *options, struct dyString *errors)
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

struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    retVal |= hubCheckTrackSettings(hub, genome, tdb, options, errors);
    if (options->checkFiles)
        retVal |= hubCheckTrackFile(hub, genome, tdb, errors);
    }
verbose(2, "%d tracks in %s\n", slCount(tdbList), genome->name);

return retVal;
}

char *trackHubVersionDefault()
/* Return current version of trackDb settings spec for hubs */
{
// TODO: get from goldenPath/help/trackDb
    return "V1";
}

struct trackHubSetting *trackHubSettingsForVersion(char *version, char *specUrl)
/* Return list of settings with support level */
{
if (version == NULL)
    version = trackHubVersionDefault();
if (specUrl == NULL)
    {
    char url[256];
    safef(url, sizeof url, "http://genome.ucsc.edu/goldenPath/help/trackDbHub%s%s.html",
        version ? "." : "", version ? version : "");
    specUrl = url;
    }
struct htmlPage *page = htmlPageGet(specUrl);
if (page == NULL)
    errAbort("Can't open trackDb settings spec %s\n", specUrl);
verbose(3, "Opened URL %s\n", specUrl);

/* Retrieve specs from file url. 
 * Settings are the first text word within a <code> element nested in * a <div> having 
 *  attribute class="format".  The support level ('level-*') is the class value of the * <code> tag.
 * E.g.  <div class="format"><code class="level-core">boxedConfig on</code></div> produces:
 *      setting=boxedConfig, class=core */

struct htmlTag *tag, *codeTag;
struct htmlAttribute *attr, *codeAttr;
struct trackHubSetting *spec, *specs = NULL;
verbose(3, "Found %d tags\n", slCount(page->tags));
int divCount = 0;
char buf[256];
for (tag = page->tags; tag != NULL; tag = tag->next)
    {
    if (differentWord(tag->name, "DIV"))
        continue;
    divCount++;
    verbose(5, "<div>%s\n", tag->start);
    for (attr = tag->attributes; attr != NULL; attr = attr->next)
        {
        if (differentWord(attr->name, "class") || differentWord(attr->val, "format"))
            continue;
        // TODO:  Look on code tags (there may be multiple after "format"
        codeTag = tag->next;
        verbose(5, "Found format: tag %s\n", tag->name);
        if (differentWord(codeTag->name, "CODE"))
            break;
        verbose(5, "Found <code>\n");
        for (codeAttr = codeTag->attributes; codeAttr != NULL; codeAttr = codeAttr->next)
            {
            verbose(5, "attr: name=%s, val=%s\n", codeAttr->name, codeAttr->val);
            if (differentWord(codeAttr->name, "class") || !startsWith("level-", codeAttr->val))
                break;
            AllocVar(spec);
            int len = min(codeTag->next->start - codeTag->end, sizeof buf - 1);
            memcpy(buf, codeTag->end, len);
            buf[len] = 0;
            spec->name = cloneString(firstWordInLine(buf));
            spec->level = chopPrefixAt(cloneString(codeAttr->val), '-');
            // TODO: hash to pickup dupes (retain one with level, warn if multiple differ)
            slAddHead(&specs, spec);
            verbose(5, "spec: name=%s, level=%s\n", spec->name, spec->level);
            }
        }
    }
verbose(5, "Found %d <div>'s\n", divCount);
return specs;
}

int trackHubCheck(char *hubUrl, struct trackHubCheckOptions *options, struct dyString *errors)
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

struct trackHubGenome *genome;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    retVal |= hubCheckGenome(hub, genome, options, errors);
    }
trackHubClose(&hub);
return retVal;
}
