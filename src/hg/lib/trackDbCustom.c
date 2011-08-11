/* trackDbCustom - custom (not autoSQL generated) code for working
 * with trackDb.  This code is concerned with making the trackDb
 * MySQL table out of the trackDb.ra files. */

#include "common.h"
#include "linefile.h"
#include "jksql.h"
#include "trackDb.h"
#include "hdb.h"
#include "hui.h"
#include "ra.h"
#include "hash.h"
#include "net.h"
#include "sqlNum.h"
#include "obscure.h"
#include "hgMaf.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: trackDbCustom.c,v 1.90 2010/05/18 22:37:36 kent Exp $";

/* ----------- End of AutoSQL generated code --------------------- */

struct trackDb *trackDbNew()
/* Allocate a new trackDb with just very minimal stuff filled in. */
{
struct trackDb *tdb;
AllocVar(tdb);
tdb->canPack = 2;	/* Unknown value. */
return tdb;
}

int trackDbCmp(const void *va, const void *vb)
/* Compare to sort based on priority; use shortLabel as secondary sort key.
 * Note: parallel code to hgTracks.c:tgCmpPriority */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
   return -1;
else if (dif == 0.0)
   return strcasecmp(a->shortLabel, b->shortLabel);
else
   return 1;
}

void parseColor(char *text, unsigned char *r, unsigned char *g, unsigned char *b)
/* Turn comma-separated string of three numbers into three
 * color components. */
{
char dupe[64];
safecpy(dupe, sizeof(dupe), text);
char *words[4];
int wordCount;
wordCount = chopString(dupe, ", \t", words, ArraySize(words));
if (wordCount != 3)
    errAbort("Expecting 3 comma separated values in %s.", text);
*r = atoi(words[0]);
*g = atoi(words[1]);
*b = atoi(words[2]);
}

static unsigned char parseVisibility(char *value)
/* Parse a visibility value */
{
if (sameString(value, "hide") || sameString(value, "0"))
    return tvHide;
else if (sameString(value, "dense") || sameString(value, "1"))
    return tvDense;
else if (sameString(value, "full") || sameString(value, "2") || sameString(value, "show"))
    return tvFull;
else if (sameString(value, "pack") || sameString(value, "3"))
    return tvPack;
else if (sameString(value, "squish") || sameString(value, "4"))
    return tvSquish;
else
    errAbort("Unknown visibility %s ", value);
return tvHide;  /* never reached */
}

static void parseTrackLine(struct trackDb *bt, char *value,
                           struct lineFile *lf)
/* parse the track line */
{
char *val2 = cloneString(value);
bt->track = nextWord(&val2);

// check for override option
if (val2 != NULL)
    {
    val2 = trimSpaces(val2);
    if (!sameString(val2, "override"))
        errAbort("invalid track line: %s:%d: track %s", lf->fileName, lf->lineIx, value);
    bt->overrides = hashNew(0);
    }
}

static void trackDbAddRelease(struct trackDb *bt, char *releaseTag)
/* Add release tag */
{
hashAdd(bt->settingsHash, "release", cloneString(releaseTag));
}

static void trackDbAddInfo(struct trackDb *bt,
	char *var, char *value, struct lineFile *lf)
/* Add info from a variable/value pair to browser table. */
{
if (sameString(var, "track"))
    parseTrackLine(bt, value, lf);
if (bt->settingsHash == NULL)
    bt->settingsHash = hashNew(7);
hashAdd(bt->settingsHash, var, cloneString(value));

if (bt->overrides != NULL)
    hashAdd(bt->overrides, var, NULL);
}

//not needed?
int bedDetailSizeFromType(char *type)
/* parse bedSize from type line for bedDetail, assume 4 if none */
{
int ret = 4;  /* minimal expected */
char *words[3];
int wordCount = chopLine(cloneString(type), words);
if (wordCount > 1)
    ret = atoi(words[1]) - 2; /* trackDb has field count, we want bedSize */
return ret;
}

void trackDbFieldsFromSettings(struct trackDb *bt)
/* Update trackDb fields from settings hash */
{
char *shortLabel = trackDbSetting(bt, "shortLabel");
if (shortLabel != NULL)
     bt->shortLabel = cloneString(shortLabel);
char *longLabel = trackDbSetting(bt, "longLabel");
if (longLabel != NULL)
     bt->longLabel = cloneString(longLabel);
char *priority = trackDbSetting(bt, "priority");
if (priority != NULL)
     bt->priority = atof(priority);
char *url = trackDbSetting(bt, "url");
if (url != NULL)
     bt->url = cloneString(url);
char *visibility = trackDbSetting(bt, "visibility");
if (visibility != NULL)
     bt->visibility = parseVisibility(visibility);
char *color = trackDbSetting(bt, "color");
if (color != NULL)
    parseColor(color, &bt->colorR, &bt->colorG, &bt->colorB);
char *altColor = trackDbSetting(bt, "altColor");
if (altColor != NULL)
    parseColor(altColor, &bt->altColorR, &bt->altColorG, &bt->altColorB);
char *type = trackDbSetting(bt, "type");
if (type != NULL)
     bt->type = cloneString(type);
if (trackDbSetting(bt, "spectrum") != NULL || trackDbSetting(bt, "useScore") != NULL)
    bt->useScore = TRUE;
char *canPack = trackDbSetting(bt, "canPack");
if (canPack != NULL)
    bt->canPack = !(sameString(canPack, "0") || sameString(canPack, "off"));
char *chromosomes = trackDbSetting(bt, "chromosomes");
if (chromosomes != NULL)
    sqlStringDynamicArray(chromosomes, &bt->restrictList, &bt->restrictCount);
if (trackDbSetting(bt, "private") != NULL)
    bt->private = TRUE;
char *grp = trackDbSetting(bt, "group");
if (grp != NULL)
     bt->grp = cloneString(grp);
}

