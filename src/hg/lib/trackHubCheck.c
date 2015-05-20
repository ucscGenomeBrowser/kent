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
#include "axt.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif


/* Mini English spell-check using axt sequence alignment code!  From JK
 * Works in this context when thresholded high.  */

static struct axtScoreScheme *scoreSchemeEnglish()
/* Return something that will match just English words more or less. */
{
struct axtScoreScheme *ss;
AllocVar(ss);
ss->gapOpen = 4;
ss->gapExtend = 2;

/* Set up diagonal to match */
int i;
for (i=0; i<256; ++i)
    ss->matrix[i][i] = 2;

/* Set up upper and lower case to match mostly */
int caseDiff = 'A' - 'a';
for (i='a'; i<='z'; ++i)
    {
    ss->matrix[i][i+caseDiff] = 1;
    ss->matrix[i+caseDiff][i] = 1;
    }
return ss;
}


static int scoreWordMatch(char *a, char *b, struct axtScoreScheme *ss)
/* Return alignment score of two words */
{
struct dnaSeq aSeq = { .name = "a", .dna = a, .size = strlen(a)};
struct dnaSeq bSeq = { .name = "b", .dna = b, .size = strlen(b)};
struct axt *axt = axtAffine(&aSeq, &bSeq, ss);
int result = 0;
if (axt != NULL)
    {
    result = axt->score;
    axtFree(&axt);
    }
return result;
}


static char *suggestSetting(char *setting, struct trackHubCheckOptions *options)
/* Suggest a similar word from settings lists.  Suggest only if there is a single good match */
{
char *best;
int bestScore = 0;
int bestCount = 0;
struct slName *suggest;

struct axtScoreScheme *ss = scoreSchemeEnglish();
for (suggest = options->suggest; suggest != NULL; suggest = suggest->next)
    {
    int score = scoreWordMatch(setting, suggest->name, ss);
    if (score < bestScore)
        continue;
    if (score > bestScore)
        {
        best = suggest->name;
        bestScore = score;
        bestCount = 1;
        }
    else
        {
        // same score
        bestCount++;
        }
    }
if (bestCount == 1 && bestScore > 15)
    {
    verbose(3, "suggest %s score: %d\n", best, bestScore);
    return best;
    }
return NULL;
}


static int hubCheckTrackSetting(struct trackHub *hub, struct trackDb *tdb, char *setting, 
                                struct trackHubCheckOptions *options, struct dyString *errors)
/* Check trackDb setting to spec (by version and level). Returns non-zero if error, msg in errors */
{
verbose(4, "    Check setting '%s'\n", setting);

int retVal = 0;
/* skip internally added/used settings */
if (sameString(setting, "polished") || sameString(setting, "group"))
    return 0;

/* check setting is in extra file of supported settings */
if (options->extra && hashLookup(options->extra, setting))
        return 0;

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    /* check setting is supported in this version */
    struct trackHubSetting *hubSetting = hashFindVal(options->settings, setting);
    if (hubSetting == NULL)
        {
        struct dyString *ds = dyStringNew(0);
        dyStringPrintf(ds, "Setting '%s' is unknown/unsupported", setting);
        char *suggest = suggestSetting(setting, options);
        if (suggest != NULL)
            dyStringPrintf(ds, " (did you mean '%s' ?) ", suggest);
        errAbort("%s\n", dyStringCannibalize(&ds));
        }

    // check level
    if (options->strict && differentString(hubSetting->level, "core"))
        errAbort( "Setting '%s' is level '%s'\n", setting, hubSetting->level);
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    dyStringPrintf(errors, "%s", errCatch->message->string);
    retVal = 1;
    }
errCatchFree(&errCatch);
return retVal;
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

