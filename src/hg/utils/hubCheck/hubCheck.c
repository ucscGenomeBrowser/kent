/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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
#include "knetUdc.h"

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
  "   -genome=genome        - only check this genome\n"
  "   -udcDir=/dir/to/cache - place to put cache for remote bigBed/bigWigs.\n"
  "                                     Will create this directory if not existing\n"
  "   -httpsCertCheck=[abort,warn,log,none] - set the ssl certificate verification mode.\n"  
  "   -httpsCertCheckDomainExceptions= - space separated list of domains to whitelist.\n"  
  "   -printMeta            - print the metadata for each track\n"
  "   -cacheTime=N          - set cache refresh time in seconds, default %d\n"
  "   -allowWarnings        - return 0 exit code when only warnings are found (no errors)\n"
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
   {"genome", OPTION_STRING},
   {"test", OPTION_BOOLEAN},
   {"printMeta", OPTION_BOOLEAN},
   {"udcDir", OPTION_STRING},
   {"httpsCertCheck", OPTION_STRING},
   {"httpsCertCheckDomainExceptions", OPTION_STRING},
   {"specHost", OPTION_STRING},
   {"cacheTime", OPTION_INT},
   {"allowWarnings", OPTION_BOOLEAN},
   // intentionally undocumented option for hgHubConnect
   {"htmlOut", OPTION_BOOLEAN},
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
    char *genome;               /* only check this genome */
    /* intermediate data */
    struct hash *settings;      /* supported settings for this version */
    struct hash *extra;         /* additional trackDb settings to accept */
    struct slName *suggest;     /* list of supported settings for suggesting */
    boolean allowWarnings;      /* return 0 exit code for warnings (no errors) */
    /* hgHubConnect only */
    boolean htmlOut;            /* put special formatted text into the errors dyString */
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
    if (spec->name == NULL || strlen(spec->name) == 0)
        {
        warn("ERROR: format problem with trackDbHub.html -- contact UCSC.");
        continue;
        }
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

boolean hasComplexSetting = isComplexSetting(setting);
if (hasComplexSetting)
    {
    verbose(5, "skipping validation for complex setting=%s.", setting);
    return 1;
    }