static void replaceStr(char **varPtr, char *val)
/** replace string in varPtr with val */
{
freeMem(*varPtr);
*varPtr = cloneString(val);
}

static void overrideField(struct trackDb *td, struct trackDb *overTd,
                          char *var)
/* Update override one field from override. */
{
if (sameString(var, "track") || sameString(var, "release"))
    return;
char *val = hashMustFindVal(overTd->settingsHash, var);
struct hashEl *hel = hashStore(td->settingsHash, var);
replaceStr((char**)&hel->val, val);
}

void trackDbOverride(struct trackDb *td, struct trackDb *overTd)
/* apply an trackOverride trackDb entry to a trackDb entry */
{
assert(overTd->overrides != NULL);
struct hashEl *hel;
struct hashCookie hc = hashFirst(overTd->overrides);
while ((hel = hashNext(&hc)) != NULL)
    {
    overrideField(td, overTd, hel->name);
    }
}

static boolean packableType(char *type)
/* Return TRUE if we can pack this type. */
{
char *t = cloneString(type);
char *s = firstWordInLine(t);
boolean canPack = (sameString("psl", s) || sameString("chain", s) ||
                   sameString("bed", s) || sameString("genePred", s) ||
		   sameString("bigBed", s) || sameString("makeItems", s) ||
                   sameString("expRatio", s) || sameString("wigMaf", s) ||
		   sameString("factorSource", s) || sameString("bed5FloatScore", s) ||
		   sameString("bed6FloatScore", s) || sameString("altGraphX", s) ||
		   sameString("bam", s) || sameString("bedDetail", s) ||
		   sameString("bed8Attrs", s) || sameString("gvf", s) ||
		   sameString("vcfTabix", s));
freeMem(t);
return canPack;
}



void trackDbPolish(struct trackDb *bt)
/* Fill in missing values with defaults. */
{
if (bt->shortLabel == NULL)
    bt->shortLabel = cloneString(bt->track);
if (bt->longLabel == NULL)
    bt->longLabel = cloneString(bt->shortLabel);
if (bt->altColorR == 0 && bt->altColorG == 0 && bt->altColorB == 0)
    {
    bt->altColorR = (255+bt->colorR)/2;
    bt->altColorG = (255+bt->colorG)/2;
    bt->altColorB = (255+bt->colorB)/2;
    }
if (bt->type == NULL)
    bt->type = cloneString("");
if (bt->priority == 0)
    bt->priority = 100.0;
if (bt->url == NULL)
    bt->url = cloneString("");
if (bt->html == NULL)
    bt->html = cloneString("");
if (bt->grp == NULL)
    bt->grp = cloneString("x");
if (bt->canPack == 2)
    bt->canPack = packableType(bt->type);
if (bt->settings == NULL)
    bt->settings = cloneString("");
}

char *trackDbInclude(char *raFile, char *line, char **releaseTag)
/* Get include filename from trackDb line.
   Return NULL if line doesn't contain include */
{
static char incFile[256];
char *file;

if (startsWith("include", line))
    {
    splitPath(raFile, incFile, NULL, NULL);
    nextWord(&line);
    file = nextQuotedWord(&line);
    strcat(incFile, file);
    *releaseTag = nextWord(&line);
    return cloneString(incFile);
    }
else
    return NULL;
}