if (options->checkSettings && options->settings)
    {
    //verbose(3, "Found %d settings to check to spec\n", slCount(settings));
    verbose(3, "Checking track: %s\n", tdb->shortLabel);
    verbose(3, "Found %d settings to check to spec\n", hashNumEntries(tdb->settingsHash));
    struct hashEl *hel;
    struct hashCookie cookie = hashFirst(tdb->settingsHash);
    while ((hel = hashNext(&cookie)) != NULL)
        retVal |= hubCheckTrackSetting(hub, tdb, hel->name, options, errors);
    /* TODO: ? also need to check settings not in this list (other tdb fields) */
    }

if (!options->checkFiles)
    return retVal;

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    hubCheckTrackFile(hub, genome, tdb);
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

verbose(2, "%d tracks in %s\n", slCount(tdbList), genome->name);
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    retVal |= hubCheckTrack(hub, genome, tdb, options, errors);
    }

return retVal;
}


char *trackHubVersionDefault()
/* Return current version of trackDb settings spec for hubs */
{
// TODO: get from goldenPath/help/trackDb/trackDbHub.current.html
    return "v0";  // minor rev to v1a, etc.
}


int trackHubSettingLevel(struct trackHubSetting *spec)
/* Get integer for level  (core > full > new > deprecated) */
{
if (sameString(spec->level, "core"))
    return 4;
if (sameString(spec->level, "full"))
    return 3;
if (sameString(spec->level, "new"))
    return 2;
if (sameString(spec->level, "deprecated"))
    return 1;
return 0; // errAbort ?
}


boolean trackHubSettingLevelCmp(struct trackHubSetting *spec1, struct trackHubSetting *spec2)
{
/* Compare setting levels */
return trackHubSettingLevel(spec1) - trackHubSettingLevel(spec2);
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
    char *specHost = "genome.ucsc.edu";
    safef(buf, sizeof buf, "http://%s/goldenPath/help/trackDb/trackDbHub.%s.html", 
                        specHost, version);
    specUrl = buf;
    }
verbose(2, "Validating to spec at %s\n", specUrl);
struct htmlPage *page = htmlPageGet(specUrl);
if (page == NULL)
    errAbort("Can't open trackDb settings spec %s\n", specUrl);

//TODO: apply page validator
//htmlPageValidateOrAbort(page);  // would like to use this, but current page doesn't validate
// Would need to replace empty table (replaced by JS) with div, and assure htmlPageValidateOrAbort
// is run on any page change.

/* TODO: validate this is a trackDbHub spec */
/* (scan tags for tag->name="span" tag->attribute="id", attr->"value=trackDbHub_version",
 * might want to limit to first N tags) */

/* Retrieve specs from file url. 
 * Settings are the first text word within any <code> element nested in * a <div> having 
 *  attribute class="format".  The support level ('level-*') is the class value of the * <code> tag.
 * E.g.  <div class="format"><code class="level-core">boxedConfig on</code></div> produces:
 *      setting=boxedConfig, class=core */

struct htmlTag *tag, *codeTag;
struct htmlAttribute *attr, *codeAttr;
struct trackHubSetting *spec, *savedSpec;
struct hash *specHash = hashNew(0);
verbose(5, "Found %d tags\n", slCount(page->tags));
int divCount = 0;
char buf[256];
for (tag = page->tags; tag != NULL; tag = tag->next)
    {
    verbose(6, "    TAG: %s\n", tag->name);
    if (differentWord(tag->name, "DIV"))
        continue;
    divCount++;
    attr = tag->attributes;
    if (differentWord(attr->name, "class") || differentWord(attr->val, "format"))
        continue;
    verbose(7, "Found format: tag %s\n", tag->name);
    // Look for one or more <code> tags in this format div
    for (codeTag = tag->next; 
            codeTag != NULL && differentWord(codeTag->name,"/DIV"); codeTag = codeTag->next)
        {
        if (differentWord(codeTag->name, "CODE"))
            continue;
        verbose(7, "Found <code>\n");
        codeAttr = codeTag->attributes;
        //verbose(8, "attr: name=%s, val=%s\n", codeAttr->name, codeAttr->val);
        if (codeAttr == NULL || differentString(codeAttr->name, "class") ||
                !startsWith("level-", codeAttr->val))
                        continue;
        AllocVar(spec);
        int len = min(codeTag->next->start - codeTag->end, sizeof buf - 1);
        memcpy(buf, codeTag->end, len);
        buf[len] = 0;
        verbose(7, "Found spec: %s\n", buf);
        spec->name = cloneString(firstWordInLine(buf));
        spec->level = cloneString(chopPrefixAt(codeAttr->val, '-'));
        verbose(6, "spec: name=%s, level=%s\n", spec->name, spec->level);

        savedSpec = (struct trackHubSetting *)hashFindVal(specHash, spec->name);
        if (savedSpec != NULL)
            verbose(6, "found spec %s level %s in hash\n", savedSpec->name, savedSpec->level);
        if (savedSpec == NULL)
            {
            hashAdd(specHash, spec->name, spec);
            verbose(6, "added spec %s at level %s\n", spec->name, spec->level);
            }
        else if (trackHubSettingLevelCmp(spec, savedSpec) > 0)
            {
            hashReplace(specHash, spec->name, spec);
            verbose(6, "replaced spec %s at level %s, was %s\n", 
                spec->name, spec->level, savedSpec->level);
            }
        }
    }