if (hubSetting == NULL)
    {
    dyStringPrintf(errors, "Setting '%s' is not recognized. Check for typos, or see https://genome.ucsc.edu/goldenPath/help/trackDb/trackDbHub.html for supported settings", setting);
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

boolean extFileExists(char *path)
/* Check that a remote URL actually exists, path may be a URL or a local path relative to hub.txt */
{
// if the file is local check that it exists:
if (!hasProtocol(path) && udcExists(path))
    return TRUE;
else
    {
    // netLineFileSilentOpen will handle 301 redirects and the like
    if (netLineFileSilentOpen(path) != NULL)
        return TRUE;
    }
return FALSE;
}

char *makeFolderObjectString(char *id, char *text, char *parent, char *title, boolean children, boolean openFolder)
/* Construct a folder item for one of the jstree arrays */
{
struct dyString *folderString = dyStringNew(0);
dyStringPrintf(folderString, "{icon: '../../images/folderC.png', id: '%s', "
    "text:'%s', parent:'%s',"
    "li_attr:{title:'%s'}, children:%s, state: {opened: %s}}",
    htmlEncode(id), htmlEncode(text), htmlEncode(parent), title, children ? "true" : "false", openFolder ? "true" : "false");
return dyStringCannibalize(&folderString);
}

char *makeChildObjectString(char *id, char *title, char *shortLabel, char *longLabel,
    char *color, char *name, char *text, char *parent)
/* Construct a single child item for one of the jstree arrays */
{
struct dyString *item = dyStringNew(0);
dyStringPrintf(item, "{icon: 'fa fa-plus', id:'%s', li_attr:{class: 'hubError', title: '%s', "
        "shortLabel: '%s', longLabel: '%s', color: '%s', name:'%s'}, "
        "text:'%s', parent: '%s', state: {opened: true}}",
        htmlEncode(id), title, htmlEncode(shortLabel), htmlEncode(longLabel), color, name, htmlEncode(text), htmlEncode(parent));
return dyStringCannibalize(&item);
}

void hubErr(struct dyString *errors, char *message, struct trackHub *hub, boolean doHtml)
/* Construct the right javascript for the jstree for a top level hub.txt error. */
{
if (!doHtml)
    dyStringPrintf(errors, "%s", message);
else
    {
    char *sl;
    char *strippedMessage = NULL;
    static int count = 0; // force a unique id for the jstree object
    char id[512];
    //TODO: Choose better default labels
    if (hub && hub->shortLabel != NULL)
        {
        sl = hub->shortLabel;
        }
    else
        sl = "Hub Error";
    if (message)
        strippedMessage = cloneString(message);
    stripChar(strippedMessage, '\n');
    safef(id, sizeof(id), "%s%d", sl, count);

    // make the error message
    dyStringPrintf(errors, "trackData['%s'] = [%s];\n", htmlEncode(sl),
        makeChildObjectString(id, "Hub Error", htmlEncode(sl), htmlEncode(sl), "#550073", htmlEncode(sl), strippedMessage, sl));

    count++;
    }
}

void genomeErr(struct dyString *errors, char *message, struct trackHub *hub,
    struct trackHubGenome *genome, boolean doHtml)
/* Construct the right javascript for the jstree for a top-level genomes.txt error or
 * error opening a trackDb.txt file */
{
if (!doHtml)
    dyStringPrintf(errors, "%s", message);
else
    {
    static int count = 0; // forces unique ID's which the jstree object needs
    char id[512];
    char *errorMessages[16];
    char *strippedMessage = NULL;
    char *genomeName = trackHubSkipHubName(genome->name);
    if (message)
        strippedMessage = cloneString(message);
    // multiple errors may be in a single message, chop by newline and make a node in the tree for each message
    int numMessages = chopByChar(strippedMessage, '\n', errorMessages, sizeof(errorMessages));
    int i = 0;
    dyStringPrintf(errors, "trackData['%s'] = [", genomeName);
    for (; i < numMessages && isNotEmpty(errorMessages[i]); i++)
        {
        safef(id, sizeof(id), "%s%d", genomeName, count);
        dyStringPrintf(errors, "%s,", makeChildObjectString(id, "Genome Error", genomeName, genomeName, "#550073", genomeName, errorMessages[i], genomeName));
        count++;
        }
    }
}

void trackDbErr(struct dyString *errors, char *message, struct trackHubGenome *genome, struct trackDb *tdb, boolean doHtml)
/* Adds the right object for a jstree object of trackDb configuration errors.  */
{
if (!doHtml)
    {
    dyStringPrintf(errors, "%s", message);
    }
else
    {
    char *strippedMessage = NULL;
    char *splitMessages[16]; // SUBGROUP_MAX=9 but add a few extra just in case
    char parentOrTrackString[512];
    char id[512];
    static int count = 0; // forces unique ID's which the jstree object needs
    int numMessages = 0;
    int i = 0;

    // if a subtrack is missing multiple subgroups, then message will contain
    // at least two newline separated errors, both should be printed separately:
    if (message)
        {
        strippedMessage = cloneString(message);
        while (lastChar(strippedMessage) == '\n')
            trimLastChar(strippedMessage);
        numMessages = chopByChar(strippedMessage, '\n', splitMessages, sizeof(splitMessages));
        }

    for (; i < numMessages; i++)
        {
        if (isNotEmpty(splitMessages[i]))
            {
            safef(id, sizeof(id), "%s%d", trackHubSkipHubName(tdb->track), count);
            safef(parentOrTrackString, sizeof(parentOrTrackString), "%s_%s", trackHubSkipHubName(genome->name), trackHubSkipHubName(tdb->track));
            dyStringPrintf(errors, "%s,",
                    makeChildObjectString(id, "TrackDb Error", tdb->shortLabel, tdb->longLabel,
                    "#550073", trackHubSkipHubName(tdb->track), splitMessages[i], parentOrTrackString));
            count++;
            }
        }
    }
}

boolean checkEmptyMembersForAll(membersForAll_t *membersForAll, struct trackDb *parentTdb)
/* membersForAll may be allocated and exist but not have any actual members defined. */
{
int i;
for (i = 0; i < ArraySize(membersForAll->members); i++)
    {
    if (membersForAll->members[i] != NULL)
        return TRUE;
    }
return FALSE;
}

int hubCheckSubtrackSettings(struct trackHubGenome *genome, struct trackDb *tdb, struct dyString *errors, struct trackHubCheckOptions *options)
/* Check subtrack specific settings, for example that 'subgroups' are consistent with what
 * is defined at the parent level */
{
int retVal = 0;
if (!tdbIsSubtrack(tdb))
    return retVal;

int i;
char *subtrackName = trackHubSkipHubName(tdb->track);
sortOrder_t *sortOrder = NULL;
membership_t *membership = NULL;
membersForAll_t *membersForAll = NULL;
struct errCatch *errCatch = errCatchNew();

if (errCatchStart(errCatch))
    {
    // check that subtrack group is the same as the parent group:
    char *subTrackGroup = tdb->grp;
    char *parentGroup = tdb->parent->grp;
    if (!sameString(subTrackGroup, parentGroup))
        {
        errAbort("assembly %s: subtrack \"%s\" has 'group %s' but its parent \"%s\" has 'group %s'. Subtracks inherit their parent's group. Either remove the 'group' line from the subtrack, or set both parent and subtrack to the same group.", trackHubSkipHubName(genome->name), subtrackName, subTrackGroup, trackHubSkipHubName(tdb->parent->track), parentGroup);
        }

    // check subgroups settings
    membersForAll = membersForAllSubGroupsGet(tdb->parent, NULL);

    // membersForAllSubGroupsGet() warns about the parent stanza, turn it into an errAbort
    if (errCatch->gotWarning)
        {
        errAbort("%s", errCatch->message->string);
        }

    if (membersForAll && checkEmptyMembersForAll(membersForAll, tdb->parent))
        {
        membership = subgroupMembershipGet(tdb);
        sortOrder = sortOrderGet(NULL, tdb->parent);

        if (membership == NULL)
            {
            errAbort("missing 'subgroups' setting for subtrack %s. Add a 'subGroups' line declaring this subtrack's group membership, e.g. 'subGroups view=signal'", subtrackName);
            }

        // if a sortOrder is defined, make sure every subtrack has that membership
        if (sortOrder)
            {
            for (i = 0; i < sortOrder->count; i++)
                {
                char *col = sortOrder->column[i];
                if ( (!sameString(col, SUBTRACK_COLOR_SUBGROUP)) && (membership == NULL || stringArrayIx(col, membership->subgroups, membership->count) == -1))
                    warn("%s not a member of sortOrder subgroup %s", subtrackName, col);
                }
            }

        // now check that this subtrack is a member of every defined subgroup
        for (i = 0; i < ArraySize(membersForAll->members); i++)
            {
            if (membersForAll->members[i] != NULL)
                {
                char *subgroupName = membersForAll->members[i]->groupTag;
                if (stringArrayIx(subgroupName, membership->subgroups, membership->count) == -1)
                    {
                    warn("subtrack %s not a member of subgroup %s", subtrackName, subgroupName);
                    }
                }
            }

        // check that the subtrack does not have any bogus subgroups that don't exist in the parent
        for (i = 0; i < membership->count; i++)
            {
            char *subgroupName = membership->subgroups[i];
            if (!subgroupingExists(tdb->parent, subgroupName))
                {
                warn("subtrack \"%s\" declares subgroup \"%s\", but the parent composite does not define this subgroup. Check the 'subGroup' lines in the parent stanza for a matching group name.", subtrackName, subgroupName);
                }
            }
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError || errCatch->gotWarning)
    {
    trackDbErr(errors, errCatch->message->string, genome, tdb, options->htmlOut);
    if (errCatch->gotError || !options->allowWarnings)
        retVal = 1;
    }
errCatchFree(&errCatch);

return retVal;
}

int hubCheckCompositeSettings(struct trackHubGenome *genome, struct trackDb *tdb, struct dyString *errors, struct trackHubCheckOptions *options)
/* Check composite level settings like subgroups, dimensions, sortOrder, etc.
 * Note that this function returns an int because we want to warn about all errors in a single
 * composite stanza rather than errAbort on the first error */
{
int retVal = 0;

// for now all the combination style stanzas can get checked here, but
// in the future they might need their own routines
if (! (tdbIsComposite(tdb) || tdbIsCompositeView(tdb) || tdbIsContainer(tdb)) )
    return retVal;

struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    // check that subgroup lines are syntactically correct:
    // "subGroup name Title tag1=value1 ..."
    (void)membersForAllSubGroupsGet(tdb, NULL);

    // check that multiWigs have > 1 track
    char *multiWigSetting = trackDbLocalSetting(tdb, "container");
    if (multiWigSetting && slCount(tdb->subtracks) < 2)
        {
        errAbort("container multiWig %s has only one subtrack. multiWigs must have more than one subtrack", tdb->track);
        }
    }
errCatchEnd(errCatch);

if (errCatch->gotError || errCatch->gotWarning)
    {
    // don't add a new line because one will already be inserted by the errCatch->message
    trackDbErr(errors, errCatch->message->string, genome, tdb, options->htmlOut);
    if (errCatch->gotError || !options->allowWarnings)
        retVal = 1;
    }

return retVal;
}

void hubCheckParentsAndChildren(struct trackDb *tdb)
/* Check that a single trackDb stanza has the correct parent and subtrack pointers */
{
if (tdbIsSuper(tdb) || tdbIsComposite(tdb) || tdbIsCompositeView(tdb) || tdbIsContainer(tdb))
    {
    if (tdb->subtracks == NULL)
        {
        errAbort("Track \"%s\" is declared superTrack, compositeTrack, view or "
            "container, but has no subtracks", tdb->track);
        }

    // Containers should not have a bigDataUrl setting
    if (trackDbLocalSetting(tdb, "bigDataUrl"))
        {
        errAbort("Track \"%s\" is a container (compositeTrack/superTrack/view) but also has "
            "a 'bigDataUrl'. Container tracks organize subtracks and should not have data files. "
            "Remove 'bigDataUrl' from this stanza, or remove the container declaration if this is "
            "a data track.", tdb->track);
        }

    // multiWigs cannot be the child of a composite
    if (tdbIsContainer(tdb) &&
            (tdb->parent != NULL &&
            (tdbIsComposite(tdb->parent) || tdbIsCompositeView(tdb->parent))))
        {
        errAbort("Track \"%s\" is declared container multiWig and has parent \"%s\"."
            " Container multiWig tracks cannot be children of composites or views",
            tdb->track, tdb->parent->track);
        }
    }
else if (tdb->subtracks != NULL)
    {
    errAbort("Track \"%s\" has children tracks (e.g: \"%s\"), but is not a "
        "compositeTrack, container, view or superTrack", tdb->track, tdb->subtracks->track);
    }
}

boolean checkTypeLine(struct trackHubGenome *genome, struct trackDb *tdb, struct dyString *errors, struct trackHubCheckOptions *options)
{
boolean retVal = FALSE;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    char *type = trackDbRequiredSetting(tdb, "type");
    char *splitType[4];
    int numWords = chopByWhite(cloneString(type), splitType, sizeof(splitType));
    char *trackType = splitType[0];
    boolean isParentTrack = (tdbIsComposite(tdb) || tdbIsCompositeView(tdb) || tdbIsContainer(tdb));
    if (!isParentTrack &&
            // these are the valid trackDb types for hub data tracks:
            !(sameString("bigNarrowPeak", trackType) || sameString("bigBed", trackType) ||
                    sameString("bigGenePred", trackType)  || sameString("bigPsl", trackType)||
                    sameString("bigChain", trackType)|| sameString("bigMaf", trackType) ||
                    sameString("bigBarChart", trackType) || sameString("bigInteract", trackType) ||
                    sameString("bigLolly", trackType) || sameString("bigRmsk", trackType) ||
                    sameString("bigWig", trackType) || sameString("longTabix", trackType) ||
                    sameString("vcfTabix", trackType) || sameString("vcfPhasedTrio", trackType) ||
                    sameString("bam", trackType) || sameString("hic", trackType)
            #ifdef USE_HAL
                    || sameString("halSnake", trackType)
            #endif
            ))
        {
        errAbort("error: unrecognized type \"%s\" for track \"%s\". Valid types include: bigWig, bigBed, bigGenePred, bigPsl, bigChain, bigMaf, bigBarChart, bigInteract, vcfTabix, bam, hic, and longTabix. See https://genome.ucsc.edu/goldenPath/help/trackDb/trackDbHub.html#type for the full list.", trackType, tdb->track);
        }

    if (sameString(splitType[0], "bigBed"))
        {
        if (numWords > 1 && (strchr(splitType[1], '+') || strchr(splitType[1], '.')))
            {
            errAbort("error in type line \"%s\" for track \"%s\". "
                "A space is needed after the \"+\" or \".\" character.", type, tdb->track);
            }
        if (numWords > 2 && (!sameString(splitType[2], "+") && !sameString(splitType[2], ".")))
            {
            errAbort("error in type line \"%s\" for track \"%s\". "
                "Only \"+\" or \".\" is allowed after bigBed numFields setting.", type, tdb->track);
            }
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    trackDbErr(errors, errCatch->message->string, genome, tdb, options->htmlOut);
    retVal = TRUE;
    }
return retVal;
}

boolean isValidDouble(char *s, double *result)
/* Convert string to a double.  Assumes all of string is number
 * and returns FALSE if double is invalid.
 * Does not errAbort. Avoids sqlDouble which does abort. */
{
char* end;
double val = strtod(s, &end);

if ((end == s) || (*end != '\0')
 || *s == ' '
 )
    return FALSE;
*result = val;
return TRUE;
}


static void parseColonRange(struct trackDb *tdb, char *settingName, char *setting)
/* Parse setting's two colon-separated numbers and detects errors in their value. 
 * Helpfully shows track and setting name as well as setting value in the error message. 
 * Does not swap or return min max range values.
 * trackDbDoc syntax
    viewLimits <lower:upper>
 */
{
char tmp[64];
safecpy(tmp, sizeof(tmp), setting);
char *words[3];
if (chopByChar(tmp, ':', words, ArraySize(words)) == 2)
    {
    char *lower = words[0];
    double lowerVal = 0;
    boolean validLower = isValidDouble(lower, &lowerVal);
    if (!validLower)
	errAbort("Invalid double range lower value '%s' in range '%s' in setting %s track %s", lower, setting, settingName, tdb->track);

    char *upper = words[1];
    double upperVal = 0;
    boolean validUpper = isValidDouble(upper, &upperVal);
    if (!validUpper)
	errAbort("Invalid double range upper value '%s' in range '%s' in setting %s track %s", upper, setting, settingName, tdb->track);

    if (validUpper && validLower)
	{
	if (upperVal < lowerVal)
	    warn("in track \"%s\", setting '%s' has the range '%s' where the upper bound is less than the lower bound. Swap the values so the format is lower:upper (e.g. 'viewLimits 0:100').", tdb->track, settingName, setting);
	}
    }
else
    errAbort("Missing a colon in range value '%s' in setting %s track %s", setting, settingName, tdb->track);
}

void checkViewLimitsSettings(struct trackDb *tdb)
/* check viewLimits and viewLimitsMax and defaultViewLimits setting values */
{
int i;
for(i = 0; i < 3; i++)
    {
    char *settingName;
    if (i == 0)
	settingName = VIEWLIMITS;
    if (i == 1)
	settingName = VIEWLIMITSMAX;
    if (i == 2)
	settingName = DEFAULTVIEWLIMITS;

    char *setting = trackDbSetting(tdb, settingName);
    if (setting)
        {
        parseColonRange(tdb, settingName, setting);
        }
    }
}

int hubCheckTrack(struct trackHub *hub, struct trackHubGenome *genome, struct trackDb *tdb,
                        struct trackHubCheckOptions *options, struct dyString *errors)
/* Check track settings and optionally, files */
{
int retVal = 0;
int trackDbErrorCount = 0;

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

struct trackDb *tempTdb = NULL;
char *textName = NULL;
char idName[512];
struct errCatch *errCatch = errCatchNew();
boolean trackIsContainer = (tdbIsComposite(tdb) || tdbIsCompositeView(tdb) || tdbIsContainer(tdb));

// first get down into the subtracks
if (tdb->subtracks != NULL)
    {
    for (tempTdb = tdb->subtracks; tempTdb != NULL; tempTdb = tempTdb->next)
        retVal |= hubCheckTrack(hub, genome, tempTdb, options, errors);
    }

// for when assembly hubs have tracks with the same name, prepend assembly name to id
safef(idName, sizeof(idName), "%s_%s", trackHubSkipHubName(genome->name), trackHubSkipHubName(tdb->track));

if (options->htmlOut)
    {
    dyStringPrintf(errors, "trackData['%s'] = [", idName);
    }

if (errCatchStart(errCatch))
    {
    if (tdb->errMessage)  // if we found any errors when first reading in the trackDb
        errAbort("%s",tdb->errMessage);

    hubCheckParentsAndChildren(tdb);
    if (trackIsContainer)
        retVal |= hubCheckCompositeSettings(genome, tdb, errors, options);

    if (tdbIsSubtrack(tdb))
        retVal |= hubCheckSubtrackSettings(genome, tdb, errors, options);

    // check that type line is syntactically correct regardless of
    // if we actually want to check the data file itself
    boolean foundTypeError = checkTypeLine(genome, tdb, errors, options);
    retVal |= foundTypeError;

    // No point in checking the data file if the type setting is incorrect,
    // since hubCheckBigDataUrl will error out early with a less clear message
    // if the type line is messed up. This has the added benefit of providing
    // consistent messaging on command line interface vs web interface
    if (!foundTypeError && options->checkFiles)
        hubCheckBigDataUrl(hub, genome, tdb);


    checkViewLimitsSettings(tdb);

    if (!sameString(tdb->track, "cytoBandIdeo"))
        {
        trackHubAddDescription(genome->trackDbFile, tdb);
        if (!tdb->html)
            warn("warning: missing description page for track. Add 'html %s.html' line to the '%s' track stanza. ",
                 tdb->track, tdb->track);
        }

    if (!trackIsContainer && sameString(trackDbRequiredSetting(tdb, "type"), "bigWig"))
        {
        char *autoScaleSetting = trackDbLocalSetting(tdb, "autoScale");
        if (autoScaleSetting && !sameString(autoScaleSetting, "off") && !sameString(autoScaleSetting, "on"))
            {
            errAbort("track \"%s\" uses 'autoScale %s', but individual bigWig tracks only accept "
                    "'autoScale on' or 'autoScale off'. To use 'autoScale %s', move that setting to the "
                    "parent composite stanza instead.",
                    trackHubSkipHubName(tdb->track), autoScaleSetting, autoScaleSetting);
            }
        }
    }
errCatchEnd(errCatch);
if (errCatch->gotError || errCatch->gotWarning)
    {
    trackDbErr(errors, errCatch->message->string, genome, tdb, options->htmlOut);
    if (errCatch->gotError || !options->allowWarnings)
        retVal = 1;
    if (errCatch->gotError)
        trackDbErrorCount += 1;
    }
errCatchFree(&errCatch);

if (options->htmlOut)
    {
    if (trackIsContainer)
        {
        for (tempTdb = tdb->subtracks; tempTdb != NULL; tempTdb = tempTdb->next)
            {
            char subtrackName[512];
            safef(subtrackName, sizeof(subtrackName), "%s_%s", trackHubSkipHubName(genome->name), trackHubSkipHubName(tempTdb->track));
            textName = trackHubSkipHubName(tempTdb->longLabel);
            dyStringPrintf(errors, "%s,", makeFolderObjectString(subtrackName, textName, idName, "TRACK", TRUE, retVal));
            }
        }
    else if (!retVal)
        {
        // add "Error" to the trackname to force uniqueness for the jstree
        dyStringPrintf(errors, "{icon: 'fa fa-plus', "
            "id:'%sError', text:'No trackDb configuration errors', parent:'%s'}", idName, idName);
        }
    dyStringPrintf(errors, "];\n");
    }

return retVal;
}


int hubCheckGenome(struct trackHub *hub, struct trackHubGenome *genome,
                struct trackHubCheckOptions *options, struct dyString *errors)
/* Check out genome within hub. */
{
struct errCatch *errCatch = errCatchNew();
struct trackDb *tdbList = NULL;
int genomeErrorCount = 0;
boolean openedGenome = FALSE;
verbose(3, "checking genome %s\n", trackHubSkipHubName(genome->name));

if (errCatchStart(errCatch))
    {
    if (genome->twoBitPath != NULL)
        {
        // check that twoBitPath is a valid file, warn instead of errAbort so we can continue checking
        // the genome stanza
        char *twoBit = genome->twoBitPath;
        if (!extFileExists(twoBit))
            warn("Error: '%s' twoBitPath does not exist or is not accessible: '%s'", genome->name, twoBit);

        // groups and htmlPath are optional settings, again only warn if they are malformed
        char *groupsFile = genome->groups;
        if (groupsFile != NULL && !extFileExists(groupsFile))
            warn("warning: '%s' groups file does not exist or is not accessible: '%s'", genome->name, groupsFile);

        char *htmlPath = hashFindVal(genome->settingsHash, "htmlPath");
        if (htmlPath == NULL)
            warn("warning: missing htmlPath setting for assembly hub '%s'", genome->name);
        else if (!extFileExists(htmlPath))
            warn("warning: '%s' htmlPath file does not exist or is not accessible: '%s'", genome->name, htmlPath);
        }
    boolean foundFirstGenome = FALSE;
    tdbList = trackHubTracksForGenome(hub, genome, NULL, &foundFirstGenome);
    tdbList = trackDbLinkUpGenerations(tdbList);
    tdbList = trackDbPolishAfterLinkup(tdbList, genome->name);
    trackHubPolishTrackNames(hub, tdbList);
    }
errCatchEnd(errCatch);
if (errCatch->gotError || errCatch->gotWarning)
    {
    openedGenome = TRUE;
    genomeErr(errors, errCatch->message->string, hub, genome, options->htmlOut);
    if (errCatch->gotError || !options->allowWarnings)
        genomeErrorCount += 1;
    }
errCatchFree(&errCatch);

verbose(2, "%d tracks in %s\n", slCount(tdbList), genome->name);
struct trackDb *tdb;
int tdbCheckVal;
static struct dyString *tdbDyString = NULL;
if (!tdbDyString)
    tdbDyString = dyStringNew(0);

// build up the track results list, keep track of number of errors, then
// open up genomes folder
char *genomeName = trackHubSkipHubName(genome->name);
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (options->htmlOut)
        {
        // if we haven't already found an error then open up the array
        if (!openedGenome)
            {
            dyStringPrintf(errors, "trackData['%s']  = [", genomeName);
            openedGenome = TRUE;
            }
        }
    // use different dyString for the actual errors generated by each track
    tdbCheckVal = hubCheckTrack(hub, genome, tdb, options, tdbDyString);
    genomeErrorCount += tdbCheckVal;
    if (options->htmlOut)
        {
        // when assembly hubs have tracks with the same name, prepend assembly name to id
        char name[512];
        safef(name, sizeof(name), "%s_%s", trackHubSkipHubName(genome->name), trackHubSkipHubName(tdb->track));
        dyStringPrintf(errors, "%s", makeFolderObjectString(name, tdb->longLabel, genomeName, "TRACK", TRUE, tdbCheckVal ? TRUE : FALSE));
        if (tdb->next != NULL)
            dyStringPrintf(errors, ",");
        }
    }
if (options->htmlOut)
    dyStringPrintf(errors, "];\n");

dyStringPrintf(errors, "%s", tdbDyString->string);
dyStringClear(tdbDyString);

return genomeErrorCount;
}