struct trackDb *trackDbFromOpenRa(struct lineFile *lf, char *releaseTag)
/* Load track info from ra file already opened as lineFile into list.  If releaseTag is
 * non-NULL then only load tracks that mesh with release. */
{
char *raFile = lf->fileName;
char *line, *word;
struct trackDb *btList = NULL, *bt;
boolean done = FALSE;
char *incFile;

for (;;)
    {
    /* Seek to next line that starts with 'track' */
    for (;;)
	{
        char *subRelease;

	if (!lineFileNextFull(lf, &line, NULL, NULL, NULL))
            { // NOTE: lineFileNextFull joins continuation lines
	   done = TRUE;
	   break;
	   }
	line = skipLeadingSpaces(line);
        if (startsWithWord("track", line))
            {
            lineFileReuseFull(lf); // NOTE: only works with previous lineFileNextFull call
            break;
            }
        else if ((incFile = trackDbInclude(raFile, line, &subRelease)) != NULL)
            {
            if (subRelease)
                trackDbCheckValidRelease(subRelease);
            if (releaseTag && subRelease && !sameString(subRelease, releaseTag))
                errAbort("Include with release %s inside include with release %s line %d of %s", subRelease, releaseTag, lf->lineIx, lf->fileName);
            struct trackDb *incTdb = trackDbFromRa(incFile, subRelease);
            btList = slCat(btList, incTdb);
            }
	}
    if (done)
        break;

    /* Allocate track structure and fill it in until next blank line. */
    bt = trackDbNew();
    slAddHead(&btList, bt);
    for (;;)
        {
        /* Break at blank line or EOF. */
        if (!lineFileNextFull(lf, &line, NULL, NULL, NULL))  // NOTE: joins continuation lines
            break;
        line = skipLeadingSpaces(line);
        if (line == NULL || line[0] == 0)
            break;

        /* Skip comments. */
        if (line[0] == '#')
            continue;

        /* Parse out first word and decide what to do. */
        word = nextWord(&line);
        if (line == NULL)
            errAbort("No value for %s line %d of %s", word, lf->lineIx, lf->fileName);
        line = trimSpaces(line);
        trackDbUpdateOldTag(&word, &line);
        if (releaseTag && sameString(word, "release"))
            errAbort("Release tag %s in stanza with include override %s, line %d of %s",
                line, releaseTag, lf->lineIx, lf->fileName);
        trackDbAddInfo(bt, word, line, lf);
        }
    if (releaseTag)
        trackDbAddRelease(bt, releaseTag);
    }
slReverse(&btList);
return btList;
}

boolean trackDbCheckValidRelease(char *tag)
/* check to make sure release tag is valid */
{
char *words[5];

int count = chopString(cloneString(tag), ",", words, ArraySize(words));
if (count > 3)
    return FALSE;

int ii;
for(ii=0; ii < count; ii++)
    if (!sameString(words[ii], "alpha") && !sameString(words[ii], "beta") &&
        !sameString(words[ii], "public"))
            return FALSE;

return TRUE;
}

struct trackDb *trackDbFromRa(char *raFile, char *releaseTag)
/* Load track info from ra file into list.  If releaseTag is non-NULL
 * then only load tracks that mesh with release. */
{
struct lineFile *lf = netLineFileOpen(raFile);
struct trackDb *tdbList = trackDbFromOpenRa(lf, releaseTag);
lineFileClose(&lf);
return tdbList;
}

struct hash *trackDbHashSettings(struct trackDb *tdb)
/* Force trackDb to hash up it's settings.  Usually this is just
 * done on demand. Returns settings hash. */
{
if (tdb->settingsHash == NULL)
    tdb->settingsHash = trackDbSettingsFromString(tdb->settings);
return tdb->settingsHash;
}

struct hash *trackDbSettingsFromString(char *string)
/* Return hash of key/value pairs from string.  Differs
 * from raFromString in that it passes the key/val
 * pair through the backwards compatability routines. */
{
char *dupe = cloneString(string);
char *s = dupe, *lineEnd;
struct hash *hash = newHash(7);
char *key, *val;

for (;;)
    {
    s = skipLeadingSpaces(s);
    if (s == NULL || s[0] == 0)
        break;
    lineEnd = strchr(s, '\n');
    if (lineEnd != NULL)
        *lineEnd++ = 0;
    key = nextWord(&s);
    val = skipLeadingSpaces(s);
    trackDbUpdateOldTag(&key, &val);
    s = lineEnd;
    val = lmCloneString(hash->lm, val);
    hashAdd(hash, key, val);
    }
freeMem(dupe);
return hash;
}

char *trackDbLocalSetting(struct trackDb *tdb, char *name)
/* Return setting from tdb, but *not* any of it's parents. */
{
if (tdb == NULL)
    errAbort("Program error: null tdb passed to trackDbSetting.");
if (tdb->settingsHash == NULL)
    tdb->settingsHash = trackDbSettingsFromString(tdb->settings);
return hashFindVal(tdb->settingsHash, name);
}

struct slName *trackDbLocalSettingsWildMatch(struct trackDb *tdb, char *expression)
// Return local settings that match expression else NULL.  In alpha order.
{
if (tdb == NULL)
    errAbort("Program error: null tdb passed to trackDbSetting.");
if (tdb->settingsHash == NULL)
    tdb->settingsHash = trackDbSettingsFromString(tdb->settings);

struct slName *slFoundVars = NULL;
struct hashCookie brownie = hashFirst(tdb->settingsHash);
struct hashEl* el = NULL;
while ((el = hashNext(&brownie)) != NULL)
    {
    if (wildMatch(expression, el->name))
        slNameAddHead(&slFoundVars, el->name);
    }

if (slFoundVars != NULL)
    slNameSort(&slFoundVars);

return slFoundVars;
}

struct slName *trackDbSettingsWildMatch(struct trackDb *tdb, char *expression)
// Return settings in tdb tree that match expression else NULL.  In alpha order, no duplicates.
{
struct trackDb *generation;
struct slName *slFoundVars = NULL;
for (generation = tdb; generation != NULL; generation = generation->parent)
    {
    struct slName *slFoundHere = trackDbLocalSettingsWildMatch(generation,expression);
    if (slFoundHere != NULL)
        {
        if (slFoundVars == NULL)
            slFoundVars = slFoundHere;
        else
            {
            struct slName *one = NULL;
            while ((one = slPopHead(&slFoundHere)) != NULL)
                {
                slNameStore(&slFoundVars, one->name); // Will only store if it is not already found!  This means closest to home will work
                slNameFree(&one);
                }
            }
        }
    }
if (slFoundVars != NULL)
    slNameSort(&slFoundVars);

return slFoundVars;
}

