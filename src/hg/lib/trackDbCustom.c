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
#include "sqlNum.h"
#include "obscure.h"
#include "hgMaf.h"
#include "customTrack.h"

static char const rcsid[] = "$Id: trackDbCustom.c,v 1.72 2009/11/30 17:58:11 kent Exp $";

/* ----------- End of AutoSQL generated code --------------------- */

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
char *words[4];
int wordCount;
wordCount = chopString(text, ", \t", words, ArraySize(words));
if (wordCount != 3)
    errAbort("Expecting 3 comma separated values in %s.", text);
*r = atoi(words[0]);
*g = atoi(words[1]);
*b = atoi(words[2]);
}

static unsigned char parseVisibility(char *value, struct lineFile *lf)
/* Parse a visibility value */
{
if (sameString(value, "hide") || sameString(value, "0"))
    return tvHide;
else if (sameString(value, "dense") || sameString(value, "1"))
    return tvDense;
else if (sameString(value, "full") || sameString(value, "2"))
    return tvFull;
else if (sameString(value, "pack") || sameString(value, "3"))
    return tvPack;
else if (sameString(value, "squish") || sameString(value, "4"))
    return tvSquish;
else
    errAbort("Unknown visibility %s line %d of %s",
             value, lf->lineIx, lf->fileName);
return tvHide;  /* never reached */
}

static void parseTrackLine(struct trackDb *bt, char *value,
                           struct lineFile *lf)
/* parse the track line */
{
char *val2 = cloneString(value);
bt->tableName = nextWord(&val2);

// check for override option
if (val2 != NULL)
    {
    val2 = trimSpaces(val2);
    if (!sameString(val2, "override"))
        errAbort("invalid track line: %s:%d: track %s", lf->fileName, lf->lineIx, value);
    bt->overrides = hashNew(0);
    }
}

static void trackDbAddInfo(struct trackDb *bt,
	char *var, char *value, struct lineFile *lf)
