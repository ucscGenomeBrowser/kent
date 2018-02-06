/* hubCheck - Check a track data hub for integrity.. */

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
#include "bedTabix.h"

#ifdef USE_HAL
#include "halBlockViz.h"
#endif

static int cacheTime = 1;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubCheck - Check a track data hub for integrity.\n"
  "usage:\n"
  "   hubCheck http://yourHost/yourDir/hub.txt\n"
  "options:\n"
  "   -noTracks             - don't check remote files for tracks, just trackDb (faster)\n"
  "   -checkSettings        - check trackDb settings to spec\n"
  "   -version=[v?|url]     - version to validate settings against\n"
  "                                     (defaults to version in hub.txt, or current standard)\n"
  "   -extra=[file|url]     - accept settings in this file (or url)\n"
  "   -level=base|required  - reject settings below this support level\n"
  "   -settings             - just list settings with support level\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs.\n"
  "                                     Will create this directory if not existing\n"
  "   -printMeta            - print the metadaa for each track\n"
  "   -cacheTime=N          - set cache refresh time in seconds, default %d\n"
  "   -verbose=2            - output verbosely\n"
  , cacheTime
  );
}

static struct optionSpec options[] = {
   {"version", OPTION_STRING},
   {"level", OPTION_STRING},
   {"extra", OPTION_STRING},
   {"noTracks", OPTION_BOOLEAN},
   {"settings", OPTION_BOOLEAN},
   {"checkSettings", OPTION_BOOLEAN},
   {"test", OPTION_BOOLEAN},
   {"printMeta", OPTION_BOOLEAN},
   {"udcDir", OPTION_STRING},
   {"specHost", OPTION_STRING},
   {"cacheTime", OPTION_INT},
   {NULL, 0},
};

struct trackHubCheckOptions
/* How to check track hub */
    {
    boolean checkFiles;         /* check remote files exist and are correct type */
    boolean checkSettings;      /* check trackDb settings to spec */
    boolean printMeta;          /* print out the metadata for each track */
    char *version;              /* hub spec version to check */
    char *specHost;             /* server hosting hub spec */
    char *level;                /* check hub is valid to this support level */
    char *extraFile;            /* name of extra file/url with additional settings to accept */
    /* intermediate data */
    struct hash *settings;      /* supported settings for this version */
    struct hash *extra;         /* additional trackDb settings to accept */
    struct slName *suggest;     /* list of supported settings for suggesting */
    };

struct trackHubSettingSpec
/* Setting name and support level, from trackDbHub.html (the spec) */
    {
    struct trackHubSettingSpec *next;
    char *name;                 /* setting name */
    char *level;                /* support level (required, base, full, new, deprecated) */
    };


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
char *best = NULL;
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

struct htmlPage *trackHubVersionSpecMustGet(char *specHost, char *version)
/* Open the trackDbHub.html file and attach html reader. Use default version if NULL */
{
char *specUrl;
char buf[256];
if (version != NULL && startsWith("http", version))
    specUrl = version;
else
    {
    safef(buf, sizeof buf, "http://%s/goldenPath/help/trackDb/trackDbHub%s%s.html", 
                        specHost, version ? "." : "", version ? version: "");
    specUrl = buf;
    }
verbose(2, "Using spec at %s\n", specUrl);
struct htmlPage *page = htmlPageGet(specUrl);
if (page == NULL)
    errAbort("Can't open hub settings spec %s", specUrl);

//TODO: apply page validator
//htmlPageValidateOrAbort(page);  // would like to use this, but current page doesn't validate
// Would need to replace empty table (replaced by JS) with div, and assure htmlPageValidateOrAbort
// is run on any page change.

/* TODO: validate this is a trackDbHub spec */
/* (e.g. scan tags for the hub version, perhaps limiting to first N tags) */
return page;
}