boolean trackDbSettingOn(struct trackDb *tdb, char *name)
/* Return true if a tdb setting is "on" "true" or "enabled". */
{
char *setting = trackDbSetting(tdb,name);
return  (setting && (   sameWord(setting,"on")
                     || sameWord(setting,"true")
                     || sameWord(setting,"enabled")));
}

char *trackDbRequiredSetting(struct trackDb *tdb, char *name)
/* Return setting string or squawk and die. */
{
char *ret = trackDbSetting(tdb, name);
if (ret == NULL)
   errAbort("Missing required '%s' setting in %s track", name, tdb->track);
return ret;
}

char *trackDbSettingOrDefault(struct trackDb *tdb, char *name, char *defaultVal)
/* Return setting string, or defaultVal if none exists */
{
    char *val = trackDbSetting(tdb, name);
    return (val == NULL ? defaultVal : val);
}

float trackDbFloatSettingOrDefault(struct trackDb *tdb, char *name, float defaultVal)
/* Return setting, convert to a float, or defaultVal if none exists */
{
    char *val = trackDbSetting(tdb, name);
    if (val == NULL)
        return defaultVal;
    else
        return sqlFloat(trimSpaces(val));
}

struct hashEl *trackDbSettingsLike(struct trackDb *tdb, char *wildStr)
/* Return a list of settings whose names match wildStr (may contain wildcard
 * characters).  Free the result with hashElFreeList. */
{
struct hashEl *allSettings = hashElListHash(tdb->settingsHash);
struct hashEl *matchingSettings = NULL;
struct hashEl *hel = allSettings;

while (hel != NULL)
    {
    struct hashEl *next = hel->next;
    if (wildMatch(wildStr, hel->name))
	{
	slAddHead(&matchingSettings, hel);
	}
    else
	hashElFree(&hel);
    hel = next;
    }
return matchingSettings;
}

struct superTrackInfo {
    boolean isSuper;
    boolean isShow;
    char *parentName;
    enum trackVisibility defaultVis;
};

static struct superTrackInfo *getSuperTrackInfo(struct trackDb *tdb)
/* Get info from supertrack setting.  There are 2 forms:
 * Parent:   'superTrack on [show]'
 * Child:    'superTrack <parent> [vis]
 * Return null if there is no such setting. */
{
char *setting = trackDbSetting(tdb, "superTrack");
if (!setting)
    return NULL;
char *words[8];
int wordCt = chopLine(cloneString(setting), words);
if (wordCt < 1)
    return NULL;
struct superTrackInfo *stInfo;
AllocVar(stInfo);
if (sameString("on", words[0]))
    {
    /* parent */
    stInfo->isSuper = TRUE;
    if ((wordCt > 1) && sameString("show", words[1]))
        stInfo->isShow = TRUE;
    }
else
    {
    /* child */
    stInfo->parentName = cloneString(words[0]);
    if (wordCt > 1)
        stInfo->defaultVis = max(0, hTvFromStringNoAbort(words[1]));
    }
return stInfo;
}

char *trackDbGetSupertrackName(struct trackDb *tdb)
/* Find name of supertrack if this track is a member */
{
char *ret = NULL;
struct superTrackInfo *stInfo = getSuperTrackInfo(tdb);
if (stInfo)
    {
    if (stInfo->parentName)
        ret = cloneString(stInfo->parentName);
    freeMem(stInfo);
    }
return ret;
}

void trackDbSuperMemberSettings(struct trackDb *tdb)
/* Set fields in trackDb to indicate this is a member of a
 * supertrack. */
{
struct superTrackInfo *stInfo = getSuperTrackInfo(tdb);
if(stInfo == NULL || stInfo->isSuper)
    return;
tdb->parentName = cloneString(stInfo->parentName);
tdb->visibility = stInfo->defaultVis;
tdbMarkAsSuperTrackChild(tdb);
if(tdb->parent)
    {
    tdbMarkAsSuperTrack(tdb->parent);
    }
freeMem(stInfo);
}

char *maybeSkipHubPrefix(char *track)
{
if (!startsWith("hub_", track))
    return track;

char *nextUnderBar = strchr(track + sizeof "hub_", '_');

if (nextUnderBar)
    return nextUnderBar + 1;

return track;
}

void trackDbSuperMarkup(struct trackDb *tdbList)
/* Set trackDb from superTrack setting */
{
struct trackDb *tdb;
struct hash *superHash = hashNew(0);
struct superTrackInfo *stInfo;

/* find supertracks, setup their settings */
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    stInfo = getSuperTrackInfo(tdb);
    if (!stInfo)
        continue;
    if (stInfo->isSuper)
        {
        tdbMarkAsSuperTrack(tdb);
        tdb->isShow = stInfo->isShow;
        if (!hashLookup(superHash, tdb->track))
            hashAdd(superHash, maybeSkipHubPrefix(tdb->track), tdb);
        }
    freeMem(stInfo);
    }
