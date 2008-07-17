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

static char const rcsid[] = "$Id: trackDbCustom.c,v 1.41 2008/07/17 22:54:45 markd Exp $";

/* ----------- End of AutoSQL generated code --------------------- */

int trackDbCmp(const void *va, const void *vb)
/* Compare to sort based on priority. */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
   return -1;
else if (dif == 0.0)
   return 0;
else
   return 1;
}

static void parseColor(struct lineFile *lf, char *text, 
	unsigned char *r, unsigned char *g, unsigned char *b)
/* Turn comma-separated string of three numbers into three 
 * color components. */
{
char *words[4];
int wordCount;
wordCount = chopString(text, ", \t", words, ArraySize(words));
if (wordCount != 3)
    errAbort("Expecting 3 comma separated values line %d of %s",
    		lf->lineIx, lf->fileName);
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

static void trackDbAddInfo(struct trackDb *bt, 
	char *var, char *value, struct lineFile *lf)
/* Add info from a variable/value pair to browser table. */
{
if (sameString(var, "track"))
    {
    bt->tableName = cloneString(value);
    }
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
    parseColor(lf, value, &bt->colorR, &bt->colorG, &bt->colorB);
    }
else if (sameWord(var, "altColor"))
    {
    parseColor(lf, value, &bt->altColorR, &bt->altColorG, &bt->altColorB);
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
}

static boolean packableType(char *type)
/* Return TRUE if we can pack this type. */
{
char *t = cloneString(type);
char *s = firstWordInLine(t);
boolean canPack = (sameString("psl", s) || sameString("chain", s) || 
                   sameString("bed", s) || sameString("genePred", s) || 
                   sameString("expRatio", s) || sameString("wigMaf", s) ||
		   sameString("factorSource", s) || sameString("bed5FloatScore", s) || 
		   sameString("bed6FloatScore", s) || sameString("altGraphX", s));
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
   Return NULL if line doesn't contain #include */
{
static char incFile[256];
char *file;

/* For transition, allow with or without leading #.
   Later, we'll only allow w/o # */
if (startsWith("#include", line) || startsWith("include", line))
    {
    splitPath(raFile, incFile, NULL, NULL);
    nextWord(&line);
    file = nextQuotedWord(&line);
    strcat(incFile, file);
    printf("found include file: %s\n", incFile);
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
        if (startsWith("track", line))
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
        trackDbSetting(bt, "noInherit") == NULL)
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
            td->priority = atof(priority);
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

inline boolean tdbIsSuper(struct trackDb *tdb)
/* Is this trackDb struct marked as a supertrack ? */
{ return tdb && SUPERTRACK_NODE(tdb->treeNodeType); }

inline boolean tdbIsSuperTrack(struct trackDb *tdb) 
/* Is this trackDb struct marked as a supertrack ? */
{ return tdb && /*tdb->subtracks &&*/ SUPERTRACK_NODE(tdb->treeNodeType); }  // TODO: superTrack code needs rewrite to contain it's children

inline boolean tdbIsComposite( struct trackDb *tdb) 
/* Is this trackDb struct marked as a composite with children ?  */
{ return tdb && tdb->subtracks && COMPOSITE_NODE( tdb->treeNodeType); }

inline boolean tdbIsSuperTrackChild(struct trackDb *tdb) 
/* Is this trackDb struct marked as a child of a supertrack ?  */
{ return tdb && tdb->parent && SUPERTRACK_CHILD_NODE(tdb->treeNodeType); }

inline boolean tdbIsCompositeChild(struct trackDb *tdb)  
/* Is this trackDb struct marked as a child of a composite track ?  */
{ return tdb && tdb->parent && COMPOSITE_CHILD_NODE( tdb->treeNodeType); }

inline void tdbMarkAsSuperTrack(struct trackDb *tdb)      
/* Marks a trackDb struct as a supertrack */
{ tdb->treeNodeType |= SUPERTRACK_MASK;       }

inline void tdbMarkAsComposite( struct trackDb *tdb)      
/* Marks a trackDb struct as a composite track  */
{ tdb->treeNodeType |= COMPOSITE_MASK;        }

inline void tdbMarkAsSuperTrackChild(struct trackDb *tdb) 
/* Marks a trackDb struct as a child of a supertrack  */
{ tdb->treeNodeType |= SUPERTRACK_CHILD_MASK; }

inline void tdbMarkAsCompositeChild( struct trackDb *tdb) 
/* Marks a trackDb struct as a child of a composite track  */
{ tdb->treeNodeType |= COMPOSITE_CHILD_MASK;  }