void checkGenomeRestriction(struct trackHubCheckOptions *options, struct trackHub *hub)
/* check the a genome restriction from the command line is a valid genome */
{
if (options->genome == NULL)
    return;  // OK
for (struct trackHubGenome *genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    if (sameString(trackHubSkipHubName(genome->name), options->genome))
        return;  // OK
    }
errAbort("Genome %s not found in hub", options->genome);
}

boolean shouldCheckGenomes(struct trackHubCheckOptions *options, struct trackHubGenome *genome)
/* should this genome be check based on command line restrictions */
{
return (options->genome == NULL) ||
    sameString(trackHubSkipHubName(genome->name), options->genome);
}


int trackHubCheck(char *hubUrl, struct trackHubCheckOptions *options, struct dyString *errors)
/* Check a track data hub for integrity. Put errors in dyString.
 *      return 0 if hub has no errors, 1 otherwise
 *      if options->checkTracks is TRUE, check remote files of individual tracks
 */
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *hub = NULL;
struct dyString *hubErrors = dyStringNew(0);
int retVal = 0;

if (errCatchStart(errCatch))
    {
    hub = trackHubOpen(hubUrl, "");
    char *descUrl = hub->descriptionUrl;
    if (descUrl == NULL)
        warn("warning: missing hub overview description page. Add a 'descriptionUrl hubDescription.html' line to hub.txt.");
    else if (!extFileExists(descUrl))
        warn("warning: descriptionUrl '%s' could not be accessed. Verify the file exists at the specified path and is publicly readable.", hub->descriptionUrl);
    }