/* Add info from a variable/value pair to browser table. */
{
if (sameString(var, "track"))
    parseTrackLine(bt, value, lf);
else if (sameString(var, "shortLabel") || sameString(var, "name"))
    bt->shortLabel = cloneString(value);
else if (sameString(var, "longLabel") || sameString(var, "description"))
    bt->longLabel = cloneString(value);
else if (sameString(var, "priority"))
    bt->priority = atof(value);
else if (sameWord(var, "url"))
    bt->url = cloneString(value);
else if (sameString(var, "visibility"))
    {
    bt->visibility =  parseVisibility(value, lf);
    }
else if (sameWord(var, "color"))
    {
    parseColor(value, &bt->colorR, &bt->colorG, &bt->colorB);
    }
else if (sameWord(var, "altColor"))
    {
    parseColor(value, &bt->altColorR, &bt->altColorG, &bt->altColorB);
    }
else if (sameWord(var, "type"))
    {
    bt->type = cloneString(value);
    }
else if (sameWord(var, "spectrum") || sameWord(var, "useScore"))
    {
    bt->useScore = TRUE;
    }
else if (sameWord(var, "canPack"))
    {
    bt->canPack = !(sameString(value, "0") || sameString(value, "off"));
    }
else if (sameWord(var, "chromosomes"))
    sqlStringDynamicArray(value, &bt->restrictList, &bt->restrictCount);
else if (sameWord(var, "private"))
    bt->private = TRUE;
else if (sameWord(var, "group"))
    {
    bt->grp = cloneString(value);
    }
else	/* Add to settings. */
    {
    if (bt->settingsHash == NULL)
	bt->settingsHash = hashNew(7);
    hashAdd(bt->settingsHash, var, cloneString(value));
    }
if (bt->overrides != NULL)
    hashAdd(bt->overrides, var, NULL);
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
if (sameString(var, "track"))
    {
    // skip
    }
else if (sameString(var, "shortLabel") || sameString(var, "name"))
    replaceStr(&td->shortLabel, overTd->shortLabel);
else if (sameString(var, "longLabel") || sameString(var, "description"))
    replaceStr(&td->longLabel, overTd->longLabel);
else if (sameString(var, "priority"))
    td->priority = overTd->priority;
else if (sameWord(var, "url"))
    replaceStr(&td->url, overTd->url);
else if (sameString(var, "visibility"))
    td->visibility =  overTd->visibility;
else if (sameWord(var, "color"))
    {
    td->colorR = overTd->colorR;
    td->colorG = overTd->colorG;
    td->colorB = overTd->colorB;
    }
else if (sameWord(var, "altColor"))
    {
    td->altColorR = overTd->altColorR;
    td->altColorG = overTd->altColorG;
    td->altColorB = overTd->altColorB;
    }
else if (sameWord(var, "type"))
    replaceStr(&td->type, overTd->type);
else if (sameWord(var, "spectrum") || sameWord(var, "useScore"))
    td->useScore = overTd->useScore;
else if (sameWord(var, "canPack"))
    td->canPack = overTd->canPack;
else if (sameWord(var, "chromosomes"))
    {
    // just format and re-parse
    char *sa = sqlStringArrayToString(overTd->restrictList, overTd->restrictCount);
    sqlStringFreeDynamicArray(&td->restrictList);
    sqlStringDynamicArray(sa, &td->restrictList, &td->restrictCount);
    freeMem(sa);
    }
else if (sameWord(var, "private"))
    td->private = overTd->private;
else if (sameWord(var, "group"))
    {
    replaceStr(&td->grp, overTd->grp);
    }
else if (sameWord(var, "release"))
    {
    // ignore -- it was pruned by pruneRelease before this was called.
    }
else	/* Add to settings. */
    {
    if (td->settingsHash == NULL)
	td->settingsHash = hashNew(7);
    char *val = hashMustFindVal(overTd->settingsHash, var);
    struct hashEl *hel = hashStore(td->settingsHash, var);
    replaceStr((char**)&hel->val, val);
    }
}

void trackDbOverride(struct trackDb *td, struct trackDb *overTd)
/* apply an trackOverride trackDb entry to a trackDb entry */
{
assert(overTd->overrides != NULL);
struct hashEl *hel;
struct hashCookie hc = hashFirst(overTd->overrides);
while ((hel = hashNext(&hc)) != NULL)
    overrideField(td, overTd, hel->name);
}

static boolean packableType(char *type)
/* Return TRUE if we can pack this type. */
{
char *t = cloneString(type);
char *s = firstWordInLine(t);
boolean canPack = (sameString("psl", s) || sameString("chain", s) ||
                   sameString("bed", s) || sameString("genePred", s) ||
		   sameString("bigBed", s) ||
                   sameString("expRatio", s) || sameString("wigMaf", s) ||
		   sameString("factorSource", s) || sameString("bed5FloatScore", s) ||
		   sameString("bed6FloatScore", s) || sameString("altGraphX", s) ||
		   sameString("bam", s));
freeMem(t);
return canPack;
}


void trackDbInherit(struct trackDb *bt, struct trackDb *composite)
/* Fill in some missing values with values from parent track */
{
if (bt->type == NULL)
    bt->type = cloneString(composite->type);
if (bt->url == NULL && composite->url != NULL)
    bt->url = cloneString(composite->url);
if (bt->grp == NULL)
    bt->grp = cloneString(composite->grp);
if (bt->canPack == 2 && composite->canPack != 2)
    bt->canPack = composite->canPack;
if (composite->private)
    bt->private = TRUE;
if (composite->useScore)
    bt->useScore = TRUE;
}


void trackDbPolish(struct trackDb *bt)
/* Fill in missing values with defaults. */
{
if (bt->shortLabel == NULL)
    bt->shortLabel = cloneString(bt->tableName);
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

char *trackDbInclude(char *raFile, char *line)
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
    return cloneString(incFile);
    }