/* adjust settings on supertrack members after verifying they have
 * a supertrack configured in this trackDb */
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    stInfo = getSuperTrackInfo(tdb);
    if (!stInfo)
        continue;
    if(!stInfo->isSuper)
        {
        tdb->parent = hashFindVal(superHash, stInfo->parentName);
        if (tdb->parent)
	    {
            trackDbSuperMemberSettings(tdb);
	    }
        }
    freeMem(stInfo);
    }
}

char *trackDbOrigAssembly(struct trackDb *tdb)
/* return setting from trackDb, if any */
{
return (trackDbSetting(tdb, "origAssembly"));
}

void trackDbPrintOrigAssembly(struct trackDb *tdb, char *database)
/* Print lift information from trackDb, if any */
{
char *origAssembly = trackDbOrigAssembly(tdb);
if (origAssembly)
    {
    if (differentString(origAssembly, database))
        {
	printf("<B>Data coordinates converted via <A TARGET=_BLANK "
	       "HREF=\"../goldenPath/help/hgTracksHelp.html#Liftover\">liftOver</A> from:</B> ");
        char *freeze = hFreezeFromDb(origAssembly);
	if (freeze == NULL)
	    printf("%s<BR>\n", origAssembly);
	else if (stringIn(origAssembly, freeze))
	    printf("%s<BR>\n", freeze);
	else
	    printf("%s (%s)<BR>\n", freeze, origAssembly);
        }
    }
}

eCfgType cfgTypeFromTdb(struct trackDb *tdb, boolean warnIfNecessary)
/* determine what kind of track specific configuration is needed,
   warn if not multi-view compatible */
{
eCfgType cType = cfgNone;
char *type = tdb->type;
if(startsWith("wigMaf", type))
    cType = cfgWigMaf;
else if(startsWith("wig", type))
    cType = cfgWig;
else if(startsWith("bigWig", type))
    cType = cfgWig;
else if(startsWith("bedGraph", type))
    cType = cfgWig;
else if(startsWith("netAlign", type))
    {
    cType = cfgNetAlign;
    warnIfNecessary = FALSE;
    }
else if(sameWord("bed5FloatScore",       type)
     || sameWord("bed5FloatScoreWithFdr",type))
    cType = cfgBedScore;
else if(sameWord("narrowPeak",type)
     || sameWord("broadPeak", type)
     || sameWord("encodePeak",type)
     || sameWord("gappedPeak",type))
    cType = cfgPeak;
else if(sameWord("genePred",type))
        cType = cfgGenePred;
else if(sameWord("bedLogR",type) || sameWord("peptideMapping", type))
    cType = cfgBedScore;
else if(startsWith("bed ", type))
    {
    char *words[3];
    chopLine(cloneString( type), words);
    if (trackDbSetting(tdb, "bedFilter") != NULL)
	   cType = cfgBedFilt;
    else if (atoi(words[1]) >= 5 && trackDbSettingClosestToHome(tdb, "noScoreFilter") == NULL)
        cType = cfgBedScore;
    }
else if(startsWith("chain",type))
    cType = cfgChain;
else if (startsWith("bam", type))
    cType = cfgBam;
else if (startsWith("psl", type))
    cType = cfgPsl;
// TODO: Only these are configurable so far

if(cType == cfgNone && warnIfNecessary)
    {
    if(!startsWith("bed ", type) && !startsWith("bigBed", type)
    && subgroupFind(tdb,"view",NULL))
        warn("Track type \"%s\" is not yet supported in multi-view composites for %s.",type,tdb->track);
    }
return cType;
}

char *trackDbSetting(struct trackDb *tdb, char *name)
/* Look for a trackDb setting from lowest level on up chain of parents. */
{
struct trackDb *generation;
char *trackSetting = NULL;
for (generation = tdb; generation != NULL; generation = generation->parent)
    {
    trackSetting = trackDbLocalSetting(generation,name);
    if (trackSetting != NULL)
        break;
    }
return trackSetting;
}

char *trackDbSettingByView(struct trackDb *tdb, char *name)
/* For a subtrack of a multiview composite, get a setting stored in the view or any other
 * ancestor. */
{
if (tdb->parent == NULL)
    return NULL;
return trackDbSetting(tdb->parent, name);
}


char *trackDbSettingClosestToHomeOrDefault(struct trackDb *tdb, char *name, char *defaultVal)
/* Look for a trackDb setting (or default) from lowest level on up chain of parents. */
{
char *trackSetting = trackDbSetting(tdb,name);
if(trackSetting == NULL)
    trackSetting = defaultVal;
return trackSetting;
}

boolean trackDbSettingClosestToHomeOn(struct trackDb *tdb, char *name)
/* Return true if a tdb setting closest to home is "on" "true" or "enabled". */
{
char *setting = trackDbSetting(tdb,name);
return  (setting && (   sameWord(setting,"on")
                     || sameWord(setting,"true")
                     || sameWord(setting,"enabled")
                     || atoi(setting) != 0));
}

struct trackDb *subTdbFind(struct trackDb *parent,char *childName)
/* Return child tdb if it exists in parent. */
{
if(parent == NULL)
    return NULL;

struct trackDb *found = NULL;
struct slRef *tdbRef, *tdbRefList = trackDbListGetRefsToDescendants(parent->subtracks);
for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
    {
    struct trackDb *tdb = tdbRef->val;
    if (sameString(tdb->track, childName))
	{
	found = tdb;
        break;
	}
    }
slFreeList(&tdbRefList);
return found;
}