errCatchEnd(errCatch);
if (errCatch->gotError || errCatch->gotWarning)
    {
    hubErr(hubErrors, errCatch->message->string, hub, options->htmlOut);
    if (errCatch->gotError || !options->allowWarnings)
        retVal = 1;

    if (options->htmlOut)
        {
        if (hub && hub->shortLabel)
            {
            dyStringPrintf(errors, "trackData['#'] = [%s,",
                makeFolderObjectString(hub->shortLabel, "Hub Errors", "#",
                    "Click to open node", TRUE, TRUE));
            }
        else
            {
            dyStringPrintf(errors, "trackData['#'] = [%s,",
                makeFolderObjectString("Hub Error", "Hub Errors", "#",
                    "Click to open node", TRUE, TRUE));
            }
        }
    }
errCatchFree(&errCatch);

if (hub == NULL)
    {
    // the reason we couldn't close the array in the previous block is because
    // there may be non-fatal errors and we still want to keep trying to check
    // the genomes settings, which need to be children of the root '#' node.
    // Here we are at a fatal error so we can close the array and return
    if (options->htmlOut)
        dyStringPrintf(errors, "];\n%s", dyStringCannibalize(&hubErrors));
    else
        dyStringPrintf(errors, "%s", dyStringCannibalize(&hubErrors));
    return 1;
    }