else
    return NULL;
}

struct trackDb *trackDbFromRa(char *raFile)
/* Load track info from ra file into list. */
{
struct lineFile *lf = lineFileOpen(raFile, TRUE);
char *line, *word;
struct trackDb *btList = NULL, *bt;
boolean done = FALSE;
struct hash *compositeHash = hashNew(8);
char *incFile;

for (;;)
    {
    /* Seek to next line that starts with 'track' */
    for (;;)
	{
	if (!lineFileNext(lf, &line, NULL))
	   {
	   done = TRUE;
	   break;
	   }
	line = skipLeadingSpaces(line);
        if (startsWithWord("track", line))
            {
            lineFileReuse(lf);
            break;
            }
        else if ((incFile = trackDbInclude(raFile, line)) != NULL)
            {
            struct trackDb *incTdb = trackDbFromRa(incFile);
            btList = slCat(btList, incTdb);
            }
	}
    if (done)
        break;

    /* Allocate track structure and fill it in until next blank line. */
    AllocVar(bt);
    bt->canPack = 2;	/* Unknown value */
    slAddHead(&btList, bt);
    for (;;)
        {
	/* Break at blank line or EOF. */
	if (!lineFileNext(lf, &line, NULL))
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
	trackDbAddInfo(bt, word, line, lf);
	}
    if (trackDbSetting(bt, "compositeTrack") != NULL)
        hashAdd(compositeHash, bt->tableName, bt);
    }
lineFileClose(&lf);

for (bt = btList; bt != NULL; bt = bt->next)
    {
    struct trackDb *compositeTdb;
    char *compositeName;
    if ((compositeName = trackDbSetting(bt, "subTrack")) != NULL &&
        trackDbSettingClosestToHome(bt, "noInherit") == NULL)
            if ((compositeTdb =
                    hashFindVal(compositeHash, compositeName)) != NULL)
                trackDbInherit(bt, compositeTdb);
    trackDbPolish(bt);
    }
slReverse(&btList);
return btList;
}

void trackDbOverrideVisbility(struct hash *tdHash, char *visibilityRa,
			      boolean hideFirst)
/* Override visbility settings using a ra file.  If hideFirst, set all
 * visibilities to hide before applying visibilityRa. */
{
struct lineFile *lf;
struct hash *raRecord;

if (hideFirst)
    {
    /* Set visibility to hide on all entries */
    struct hashEl *hel;
    struct hashCookie cookie;
    cookie = hashFirst(tdHash);
    while ((hel = hashNext(&cookie)) != NULL)
	((struct trackDb *)hel->val)->visibility = tvHide;
    }

/* Parse the ra file, adjusting visibility accordingly */
lf = lineFileOpen(visibilityRa, TRUE);
while ((raRecord = raNextRecord(lf)) != NULL)
    {
    char *trackName = hashFindVal(raRecord, "track");
    char *visibility = hashFindVal(raRecord, "visibility");
    if ((trackName != NULL) && (visibility != NULL))
        {
        struct trackDb *td = hashFindVal(tdHash, trackName);
        if (td != NULL)
            td->visibility = parseVisibility(visibility, lf);
        }
    hashFree(&raRecord);
    }
lineFileClose(&lf);
}

void trackDbOverridePriority(struct hash *tdHash, char *priorityRa)
/* Override priority settings using a ra file. */
{
struct lineFile *lf;
struct hash *raRecord;

/* Parse the ra file, adjusting priority accordingly */
lf = lineFileOpen(priorityRa, TRUE);
while ((raRecord = raNextRecord(lf)) != NULL)
    {
    char *trackName = hashFindVal(raRecord, "track");
    char *priority = hashFindVal(raRecord, "priority");
    if ((trackName != NULL) && (priority != NULL))
        {
        struct trackDb *td = hashFindVal(tdHash, trackName);
        if (td != NULL)
            {
            td->priority = atof(priority);
            trackDbPolish(td);
            }
        }
    hashFree(&raRecord);
    }
lineFileClose(&lf);
}