struct trackDb *tdbFindOrCreate(char *db,struct trackDb *parent,char *track)
/* Find or creates the tdb for this track. May return NULL. */
{
struct trackDb *tdb = NULL;
if (parent != NULL)
    {
    if(sameString(parent->track, track))
        tdb = parent;
    else if(consWiggleFind(db,parent,track) != NULL)
        tdb = parent;
    else
        tdb = subTdbFind(parent,track);
    }
if(tdb == NULL && db != NULL)
    {
    struct sqlConnection *conn = hAllocConn(db);
    tdb = hMaybeTrackInfo(conn, track);
    hFreeConn(&conn);
    }
return tdb;
}

#ifdef OMIT
// NOTE: This may not be needed.
struct trackDb *tdbFillInAncestry(char *db,struct trackDb *tdbChild)
/* Finds parents and fills them in.  Does not find siblings, however! */
{
assert(tdbChild != NULL);
struct trackDb *tdb = tdbChild;
struct sqlConnection *conn = NULL;
for (;tdb->parent != NULL;tdb = tdb->parent)
    ; // advance to highest parent already known

// If track with no tdbParent has a parent setting then fill it in.
for (;tdb->parent == NULL; tdb = tdb->parent)
    {
    char *parentTrack = trackDbLocalSetting(tdb,"parent");
    if (parentTrack == NULL)
        break;

    if (conn == NULL)
        conn = hAllocConn(db);
    tdb->parent = hMaybeTrackInfo(conn, parentTrack); // Now there are 2 versions of this child!  And what to do about views?
    printf("tdbFillInAncestry(%s): has %d children.",parentTrack,slCount(tdb->parent->subtracks));
    //tdb->parent = tdbFindOrCreate(db,tdb,parentTrack); // Now there are 2 versions of this child!  And what to do about views?
    }
if (conn != NULL)
    hFreeConn(&conn);

return tdb;
}
#endif///def OMIT

boolean tdbIsView(struct trackDb *tdb,char **viewName)
// Is this tdb a view?  Will fill viewName if provided
{
if (tdbIsCompositeView(tdb))
    {
    if (viewName)
        {
        *viewName = trackDbLocalSetting(tdb, "view");
        assert(*viewName != NULL);
        }
    return TRUE;
    }
return FALSE;
}

char *tdbGetViewName(struct trackDb *tdb)
// returns NULL the view name for view or child track (do not free)
{
char *view = NULL;
if(tdbIsComposite(tdb))
    return NULL;
else if(tdbIsCompositeChild(tdb) && subgroupFind(tdb,"view",&view))
    return view;
else if(tdbIsView(tdb,&view))
    return view;
return NULL;
}

struct trackDb *trackDbLinkUpGenerations(struct trackDb *tdbList)
/* Convert a list to a forest - filling in parent and subtrack pointers.
 * The exact topology of the forest is a little complex due to the
 * fact there are two "inheritance" systems - the superTrack system
 * and the subTrack system.  In the superTrack system (which is on it's
 * way out)  the superTrack's themselves have the tag:
 *     superTrack on
 * and the children of superTracks have the tag:
 *     superTrack parentName
 * In the subTrack system the parents have the tag:
 *     compositeTrack on
 * and the children have the tag:
 *     parent parentName
 * In this routine the subtracks are removed from the list, and stuffed into
 * the subtracks lists of their parents.  The highest level parents stay on
 * the list.  There can be multiple levels of inheritance.
 *    For the supertracks the _parents_ are removed from the list.  The only
 * reference to them in the returned forest is that they are in the parent
 * field of their children.  The parents of supertracks have no subtracks
 * after this call currently. */
{
struct trackDb *forest = NULL;
struct hash *trackHash = hashNew(0);
struct trackDb *tdb, *next;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    hashAdd(trackHash, tdb->track, tdb);

/* Do superTrack inheritance.  This involves setting up the parent pointers to superTracks,
 * but removing the superTracks themselves from the list. */
struct trackDb *superlessList = NULL;
trackDbSuperMarkup(tdbList);
for (tdb = tdbList; tdb != NULL; tdb = next)
    {
    next = tdb->next;
    if (tdbIsSuperTrack(tdb))
        tdb->next = NULL;
    else
        slAddHead(&superlessList, tdb);
    }

/* Do subtrack hierarchy - filling in parent and subtracks fields. */
for (tdb = superlessList; tdb != NULL; tdb = next)
    {
    next = tdb->next;
    char *subtrackSetting = trackDbLocalSetting(tdb, "parent");
    if (subtrackSetting != NULL)
        {
	char *parentName = cloneFirstWord(subtrackSetting);
	struct trackDb *parent = hashFindVal(trackHash, parentName);
	if (parent != NULL)
	    {
	    slAddHead(&parent->subtracks, tdb);
	    tdb->parent = parent;
	    }
	else
	    {
	    errAbort("Parent track %s of child %s doesn't exist", parentName, tdb->track);
	    }
	freez(&parentName);
	}
    else
        {
	slAddHead(&forest, tdb);
	}
    }

hashFree(&trackHash);
return forest;
}