if (options->htmlOut && retVal != 1)
    dyStringPrintf(errors, "trackData['#'] = [");

if (options->checkSettings)
    retVal |= hubSettingsCheckInit(hub, options, errors);

struct trackHubGenome *genome;
checkGenomeRestriction(options, hub);
char genomeTitleString[128];
struct dyString *genomeErrors = dyStringNew(0);
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    if (shouldCheckGenomes(options, genome))
        {
        int numGenomeErrors = hubCheckGenome(hub, genome, options, genomeErrors);
        if (options->htmlOut)
            {
            char *genomeName = trackHubSkipHubName(genome->name);
            safef(genomeTitleString, sizeof(genomeTitleString),
                  "%s (%d configuration error%s)", genomeName, numGenomeErrors,
                  numGenomeErrors == 1 ? "" : "s");
            dyStringPrintf(errors, "%s,", makeFolderObjectString(genomeName, genomeTitleString, "#",
                           "Click to open node", TRUE, numGenomeErrors > 0 ? TRUE : FALSE));
            }
        retVal |= numGenomeErrors;
        }
    }
if (options->htmlOut)
    {
    dyStringPrintf(errors, "];\n");
    }
dyStringPrintf(errors, "%s", dyStringCannibalize(&hubErrors));
dyStringPrintf(errors, "%s", dyStringCannibalize(&genomeErrors));
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