struct hash *trackDbHashSettings(struct trackDb *tdb)
/* Force trackDb to hash up it's settings.  Usually this is just
 * done on demand. Returns settings hash. */
{
if (tdb->settingsHash == NULL)
    tdb->settingsHash = raFromString(tdb->settings);
return tdb->settingsHash;
}

char *trackDbSetting(struct trackDb *tdb, char *name)
/* Return setting string or NULL if none exists. */
{
if (tdb == NULL)
    errAbort("Program error: null tdb passed to trackDbSetting.");
if (tdb->settingsHash == NULL)
    tdb->settingsHash = raFromString(tdb->settings);
return hashFindVal(tdb->settingsHash, name);
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
   errAbort("Missing required %s setting in %s track", name, tdb->tableName);
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
tdb->parentName = cloneString(stInfo->parentName);
tdb->visibility = stInfo->defaultVis;
tdbMarkAsSuperTrackChild(tdb);
if(tdb->parent)
    {
    tdbMarkAsSuperTrack(tdb->parent);
    //if(tdb->parent->subtracks || tdb->next == NULL)  // TODO: Currently supertracks Sometimes don't contain their subtracks!
    //   slAddHead(&tdb->parent->subtracks, tdb);
    }
freeMem(stInfo);
}

void trackDbSuperSettings(struct trackDb *tdbList)
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
        if (!hashLookup(superHash, tdb->tableName))
            hashAdd(superHash, tdb->tableName, tdb);
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
        tdb->parent = hashFindVal(superHash, stInfo->parentName); // TODO: Parent wont contain children: and can't in this list!
        if (tdb->parent)
            trackDbSuperMemberSettings(tdb);
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
        char *freeze = hFreezeFromDb(origAssembly);
        printf("<B>Data coordinates converted via <A TARGET=_BLANK HREF=\"../goldenPath/help/hgTracksHelp.html#Liftover\">liftOver</A> from:</B> %s %s%s%s<BR>\n", freeze ? freeze : "", freeze ? "(" : "", origAssembly, freeze ? ")":"");
        }
    }
}