char *trackHubVersionDefault(char *specHost, struct htmlPage **pageRet)
/* Return default version of hub spec by parsing for version id in the page */
{
struct htmlPage *page = trackHubVersionSpecMustGet(specHost, NULL);
struct htmlTag *tag;

/* Find version tag (span id=) */
char buf[256];
verbose(6, "Found %d tags\n", slCount(page->tags));
for (tag = page->tags; tag != NULL; tag = tag->next)
    {
    if (sameString(strLower(tag->name), "span") &&
        tag->attributes != NULL &&
        sameString(strLower(tag->attributes->name), "id") &&
        sameString(tag->attributes->val, "trackDbHub_version"))
            {
            int len = min(tag->next->start - tag->end, sizeof buf - 1);
            memcpy(buf, tag->end, len);
            buf[len] = 0;
            verbose(6, "Found version: %s\n", buf);
            return cloneString(buf);
            }
    }
return NULL;
}


int trackHubSettingLevel(struct trackHubSettingSpec *spec)
/* Get integer for level  (required > base > full > new > deprecated). -1 for unknown */
{
if (sameString(spec->level, "required"))
    return 5;
if (sameString(spec->level, "base"))
    return 4;
if (sameString(spec->level, "full"))
    return 3;
if (sameString(spec->level, "new"))
    return 2;
if (sameString(spec->level, "deprecated"))
    return 1;
if (sameString(spec->level, "all"))     //used only as check option
    return 0;
return -1; // unknown
}


boolean trackHubSettingLevelCmp(struct trackHubSettingSpec *spec1, struct trackHubSettingSpec *spec2)
{
/* Compare setting levels */
return trackHubSettingLevel(spec1) - trackHubSettingLevel(spec2);
}