checkOptions->specHost = (optionExists("test") ? "genome-test.soe.ucsc.edu" : "genome.ucsc.edu");
checkOptions->specHost = optionVal("specHost", checkOptions->specHost);

checkOptions->printMeta = optionExists("printMeta");
checkOptions->checkFiles = !optionExists("noTracks");
checkOptions->checkSettings = optionExists("checkSettings");
checkOptions->genome = optionVal("genome", NULL);
checkOptions->allowWarnings = optionExists("allowWarnings");

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

char *httpsCertCheck = optionVal("httpsCertCheck", NULL);
if (httpsCertCheck)
    {
    //  level log for testing, but you only see something if SCRIPT_NAME env variable is set like CGIs have.
    if (sameString(httpsCertCheck, "abort") || sameString(httpsCertCheck, "warn") || sameString(httpsCertCheck, "log") || sameString(httpsCertCheck, "none"))
	{
	setenv("https_cert_check", httpsCertCheck, 1);
	}
    else
	{
	// log level is not very useful, but included it for completeness.
	verbose(1, "The value of -httpsCertCheck should be either abort to avoid Man-in-middle attack,\n"
		"warn to warn about failed certs,\n"
		"none indicating the verify is skipped entirely.");
	usage();
	}
    }

// should be space separated list, if that lists contains "noHardwiredExceptions" then the built-in hardwired whitelist in https.c is skipped.
char *httpsCertCheckDomainExceptions = optionVal("httpsCertCheckDomainExceptions", NULL);
if (httpsCertCheckDomainExceptions)
    {
    setenv("https_cert_check_domain_exceptions", httpsCertCheckDomainExceptions, 1);
    }

knetUdcInstall();  // make the htslib library use udc

if (optionExists("settings"))
    {
    showSettings(checkOptions);
    return 0;
    }

// hgHubConnect specific option for generating a jstree of the hub errors
checkOptions->htmlOut = optionExists("htmlOut");
struct dyString *errors = dyStringNew(1024);
int checkResult = trackHubCheck(argv[1], checkOptions, errors);
if (checkOptions->htmlOut)
    {
    printf("%s", errors->string);
    return checkResult ? 1 : 0;
    }
else if (errors->stringSize > 0)
    {
    // uniquify and count errors
    struct slName *errs = slNameListFromString(errors->string, '\n');
    slUniqify(&errs, slNameCmp, slNameFree);
    int errCount = slCount(errs);
    printf("Found %d problem%s:\n", errCount, errCount == 1 ? "" : "s");
    printf("%s\n", slNameListToString(errs, '\n'));
    return checkResult ? 1 : 0;
    }
return 0;
}