eCfgType cfgTypeFromTdb(struct trackDb *tdb, boolean warnIfNecessary)
/* determine what kind of track specific configuration is needed,
   warn if not multi-view compatible */
{
eCfgType cType = cfgNone;
if(startsWith("wigMaf", tdb->type))
    cType = cfgWigMaf;
else if(startsWith("wig", tdb->type))
    cType = cfgWig;
else if(startsWith("bigWig", tdb->type))
    cType = cfgWig;
else if(startsWith("bedGraph", tdb->type))
    cType = cfgWig;
else if(startsWith("netAlign", tdb->type))
    {
    cType = cfgNetAlign;
    warnIfNecessary = FALSE;
    }
else if(sameWord("bed5FloatScore",       tdb->type)
     || sameWord("bed5FloatScoreWithFdr",tdb->type))
    cType = cfgBedScore;
else if(sameWord("narrowPeak",tdb->type)
     || sameWord("broadPeak", tdb->type)
     || sameWord("encodePeak",tdb->type)
     || sameWord("gappedPeak",tdb->type))
    cType = cfgPeak;
else if(sameWord("genePred",tdb->type))
        cType = cfgGenePred;
else if(startsWith("bed ", tdb->type) || startsWith("bigBed ", tdb->type)) // TODO: Only these are configurable so far
    {
    char *words[3];
    chopLine(cloneString( tdb->type), words);
    if (trackDbSetting(tdb, "bedFilter") != NULL)
	   cType = cfgBedFilt;
    else if (atoi(words[1]) >= 5 && trackDbSettingClosestToHome(tdb, "noScoreFilter") == NULL)
        cType = cfgBedScore;
    }
else if(startsWith("chain",tdb->type))
    cType = cfgChain;

if(cType == cfgNone && warnIfNecessary)
    {
    if(!startsWith("bed ", tdb->type) && !startsWith("bigBed", tdb->type)
    && subgroupFind(tdb,"view",NULL))
        warn("Track type \"%s\" is not yet supported in multi-view composites for %s.",tdb->type,tdb->tableName);
    }
return cType;
}


 
char *trackDbCompositeSettingByView(struct trackDb *parentTdb, char* view, char *name)
/* Get a trackDb setting at the view level for a multiview composite.
   returns a string that must be freed */
{
char *trackSetting = NULL;
char *settingsByView = cloneString(trackDbSetting(parentTdb,"settingsByView"));
if(settingsByView != NULL)
    {
    char *settingForAView = NULL;
    char *words[32];
    int cnt,ix;
    // parse settingsByView "Signal:viewLimits=5:500,viewLimitsMax=0:20910 ..."
    cnt = chopLine(cloneString(settingsByView), words);
    for(ix=0;ix<cnt;ix++)
        {
        if(startsWithWordByDelimiter(view,':',words[ix]))
            {
            settingForAView = cloneString(words[ix]+(strlen(view)+1));
            break;
            }
        }
    freeMem(settingsByView);
    if(settingForAView != NULL) // found a match
        {
        // parse settingByView: "viewLimits=5:500,viewLimitsMax=0:20910"
        cnt = chopByChar(settingForAView,',',words,ArraySize(words));
        for(ix=0;ix<cnt;ix++)
            {
            if(startsWithWordByDelimiter(name,'=',words[ix]))
                {
                trackSetting = cloneString(words[ix]+strlen(name)+1);
                break;
                }
            }
        freeMem(settingForAView);
        }
    }
return trackSetting;
}

char *trackDbSettingByView(struct trackDb *tdb, char *name)
/* For a subtrack of a multiview composite, get a setting stored in the parent settingByView.
   returns a string that must be freed */
{
char * view;
if(tdbIsCompositeChild(tdb) && subgroupFind(tdb,"view",&view))
    {
    return trackDbCompositeSettingByView(tdb->parent,view,name);
    }
return NULL;
}


char *trackDbSettingClosestToHome(struct trackDb *tdb, char *name)
/* Look for a trackDb setting from lowest level on up:
   from subtrack, then composite, then settingsByView, then composite */
{
char *trackSetting = trackDbSetting(tdb,name);
if(trackSetting == NULL && tdbIsCompositeChild(tdb))
    {
    trackSetting = trackDbSettingByView(tdb,name);
    if(trackSetting == NULL)
        trackSetting = trackDbSetting(tdb->parent,name);
    }
return trackSetting;
}

char *trackDbSettingClosestToHomeOrDefault(struct trackDb *tdb, char *name, char *defaultVal)
/* Look for a trackDb setting (or default) from lowest level on up:
   from subtrack, then composite, then settingsByView, then composite */
{
char *trackSetting = trackDbSettingClosestToHome(tdb,name);
if(trackSetting == NULL)
    trackSetting = defaultVal;
return trackSetting;
}

boolean trackDbSettingClosestToHomeOn(struct trackDb *tdb, char *name)
/* Return true if a tdb setting closest to home is "on" "true" or "enabled". */
{
char *setting = trackDbSettingClosestToHome(tdb,name);
return  (setting && (   sameWord(setting,"on")
                     || sameWord(setting,"true")
                     || sameWord(setting,"enabled")
                     || atoi(setting) != 0));
}

struct trackDb *subTdbFind(struct trackDb *parent,char *table)
/* Return subTrack tdb if it exists in parent. */
{
if(parent == NULL)
    return NULL;

struct trackDb *tdb;
for (tdb = parent->subtracks;
     tdb != NULL && differentString(tdb->tableName,table);
     tdb = tdb->next) {}
return tdb;
}

