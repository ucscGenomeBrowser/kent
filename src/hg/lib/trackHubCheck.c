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

static int hubCheckTrackSetting(struct trackHub *hub, struct trackDb *tdb, char *setting, 
                                struct trackHubCheckOptions *options, struct dyString *errors)
/* Check trackDb setting is by spec (by version and level). Returns non-zero if error, msg in errors */
{
struct trackHubSetting *hubSetting = hashFindVal(options->settings, setting);
/* check setting exists in this version */
if (hubSetting == NULL)
    {
    if (options->extra == NULL)
        {
        dyStringPrintf(errors, "Unknown/unsupported trackDb setting '%s' in hub version '%s'", 
                        setting, options->version);
        return 1;
        }
    if (hashFindVal(options->extra, setting) == NULL)
        {
        dyStringPrintf(errors, 
            "Unknown/unsupported trackDb setting '%s' in hub version '%s' with extras file/url '%s'", 
                        setting, options->version, options->extraFile);
        return 1;
        }
    }
// check level
if (!options->strict)
    return 0;

if (differentString(hubSetting->level, "core"))
    {
    dyStringPrintf(errors, 
                "Setting '%s' is level '%s' in version '%s' (not 'core')", 
                                setting, hubSetting->level, options->version);
    return 1;
    }
return 0;
}


static void hubCheckTrackFile(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb)
/* Check remote file exists and is of correct type. Wrap this in error catcher */
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


static int hubCheckTrack(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb, 
                        struct trackHubCheckOptions *options, struct dyString *errors)
/* Check track settings and optionally, files */
{
int retVal = 0;

if (options->settings)
    {
    struct slPair *settings = slPairListFromString(tdb->settings, FALSE);
    struct slPair *setting;
    for (setting = settings; setting != NULL; setting = setting->next)
        {
        retVal |= hubCheckTrackSetting(hub, tdb, setting->name, options, errors);
        }
    /* NOTE: also need to check settings not in this list (other tdb fields) */
    }

if (!options->checkFiles)
    return retVal;

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    hubCheckTrackFile(hub, genome, tdb);
    }
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
    retVal |= hubCheckTrack(hub, genome, tdb, options, errors);
    }
verbose(2, "%d tracks in %s\n", slCount(tdbList), genome->name);

return retVal;
}


char *trackHubVersionDefault()
/* Return current version of trackDb settings spec for hubs */
{
// TODO: get from goldenPath/help/trackDb/trackDbHub.current.html
    return "v1";  // minor rev to v1a, etc.
}


struct trackHubSetting *trackHubSettingsForVersion(char *version)
/* Return list of settings with support level. Version can be version string or spec url */
{
if (version == NULL)
    version = trackHubVersionDefault();
char *specUrl;
if (startsWith("http", version))
    specUrl = version;
else
    {
    char buf[256];
    safef(buf, sizeof buf, "http://genome.ucsc.edu/goldenPath/help/trackDb/trackDbHub.%s.html", 
        version);
    specUrl = buf;
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
    if (differentWord(tag->name, "div"))
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
        verbose(4, "Found <code>\n");
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
            verbose(4, "spec: name=%s, level=%s\n", spec->name, spec->level);
            }
        }
    }
verbose(5, "Found %d <div>'s\n", divCount);
return specs;
}


int trackHubCheck(char *hubUrl, struct trackHubCheckOptions *options, struct dyString *errors)
/* hubCheck - Check a track data hub for integrity. Put errors in dyString.
 *      return 0 if hub has no errors, 1 otherwise 
 *      if options->checkTracks is TRUE, check remote files of individual tracks
 */
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
    dyStringPrintf(errors, "%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

if (hub == NULL)
    return 1;

verbose(2, "hub %s\nshortLabel %s\nlongLabel %s\n", hubUrl, hub->shortLabel, hub->longLabel);
verbose(2, "%s has %d elements\n", hub->genomesFile, slCount(hub->genomeList));

if (hub->version != NULL)
    options->version = hub->version;
else if (options->version == NULL)
    options->version = trackHubVersionDefault();

options->strict = FALSE;
if (hub->level != NULL)
    {
    if (sameString(hub->level, "core") || sameString(hub->level, "strict"))
        options->strict = TRUE;
    else if (differentString(hub->level, "all"))
        {
        dyStringPrintf(errors, 
            "Unknown hub support level: %s (expecting 'core' or 'all'). Defaulting to 'all'.", hub->level);
        retVal = 1;
        }
    }
struct trackHubSetting *settings = NULL;
errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    settings = trackHubSettingsForVersion(options->version);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    retVal = 1;
    dyStringPrintf(errors, "%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

struct trackHubGenome *genome;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    retVal |= hubCheckGenome(hub, genome, options, errors);
    }
trackHubClose(&hub);
return retVal;
}