struct trackHubSettingSpec *trackHubSettingsForVersion(char *specHost, char *version)
/* Return list of settings with support level. Version can be version string or spec url */
{
struct htmlPage *page = NULL;
if (version == NULL)
    {
    version = trackHubVersionDefault(specHost, &page);
    if (version == NULL)
        errAbort("Can't get default spec from host %s", specHost);
    }

/* Retrieve specs from file url. 
 * Settings are the first text word within any <code> tag having class="level-" attribute.
 * The level represents the level of support for the setting (e.g. base, full, deprecated)
 * The support level ('level-*') is the class value of the * <code> tag.
 * E.g.  <code class="level-full">boxedConfig on</code> produces:
 *      setting=boxedConfig, class=full */

if (page == NULL)
    page = trackHubVersionSpecMustGet(specHost, version);
if (page == NULL)
    errAbort("Can't get settings spec for version %s from host %s", version, specHost);
verbose(5, "Found %d tags\n", slCount(page->tags));

struct trackHubSettingSpec *spec, *savedSpec;
struct hash *specHash = hashNew(0);
struct htmlTag *tag;
struct htmlAttribute *attr;
char buf[256];
for (tag = page->tags; tag != NULL; tag = tag->next)
    {
    if (differentWord(tag->name, "code"))
        continue;
    attr = tag->attributes;
    if (attr == NULL || differentString(attr->name, "class") || !startsWith("level-", attr->val))
                        continue;
    AllocVar(spec);
    int len = min(tag->next->start - tag->end, sizeof buf - 1);
    memcpy(buf, tag->end, len);
    buf[len] = 0;
    verbose(6, "Found spec: %s\n", buf);
    spec->name = cloneString(firstWordInLine(buf));
    spec->level = cloneString(chopPrefixAt(attr->val, '-'));
    verbose(6, "spec: name=%s, level=%s\n", spec->name, spec->level);
    savedSpec = (struct trackHubSettingSpec *)hashFindVal(specHash, spec->name);
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
struct hashEl *el, *list = hashElListHash(specHash);

int settingsCt = slCount(list);
verbose(5, "Found %d settings's\n", slCount(list));
if (settingsCt == 0)
    errAbort("Can't find hub setting info for version %s (host %s)."
              " Use -version to indicate a different version number or url.", version, specHost);

slSort(&list, hashElCmp);
struct trackHubSettingSpec *specs = NULL;
int baseCt = 0;
int requiredCt = 0;
int deprecatedCt = 0;
for (el = list; el != NULL; el = el->next)
    {
    if (sameString(((struct trackHubSettingSpec *)el->val)->level, "base"))
        baseCt++;
    else if (sameString(((struct trackHubSettingSpec *)el->val)->level, "required"))
        requiredCt++;
    else if (sameString(((struct trackHubSettingSpec *)el->val)->level, "deprecated"))
        deprecatedCt++;
    slAddHead(&specs, el->val);
    }
slReverse(&specs);
verbose(3, 
        "Found %d supported settings for this version (%d required, %d base, %d deprecated)\n",
                slCount(specs), requiredCt, baseCt, deprecatedCt);
return specs;
}


int hubSettingsCheckInit(struct trackHub *hub,  struct trackHubCheckOptions *options, 
                        struct dyString *errors)
{
int retVal = 0;

if (hub->version != NULL && options->version == NULL)
    options->version = hub->version;

struct trackHubSettingSpec *hubLevel = NULL;
int level = 0;
if (hub->version != NULL)
    {
    AllocVar(hubLevel);
    if ((level = trackHubSettingLevel(hubLevel)) < 0)
        {
        dyStringPrintf(errors, "Unknown hub support level: %s. Defaulting to 'all'.\n",
                                hub->level);
        retVal = 1;
        }
    else
        options->level = hub->level;
    }
verbose(2, "Checking hub '%s'", hub->longLabel);
if (options->level)
    verbose(2, " for compliance to level '%s' (use -settings to view)", options->level);
verbose(2, "\n");

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    /* make hash of settings for this version, saving in options */
    struct trackHubSettingSpec *setting, *settings = 
                trackHubSettingsForVersion(options->specHost, options->version);
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


int hubCheckTrackSetting(struct trackHub *hub, struct trackDb *tdb, char *setting, 
                                struct trackHubCheckOptions *options, struct dyString *errors)
/* Check trackDb setting to spec (by version and level). Returns non-zero if error, msg in errors */
{
int retVal = 0;

verbose(4, "    Check setting '%s'\n", setting);
/* skip internally added/used settings */
if (sameString(setting, "polished") || sameString(setting, "group"))
    return 0;

/* check setting is in extra file of supported settings */
if (options->extra && hashLookup(options->extra, setting))
        return 0;

/* check setting is supported in this version */
struct trackHubSettingSpec *hubSetting = hashFindVal(options->settings, setting);
if (hubSetting == NULL)
    {
    dyStringPrintf(errors, "Setting '%s' is unknown/unsupported", setting);
    char *suggest = suggestSetting(setting, options);
    if (suggest != NULL)
        dyStringPrintf(errors, " (did you mean '%s' ?)", suggest);
    dyStringPrintf(errors, "\n");
    retVal = 1;
    }
else if (sameString(hubSetting->level, "deprecated"))
    {
    dyStringPrintf(errors, "Setting '%s' is deprecated\n", setting);
    retVal = 1;
    }
else
    {
    /*  check level */
    struct trackHubSettingSpec *checkLevel = NULL;
    AllocVar(checkLevel);
    checkLevel->level = options->level;
    if (trackHubSettingLevel(hubSetting) < trackHubSettingLevel(checkLevel))
        {
        dyStringPrintf(errors, "Setting '%s' is level '%s'\n", setting, hubSetting->level);
        retVal = 1;
        }
    freez(&checkLevel);
    }
return retVal;
}


void hubCheckTrackFile(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb)
/* Check remote file exists and is of correct type. Wrap this in error catcher */
{
char *relativeUrl = trackDbSetting(tdb, "bigDataUrl");
if (relativeUrl != NULL)
    {
    char *type = trackDbRequiredSetting(tdb, "type");
    char *bigDataUrl = trackHubRelativeUrl(genome->trackDbFile, relativeUrl);

    char *bigDataIndex = NULL;
    char *relIdxUrl = trackDbSetting(tdb, "bigDataIndex");
    if (relIdxUrl != NULL)
        bigDataIndex = trackHubRelativeUrl(genome->trackDbFile, relIdxUrl);

    verbose(2, "checking %s.%s type %s at %s\n", genome->name, tdb->track, type, bigDataUrl);
    if (startsWithWord("bigWig", type))
        {
        /* Just open and close to verify file exists and is correct type. */
        struct bbiFile *bbi = bigWigFileOpen(bigDataUrl);
        bbiFileClose(&bbi);
        }
    else if (startsWithWord("bigNarrowPeak", type) || startsWithWord("bigBed", type) || startsWithWord("bigGenePred", type)  || startsWithWord("bigPsl", type)|| startsWithWord("bigChain", type)|| startsWithWord("bigMaf", type) || startsWithWord("bigBarChart", type))
        {
        /* Just open and close to verify file exists and is correct type. */
        struct bbiFile *bbi = bigBedFileOpen(bigDataUrl);
        char *typeString = cloneString(type);
        nextWord(&typeString);
        if (startsWithWord("bigBed", type) && (typeString != NULL))
            {
            unsigned numFields = sqlUnsigned(nextWord(&typeString));
            if (numFields > bbi->fieldCount)
                errAbort("fewer fields in bigBed (%d) than in type statement (%d) for track %s with bigDataUrl %s", bbi->fieldCount, numFields, trackHubSkipHubName(tdb->track), bigDataUrl);
            }
        bbiFileClose(&bbi);
        }
    else if (startsWithWord("vcfTabix", type))
        {
        /* Just open and close to verify file exists and is correct type. */
        struct vcfFile *vcf = vcfTabixFileAndIndexMayOpen(bigDataUrl, bigDataIndex, NULL, 0, 0, 1, 1);
        if (vcf == NULL)
            // Warnings already indicated whether the tabix file is missing etc.
            errAbort("Couldn't open %s and/or its tabix index (.tbi) file.  "
                     "See http://genome.ucsc.edu/goldenPath/help/vcf.html",
                     bigDataUrl);
        vcfFileFree(&vcf);
        }
    else if (startsWithWord("bam", type))
        {
        bamFileAndIndexMustExist(bigDataUrl, bigDataIndex);
        }
    else if (startsWithWord("longTabix", type))
        {
        struct bedTabixFile *btf = bedTabixFileMayOpen(bigDataUrl, NULL, 0, 0);
        if (btf == NULL)
            errAbort("Couldn't open %s and/or its tabix index (.tbi) file.", bigDataUrl);
        bedTabixFileClose(&btf);
        }
#ifdef USE_HAL
    else if (startsWithWord("halSnake", type))
        {
        char *errString;
        int handle = halOpenLOD(bigDataUrl, &errString);
        if (handle < 0)
            errAbort("HAL open error: %s", errString);
        if (halClose(handle, &errString) < 0)
            errAbort("HAL close error: %s", errString);
        }
#endif
    else
        errAbort("unrecognized type %s in genome %s track %s", type, genome->name, tdb->track);
    freez(&bigDataUrl);
    }
}


int hubCheckTrack(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb, 
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

if (options->printMeta)
    {
    struct slPair *metaPairs = trackDbMetaPairs(tdb);

    if (metaPairs != NULL)
        {
        printf("%s\n", trackHubSkipHubName(tdb->track));
        struct slPair *pair;
        for(pair = metaPairs; pair; pair = pair->next)
            {
            printf("\t%s : %s\n", pair->name, (char *)pair->val);
            }
        printf("\n");
        }
    slPairFreeValsAndList(&metaPairs);
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


int hubCheckGenome(struct trackHub *hub, struct trackHubGenome *genome,
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


int trackHubCheck(char *hubUrl, struct trackHubCheckOptions *options, struct dyString *errors)
/* Check a track data hub for integrity. Put errors in dyString.
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
    dyStringPrintf(errors, "%s\n", errCatch->message->string);
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


static void addExtras(char *extraFile, struct trackHubCheckOptions *checkOptions)
/* Add settings from extra file (e.g. for specific hub display site) */
{
verbose(2, "Accepting extra settings in '%s'\n", extraFile);
checkOptions->extraFile = extraFile;
checkOptions->extra = hashNew(0);
struct lineFile *lf = NULL;
if (startsWith("http", extraFile))
    {
    struct dyString *ds = netSlurpUrl(extraFile);
    char *s = dyStringCannibalize(&ds);
    lf = lineFileOnString(extraFile, TRUE, s);
    }
else
    {
    lf = lineFileOpen(extraFile, TRUE);
    }
char *line;
while (lineFileNextReal(lf, &line))
    {
    hashAdd(checkOptions->extra, line, NULL);
    }
lineFileClose(&lf);
verbose(3, "Found %d extra settings\n", hashNumEntries(checkOptions->extra));
}


static void showSettings(struct trackHubCheckOptions *checkOptions)
/* Print settings and levels for the indicated version */
{
struct trackHubSettingSpec *settings = 
            trackHubSettingsForVersion(checkOptions->specHost, checkOptions->version);
struct trackHubSettingSpec *setting = NULL;
AllocVar(setting);
setting->level = checkOptions->level;
int checkLevel = trackHubSettingLevel(setting);
verbose(3, "Showing level %d (%s) and higher\n", checkLevel, setting->level);
freez(&setting);
for (setting = settings; setting != NULL; setting = setting->next)
    {
    if (trackHubSettingLevel(setting) >= checkLevel)
        printf("%s\t%s\n", setting->name, setting->level);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);

if (argc != 2 && !optionExists("settings"))
    usage();

struct trackHubCheckOptions *checkOptions = NULL;
AllocVar(checkOptions);

checkOptions->specHost = (optionExists("test") ? "genome-test.cse.ucsc.edu" : "genome.ucsc.edu");
checkOptions->specHost = optionVal("specHost", checkOptions->specHost);

checkOptions->printMeta = optionExists("printMeta");
checkOptions->checkFiles = !optionExists("noTracks");
checkOptions->checkSettings = optionExists("checkSettings");

struct trackHubSettingSpec *setting = NULL;
AllocVar(setting);
setting->level = optionVal("level", "all");
if (trackHubSettingLevel(setting) < 0)
    {
    fprintf(stderr, "ERROR: Unrecognized support level %s\n\n", setting->level);
    usage();
    }
checkOptions->level = setting->level;

char *version = NULL;
if (optionExists("version"))
    version = optionVal("version", NULL);
checkOptions->version = version;

char *extraFile = optionVal("extra", NULL);
if (extraFile != NULL)
    addExtras(extraFile, checkOptions);

cacheTime = optionInt("cacheTime", cacheTime);
udcSetCacheTimeout(cacheTime);
// UDC cache dir: first check for hg.conf setting, then override with command line option if given.
setUdcCacheDir();
udcSetDefaultDir(optionVal("udcDir", udcDefaultDir()));

if (optionExists("settings"))
    {
    showSettings(checkOptions);
    return 0;
    }

struct dyString *errors = newDyString(1024);
if (trackHubCheck(argv[1], checkOptions, errors))
    {
    // uniquify and count errors
    struct slName *errs = slNameListFromString(errors->string, '\n');
    slUniqify(&errs, slNameCmp, slNameFree);
    int errCount = slCount(errs);
    printf("Found %d problem%s:\n", errCount, errCount == 1 ? "" : "s");
    printf("%s\n", slNameListToString(errs, '\n'));
    return 1;
    }
return 0;
}