struct trackDb *tdbFindOrCreate(char *db,struct trackDb *parent,char *table)
/* Find or creates the tdb for this table. May return NULL. */
{
struct trackDb *tdb = NULL;
if (parent != NULL)
    {
    if(sameString(parent->tableName, table))
        tdb = parent;
    else if(consWiggleFind(db,parent,table) != NULL)
        tdb = parent;
    else
        tdb = subTdbFind(parent,table);
    }
if(tdb == NULL && db != NULL)
    {
    struct sqlConnection *conn = hAllocConn(db);
    tdb = hMaybeTrackInfo(conn, table);
    hFreeConn(&conn);
    }
return tdb;
}

metadata_t *metadataSettingGet(struct trackDb *tdb)
/* Looks for a metadata tag and parses the setting into arrays of tags and values */
{
char *setting = trackDbSetting(tdb, "metadata");
if(setting == NULL)
    return NULL;
int count = countChars(setting,'='); // <= actual count, since value may contain a =
if(count <= 0)
    return NULL;

metadata_t *metadata = needMem(sizeof(metadata_t));
metadata->setting    = cloneString(setting);
metadata->tags = needMem(sizeof(char*)*count);
metadata->values = needMem(sizeof(char*)*count);
char *cp = metadata->setting;
for(metadata->count=0;*cp != '\0' && metadata->count<count;metadata->count++)
    {
    metadata->tags[metadata->count] = cloneNextWordByDelimiter(&cp,'=');
    if(*cp != '"')
        metadata->values[metadata->count] = cloneNextWordByDelimiter(&cp,' ');
    else
        {
        metadata->values[metadata->count] = ++cp;
        for(;*cp != '\0';cp++)
            {
            if(*cp == '"' && *(cp - 1) != '\\') // Not escaped
                {
                *cp = '\0';
                metadata->values[metadata->count] = replaceChars(metadata->values[metadata->count],"\\\"","\"");
                *cp = '"'; // Put it baaack!
                break;
                }
            }
        if(*cp == '\0') // Didn't find close quote!
            metadata->values[metadata->count] = cloneString(metadata->values[metadata->count]);
        else
            cp++;
        }
    }

return metadata;
}

void metadataFree(metadata_t **metadata)
/* frees any previously obtained metadata setting */
{
if(metadata && *metadata)
    {
    for(;(*metadata)->count > 0;(*metadata)->count--)
        {
        freeMem((*metadata)->tags[  (*metadata)->count - 1]);
        freeMem((*metadata)->values[(*metadata)->count - 1]);
        }
    freeMem((*metadata)->setting);
    freez(metadata);
    }
}

char *metadataSettingFind(struct trackDb *tdb,char *name)
/* Looks for a specific metadata setting and returns the value or null
   returned value should be freed */
{
metadata_t *metadata = metadataSettingGet(tdb);
if(metadata == NULL)
    return NULL;

int ix=0;
char *setting = NULL;
for(;ix<metadata->count;ix++)
    {
    if (sameString(metadata->tags[ix],name))
        {
        setting = cloneString(metadata->values[ix]);
        break;
        }
    }
metadataFree(&metadata);
return setting;
}

int parentTdbAbandonTablelessChildren(char *db, struct trackDb *parentTdb)
/* abandons tableless children from a container tdb, such as a composite
   returns count of children that have been abandoned */
{
struct trackDb *goodKids = NULL;
struct trackDb *childTdb;
int badKids = 0;

while((childTdb = slPopHead(&(parentTdb->subtracks))) != NULL)
    {
    if (hTableOrSplitExists(db, childTdb->tableName))
        slAddHead(&goodKids,childTdb);
    else
        {
        badKids++;
        trackDbFree(&childTdb);
        }
    }
if(goodKids != NULL)
    slReverse(&goodKids);
parentTdb->subtracks = goodKids;
return badKids;
}