void trackDbPrioritizeContainerItems(struct trackDb *tdbList)
/* Set priorities in containers if they have no priorities already set
   priorities are based upon 'sortOrder' setting or else shortLabel */
{
int countOfSortedContainers = 0;

// Walk through tdbs looking for containers
struct trackDb *tdbContainer;
for (tdbContainer = tdbList; tdbContainer != NULL; tdbContainer = tdbContainer->next)
    {
    if (tdbContainer->subtracks == NULL)
        continue;

    sortOrder_t *sortOrder = sortOrderGet(NULL,tdbContainer);
    boolean needsSorting = TRUE; // default
    float firstPriority = -1.0;
    sortableTdbItem *item,*itemsToSort = NULL;

    struct slRef *child, *childList = trackDbListGetRefsToDescendantLeaves(tdbContainer->subtracks);
    // Walk through tdbs looking for items contained
    for (child = childList; child != NULL; child = child->next)
        {
	struct trackDb *tdbItem = child->val;
	if( needsSorting && sortOrder == NULL )  // do we?
	    {
	    if( firstPriority == -1.0)    // all 0 or all the same value
		firstPriority = tdbItem->priority;
	    if(firstPriority != tdbItem->priority && (int)(tdbItem->priority + 0.9) > 0)
		{
		needsSorting = FALSE;
		break;
		}
	    }
	// create an Item
	item = sortableTdbItemCreate(tdbItem,sortOrder);
	if(item != NULL)
	    slAddHead(&itemsToSort, item);
	else
	    {
	    verbose(1,"Error: '%s' missing shortLabels or sortOrder setting is inconsistent.\n",tdbContainer->track);
	    needsSorting = FALSE;
	    sortableTdbItemCreate(tdbItem,sortOrder);
	    break;
	    }
        }

    // Does this container need to be sorted?
    if(needsSorting && slCount(itemsToSort))
        {
        verbose(2,"Sorting '%s' with %d items\n",tdbContainer->track,slCount(itemsToSort));
        sortTdbItemsAndUpdatePriorities(&itemsToSort);
        countOfSortedContainers++;
        }

    // cleanup
    sortOrderFree(&sortOrder);
    sortableTdbItemsFree(&itemsToSort);
    }
}

void trackDbAddTableField(struct trackDb *tdbList)
/* Add table field by looking it up in settings.  */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    char *table = trackDbLocalSetting(tdb, "table");
    if (table != NULL)
        tdb->table = cloneString(table);
    else
        tdb->table = cloneString(tdb->track);
    }
}

void rGetRefsToDescendants(struct slRef **pList, struct trackDb *tdbList)
/* Add all member of tdbList, and all of their children to pList recursively. */
/* Convert a list to a forest - filling in parent and subtrack pointers.
 * The exact topology of the forest is a little complex due to the
 * fact there are two "inheritance" systems - the superTrack system
 * and the subTrack system.  In the superTrack system (which is on it's
 * way out)  the superTrack's themselves have the tag:
 *     superTrack on
 * and the children of superTracks have the tag:
 *     superTrack parentName
 * In the subTrack system the parents have the tag:
 *     compositeTrack on
 * and the children have the tag:
 *     subTrack parentName
 * In this routine the subtracks are removed from the list, and stuffed into
 * the subtracks lists of their parents.  The highest level parents stay on
 * the list.  There can be multiple levels of inheritance.
 *    For the supertracks the _parents_ are removed from the list.  The only
 * reference to them in the returned forest is that they are in the parent
 * field of their children.  The parents of supertracks have no subtracks
 * after this call currently. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    refAdd(pList, tdb);
    rGetRefsToDescendants(pList, tdb->subtracks);
    }
}

struct slRef *trackDbListGetRefsToDescendants(struct trackDb *tdbList)
/* Return reference list to everything in forest. Do slFreeList when done. */
{
struct slRef *refList = NULL;
rGetRefsToDescendants(&refList, tdbList);
slReverse(&refList);
return refList;
}

int trackDbCountDescendants(struct trackDb *tdb)
/* Count the number of tracks in subtracks list and their subtracks too . */
{
struct slRef *tdbRefList = trackDbListGetRefsToDescendants(tdb->subtracks);
int result = slCount(tdbRefList);
slFreeList(&tdbRefList);
return result;
}

void rGetRefsToDescendantLeaves(struct slRef **pList, struct trackDb *tdbList)
/* Add all leaf members of trackList, and any leaf descendants to pList recursively. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (tdb->subtracks)
	rGetRefsToDescendantLeaves(pList, tdb->subtracks);
    else
	refAdd(pList, tdb);
    }
}

struct slRef *trackDbListGetRefsToDescendantLeaves(struct trackDb *tdbList)
/* Return reference list all leaves in forest. Do slFreeList when done. */
{
struct slRef *refList = NULL;
rGetRefsToDescendantLeaves(&refList, tdbList);
slReverse(&refList);
return refList;
}