verbose(5, "Found %d <div>'s\n", divCount);
struct hashEl *el, *list = hashElListHash(specHash);

int settingsCt = slCount(list);
verbose(5, "Found %d settings's\n", slCount(list));
if (settingsCt == 0)
    errAbort("Can't find trackDb settings support levels at %s."
              " Use -v to indicate a different version number or url.\n", specUrl);

slSort(&list, hashElCmp);
struct trackHubSetting *specs = NULL;
int coreCt = 0;
for (el = list; el != NULL; el = el->next)
    {
    if (sameString(((struct trackHubSetting *)el->val)->level, "core"))
        coreCt++;
    slAddHead(&specs, el->val);
    }
slReverse(&specs);
verbose(3, "Found %d supported settings for this version (%d core)\n",
                        slCount(specs), coreCt);
return specs;
}


static int hubSettingsCheckInit(struct trackHub *hub,  struct trackHubCheckOptions *options, struct dyString *errors)
{
int retVal = 0;
if (hub->version != NULL)
    options->version = hub->version;
else if (options->version == NULL)
    options->version = trackHubVersionDefault();

if (options->strict == FALSE && hub->level != NULL)
    {
    if (sameString(hub->level, "core"))
        options->strict = TRUE;
    else if (differentString(hub->level, "all"))
        {
        dyStringPrintf(errors, 
            "Unknown hub support level: %s (expecting 'core' or 'all'). Defaulting to 'all'.\n", hub->level);
        retVal = 1;
        }
    }
verbose(2, "Checking hub '%s'%s\n", hub->longLabel, options->strict ? " for compliance to 'core' (use -settings to view)": "");

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    /* make hash of settings for this version, saving in options */
    struct trackHubSetting *setting, *settings = trackHubSettingsForVersion(options->version);
    options->settings = newHash(0);
    options->suggest = NULL;
    for (setting = settings; setting != NULL; setting = setting->next)
        {
        hashAdd(options->settings, setting->name, setting);
        slNameAddHead(&options->suggest, setting->name);
        }
    /* TODO: ? also need to check settings not in this list (other tdb fields) */

    // TODO: move extra file handling here (out of hubCheck)
    if (options->extra != NULL)
        {
        struct hashCookie cookie = hashFirst(options->extra);
        struct hashEl *hel;
        while ((hel = hashNext(&cookie)) != NULL)
            slNameAddHead(&options->suggest, hel->name);
        }
    slNameSort(&options->suggest);
    verbose(3, "Suggest list has %d settings\n", slCount(options->suggest));
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

if (options->checkSettings)
    retVal |= hubSettingsCheckInit(hub, options, errors);

struct trackHubGenome *genome;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    retVal |= hubCheckGenome(hub, genome, options, errors);
    }
trackHubClose(&hub);
return retVal;
}