int trackDbCountDescendantLeaves(struct trackDb *tdb)
/* Count the number of leaves in children list and their children. */
{
struct slRef *leafRefs = trackDbListGetRefsToDescendantLeaves(tdb->subtracks);
int result = slCount(leafRefs);
slFreeList(&leafRefs);
return result;
}

int trackDbRefCmp(const void *va, const void *vb)
/* Do trackDbCmp on list of references as opposed to actual trackDbs. */
{
const struct slRef *aRef = *((struct slRef **)va);
const struct slRef *bRef = *((struct slRef **)vb);
struct trackDb *a = aRef->val, *b = bRef->val;
return trackDbCmp(&a, &b);
}

struct trackDb *trackDbTopLevelSelfOrParent(struct trackDb *tdb)
/* Look for a parent who is a composite or multiTrack track and return that.  Failing that
 * just return self. */
{
struct trackDb *parent = tdbGetContainer(tdb);
if (parent != NULL)
    return parent;
else
    return tdb;
}

boolean trackDbUpdateOldTag(char **pTag, char **pVal)
/* Look for obscolete tags and update them to new format.  Return TRUE if any update
 * is done.  Will allocate fresh memory for new tag and val if updated. */
{
char *tag = *pTag;
char *val = *pVal;
boolean updated = FALSE;

if (sameString(tag, "subTrack"))
    {
    tag = "parent";
    updated = TRUE;
    }
#ifdef SOON
else if (sameString(tag, "compositeTrack"))
    {
    tag = "container";
    val = "composite";
    updated = TRUE;
    }
else if (sameString(tag, "superTrack"))
    {
    if (sameWord(val, "on"))
        {
	tag = "container";
	val = "folder";
	}
    else
        {
	tag = "parent";
	}
    updated = TRUE;
    }
#endif /* SOON */
if (updated)
    {
    *pTag = cloneString(tag);
    *pVal = cloneString(val);
    }
return updated;
}

static struct tdbExtras *tdbExtrasNew()
// Return a new empty tdbExtras
{
struct tdbExtras *extras;
AllocVar(extras); // Note no need for extras = AllocVar(extras)
// Initialize any values that need an "empty" state
extras->fourState = TDB_EXTRAS_EMPTY_STATE; // I guess it is 5 state!
// pointers are NULL and booleans are FALSE by default
return extras;
}

void tdbExtrasFree(struct tdbExtras **pTdbExtras)
// Frees the tdbExtras structure
{
// Developer, add intelligent routines to free structures
// NOTE: For now just leak contents, because complex structs would also leak
freez(pTdbExtras);
}

static struct tdbExtras *tdbExtrasGet(struct trackDb *tdb)
// Returns tdbExtras struct, initializing if needed.
{
if (tdb->tdbExtras == NULL)   // Temporarily add this back in because Angie see asserts popping.
    tdb->tdbExtras = tdbExtrasNew();
return tdb->tdbExtras;
}

int tdbExtrasFourState(struct trackDb *tdb)
// Returns subtrack four state if known, else TDB_EXTRAS_EMPTY_STATE
{
struct tdbExtras *extras = tdb->tdbExtras;
if (extras)
    return extras->fourState;
return TDB_EXTRAS_EMPTY_STATE;
}

void tdbExtrasFourStateSet(struct trackDb *tdb,int fourState)
// Sets subtrack four state
{
tdbExtrasGet(tdb)->fourState = fourState;
}

boolean tdbExtrasReshapedComposite(struct trackDb *tdb)
// Returns TRUE if composite has been declared as reshaped, else FALSE.
{
struct tdbExtras *extras = tdb->tdbExtras;
if (extras)
    return extras->reshapedComposite;
return FALSE;
}

void tdbExtrasReshapedCompositeSet(struct trackDb *tdb)
// Declares that the composite has been reshaped.
{
tdbExtrasGet(tdb)->reshapedComposite = TRUE;
}

struct mdbObj *tdbExtrasMdb(struct trackDb *tdb)
// Returns mdb metadata if already known, else NULL
{
struct tdbExtras *extras = tdb->tdbExtras;
if (extras)
    return extras->mdb;
return NULL;
}

void tdbExtrasMdbSet(struct trackDb *tdb,struct mdbObj *mdb)
// Sets the mdb metadata structure for later retrieval.
{
tdbExtrasGet(tdb)->mdb = mdb;
}

struct _membersForAll *tdbExtrasMembersForAll(struct trackDb *tdb)
// Returns composite view/dimension members for all, else NULL.
{
struct tdbExtras *extras = tdb->tdbExtras;
if (extras)
    return extras->membersForAll;
return NULL;
}

void tdbExtrasMembersForAllSet(struct trackDb *tdb, struct _membersForAll *membersForAll)
// Sets the composite view/dimensions members for all for later retrieval.
{
tdbExtrasGet(tdb)->membersForAll = membersForAll;
}

struct _membership *tdbExtrasMembership(struct trackDb *tdb)
// Returns subtrack membership if already known, else NULL
{
struct tdbExtras *extras = tdb->tdbExtras;
if (extras)
    return extras->membership;
return tdbExtrasGet(tdb)->membership;
}

void tdbExtrasMembershipSet(struct trackDb *tdb,struct _membership *membership)
// Sets the subtrack membership for later retrieval.
{
tdbExtrasGet(tdb)->membership = membership;
}

