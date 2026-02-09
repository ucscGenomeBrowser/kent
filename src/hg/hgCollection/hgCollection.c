/* hgCollection - hub builder */

/* Copyright (C) 2017 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "cartTrackDb.h"
#include "trackHub.h"
#include "trashDir.h"
#include "hubConnect.h"
#include "hui.h"
#include "grp.h"
#include "cheapcgi.h"
#include "jsHelper.h"
#include "web.h"
#include "knetUdc.h"
#include "api.h"
#include "genbank.h"
#include "htmshell.h"
#include "jsonParse.h"
#include "customComposite.h"
#include "stdlib.h"
#include "wikiLink.h"

/* Tool tips */
#define COLLECTION_TITLE  "Double-click to edit name and color"
#define FOLDER_TITLE      "Click to open node"
#define TRACK_TITLE       "Press Green Plus to add track to collection"

/* Global Variables */
struct hash *oldVars = NULL;	/* The cart before new cgi stuff added. */
// Null terminated list of CGI Variables we don't want to save permanently:
char *excludeVars[] = {"Submit", "submit", "cmd", "track", "collection", "jsonp", NULL,};

struct track
{
struct track *next;
struct track *trackList;
struct trackDb *tdb;
char *name;
char *shortLabel;
char *longLabel;
unsigned long color;
};

struct trackDbRef 
{
struct trackDbRef *next;
struct trackDb *tdb;
struct grp *grp;
double priority;
int order;
};

static char *makeUniqueLabel(struct hash *labelHash, char *label)
// Make the short label  of this track unique.
{
if (hashLookup(labelHash, label) == NULL)
    {
    hashStore(labelHash, label);
    return label;
    }

unsigned count = 1;
char buffer[4096];

for(;; count++)
    {
    safef(buffer, sizeof buffer, "%s (%d)", label, count);
    if (hashLookup(labelHash, buffer) == NULL)
        {
        hashStore(labelHash, buffer);
        return cloneString(buffer);
        }
    }

return NULL;
}

static char *makeUniqueName(struct hash *nameHash, char *name)
// Make the name of this track unique.
{
char *skipHub = trackHubSkipHubName(name);

if (hashLookup(nameHash, skipHub) == NULL)
    {
    hashStore(nameHash, skipHub);
    return skipHub;
    }

char base[4096];
safef(base, sizeof base, "%s_%lx",skipHub, time(NULL) - 1520629086);

unsigned count = 0;
char buffer[4096];

for(;; count++)
    {
    safef(buffer, sizeof buffer, "%s%d", base, count);
    if (hashLookup(nameHash, buffer) == NULL)
        {
        hashStore(nameHash, buffer);
        return cloneString(buffer);
        }
    }

return NULL;
}

static struct trackDb *createComposite(char *collectionName, char *shortLabel, char *longLabel, long color, int priority)
// Create a trackDb entry for a new composite
{
struct trackDb *tdb;
char buffer[512];

AllocVar(tdb);
tdb->settingsHash = newHash(5);
tdb->type = cloneString("mathWig");

safef(buffer, sizeof buffer, "%ld,%ld,%ld", 0xff & (color >> 16),0xff & (color >> 8),0xff & color);
hashAdd(tdb->settingsHash, "color", cloneString(buffer));

safef(buffer, sizeof buffer, "%d", priority);
hashAdd(tdb->settingsHash, "priority", cloneString(buffer));

hashAdd(tdb->settingsHash, "track", collectionName);
hashAdd(tdb->settingsHash, "shortLabel", shortLabel);
hashAdd(tdb->settingsHash, "longLabel", longLabel);
hashAdd(tdb->settingsHash, "autoScale", "on");
hashAdd(tdb->settingsHash, "compositeTrack", "on");
hashAdd(tdb->settingsHash, "aggregate", "none");
hashAdd(tdb->settingsHash, "type", "mathWig");
hashAdd(tdb->settingsHash, "visibility", "full");
hashAdd(tdb->settingsHash, "customized", "on");
hashAdd(tdb->settingsHash, "maxHeightPixels", "10000:30:11");
hashAdd(tdb->settingsHash, "showSubtrackColorOnUi", "on");
hashAdd(tdb->settingsHash, CUSTOM_COMPOSITE_SETTING, "on");

return tdb;
}

static boolean trackCanBeAdded(struct trackDb *tdb)
// are we allowing this track into a custom composite
{
return  (tdb->subtracks == NULL) && !startsWith("wigMaf",tdb->type) &&  (startsWith("wig",tdb->type) || startsWith("bigWig",tdb->type) || startsWith("bedGraph",tdb->type)) ;
}

static char *escapeLabel(char *label)
// put a blackslash in front of any single quotes in the input
{
char buffer[4096], *eptr = buffer;
for(; *label; label++)
    {
    if (*label == '\'')
        {
        *eptr++ = '\\';
        *eptr++ = '\'';
        }
    else
        *eptr++ = *label;
    }

*eptr = 0;

return cloneString(buffer);
}

static void trackToClient(char *parent, struct trackDb *tdb,  boolean user)
// output list elements for a group
{
char *userString = "";
char *title;

if (user)
    title = COLLECTION_TITLE;
else if (tdb->subtracks)
    title = FOLDER_TITLE;
else
    title = TRACK_TITLE;

if (tdb->subtracks)
    userString = "icon:'../images/folderC.png',children:true,";
else if (user)
    userString = "icon:'fa fa-minus-square',";
else
    userString = "icon:'fa fa-plus',";
    
#define IMAKECOLOR_32(r,g,b) ( ((unsigned int)b<<0) | ((unsigned int)g << 8) | ((unsigned int)r << 16))

jsInlineF("{%s id:'%s',li_attr:{title:'%s',shortlabel:'%s', longlabel:'%s',color:'#%06x',name:'%s'},text:'%s (%s)',parent:'%s'}",userString, trackHubSkipHubName(tdb->track),title, escapeLabel(tdb->shortLabel), escapeLabel(tdb->longLabel), IMAKECOLOR_32(tdb->colorR,tdb->colorG,tdb->colorB),trackHubSkipHubName(tdb->track),escapeLabel(tdb->shortLabel),escapeLabel(tdb->longLabel),parent);
}

static void outHubHeader(FILE *f, char *db)
// output a track hub header
{
fprintf(f,"hub hub1\n\
shortLabel Track Collections\n\
longLabel Track Collections\n\
useOneFile on\n\
email genome-www@soe.ucsc.edu\n\n");
fprintf(f,"genome %s\n\n", db);  
}


static char *getHubName(struct cart *cart, char *db, boolean doCreate)
// get the name of the hub to use for user collections.  Create the hub if doCreate is TRUE
{
struct tempName hubTn;
char buffer[4096];
safef(buffer, sizeof buffer, "%s-%s", customCompositeCartName, db);
char *hubName = cartOptionalString(cart, buffer);
int fd = -1;

if (!doCreate && (hubName == NULL))
    return NULL;

if ((hubName == NULL) || ((fd = open(hubName, 0)) < 0))
    {
    trashDirDateFile(&hubTn, "hgComposite", "hub", ".txt");
    hubName = cloneString(hubTn.forCgi);
    cartSetString(cart, buffer, hubName);
    FILE *f = mustOpen(hubName, "a");
    outHubHeader(f, db);
    fclose(f);
    }

if (fd >= 0)
    close(fd);

cartSetString(cart, "hubUrl", hubName);
cartSetString(cart, hgHubConnectRemakeTrackHub, hubName);
return hubName;
}

static bool subtrackEnabledInTdb(struct trackDb *subTdb)
/* Return TRUE unless the subtrack was declared with "subTrack ... off". */
{
bool enabled = TRUE;
char *words[2];
char *setting;
if ((setting = trackDbLocalSetting(subTdb, "parent")) != NULL)
    {
    if (chopLine(cloneString(setting), words) >= 2)
        if (sameString(words[1], "off"))
            enabled = FALSE;
    }
else
    return subTdb->visibility != tvHide;

return enabled;
}

static bool isSubtrackVisible(struct cart *cart, struct trackDb *tdb)
/* Has this subtrack not been deselected in hgTrackUi or declared with
 *  * "subTrack ... off"?  -- assumes composite track is visible. */
{
boolean overrideComposite = (NULL != cartOptionalString(cart, tdb->track));
bool enabledInTdb = subtrackEnabledInTdb(tdb);
char option[1024];
safef(option, sizeof(option), "%s_sel", tdb->track);
boolean enabled = cartUsualBoolean(cart, option, enabledInTdb);
if (overrideComposite)
    enabled = TRUE;
return enabled;
}


static bool isParentVisible(struct cart *cart, struct trackDb *tdb)
// Are this track's parents visible?
{
if (tdb->parent == NULL)
    return TRUE;

if (!isParentVisible(cart, tdb->parent))
    return FALSE;

char *cartVis = cartOptionalString(cart, tdb->parent->track);
boolean vis;
if (cartVis != NULL) 
    vis =  differentString(cartVis, "hide");
else if (tdbIsSuperTrack(tdb->parent))
    vis = tdb->parent->isShow;
else
    vis = tdb->parent->visibility != tvHide;

return vis;
}


static void checkForVisible(struct cart *cart, struct grp *grp, struct trackDbRef **list, struct trackDb *tdb, double priority, double multiplier)
/* Walk the trackDb hierarchy looking for visible leaf tracks. */
{
struct trackDb *subTdb;
char buffer[4096];

if (tdb->subtracks)
    {
    for(subTdb = tdb->subtracks; subTdb; subTdb = subTdb->next)
        checkForVisible(cart, grp, list, subTdb, priority + tdb->priority * multiplier, multiplier / 100.0);
    }
else
    {
    boolean isVisible = FALSE;
    if (tdb->parent == NULL) 
        {
        char *cartVis = cartOptionalString(cart, tdb->track);
        if (cartVis == NULL)
            isVisible =  tdb->visibility != tvHide;
        else
            isVisible =  differentString(cartVis, "hide");
        }
    else if (isParentVisible(cart, tdb) &&  isSubtrackVisible(cart, tdb))
        isVisible = TRUE;

    if (isVisible)
        {
        struct trackDbRef *tdbRef;
        AllocVar(tdbRef);
        tdbRef->tdb = tdb;
        tdbRef->grp = grp;
        slAddHead(list, tdbRef);
        safef(buffer, sizeof buffer, "%s_imgOrd", tdb->track);

        tdbRef->order = cartUsualInt(cart, buffer,  0);
        tdbRef->priority = priority + multiplier * tdb->priority;
        }
    }
}

static int tdbRefCompare (const void *va, const void *vb)
// Compare to sort on imgTrack->order.
{
const struct trackDbRef *a = *((struct trackDbRef **)va);
const struct trackDbRef *b = *((struct trackDbRef **)vb);

int dif = a->order - b->order;

if (dif == 0)
    {
    double ddif = a->priority - b->priority ;
    if (ddif < 0)
        dif = -1;
    else if (ddif > 0)
        dif = 1;
    }
if (dif == 0)
    dif = strcasecmp(a->tdb->shortLabel, b->tdb->shortLabel);

return dif;
}       

static void addVisibleTracks(struct hash *groupHash, struct dyString *rootChildren, struct cart *cart, struct trackDb *trackList)
// add the visible tracks table rows.
{
struct trackDb *tdb;
struct trackDbRef *tdbRefList = NULL, *tdbRef;

for(tdb = trackList; tdb; tdb = tdb->next)
    {
    double priority = tdb->priority/100.0;
    struct grp *grp = hashFindVal(groupHash, tdb->grp);
    if (grp)
        priority += grp->priority;

    checkForVisible(cart, grp, &tdbRefList, tdb,  priority, 1.0/100.0);
    }

slSort(&tdbRefList, tdbRefCompare);
if (!isEmpty(rootChildren->string))
    dyStringPrintf(rootChildren, ",");
dyStringPrintf(rootChildren, "{icon:'../images/folderC.png',id:'visible', text:'Visible Tracks', parent:'#', li_attr:{title:'%s'} ", FOLDER_TITLE);
if (tdbRefList != NULL)
    dyStringPrintf(rootChildren, ",children:true");
dyStringPrintf(rootChildren, "}");

jsInlineF("trackData['visible'] = [");
for(tdbRef = tdbRefList; tdbRef; tdbRef = tdbRef->next)
    {
    trackToClient("visible", tdbRef->tdb,  FALSE);
    if (tdbRef->next != NULL)
        jsInlineF(",");
    }
jsInlineF("];");
}

void subTracksToClient(char *arrayName, struct trackDb *parentTdb, boolean user)
{
if (parentTdb->subtracks == NULL)
    return;
jsInlineF("%s['%s'] = [", arrayName, trackHubSkipHubName(parentTdb->track));
boolean first = TRUE;
struct trackDb *tdb;
for(tdb = parentTdb->subtracks; tdb;  tdb = tdb->next)
    {
    if (!first)
        jsInlineF(",");
    trackToClient(trackHubSkipHubName(parentTdb->track), tdb, user);
    first = FALSE;
    }
jsInlineF("];");
for(tdb = parentTdb->subtracks; tdb;  tdb = tdb->next)
    subTracksToClient(arrayName,tdb, user);
}

void addSubtrackNames(struct dyString *dy, struct trackDb *parentTdb)
{
if (parentTdb->subtracks == NULL)
    return;

struct trackDb *tdb;
for(tdb = parentTdb->subtracks; tdb;  tdb = tdb->next)
    {
    dyStringPrintf(dy, "collectionNames['%s']=1;", trackHubSkipHubName(tdb->track));
    addSubtrackNames(dy, tdb);
    }
}

static void doTable(struct cart *cart, char *db, struct grp *groupList, struct trackDb *trackList)
// output the tree table
{
char *hubName = hubNameFromUrl(getHubName(cart, db, FALSE));
struct grp *curGroup;
struct hash *groupHash = newHash(10);
int count = 0;

for(curGroup = groupList; curGroup;  curGroup = curGroup->next)
    {
    if (curGroup->priority == 0)
        curGroup->priority = count--;
    hashAdd(groupHash, curGroup->name, curGroup);
    }

curGroup = NULL;
if (hubName != NULL)
    curGroup = hashFindVal(groupHash, hubName);

jsInlineF("var collectionData = []; ");
struct dyString *dyNames = dyStringNew(1024);
struct dyString *dyLabels = dyStringNew(1024);
jsInlineF("var collectionNames = [];");
jsInlineF("var collectionLabels = [];");
if (curGroup != NULL)
    {
    // print out all the tracks in all the collections
    struct trackDb *tdb;
    jsInlineF("collectionData['#'] = [");
    boolean first = TRUE;
    for(tdb = trackList; tdb;  tdb = tdb->next)
        {
        if (sameString(tdb->grp, hubName))
            {
            if (!first)
                {
                jsInlineF(",");
                }
            trackToClient("#", tdb,  TRUE);
            dyStringPrintf(dyNames, "collectionNames['%s']=1;", trackHubSkipHubName(tdb->track));
            dyStringPrintf(dyLabels, "collectionLabels['%s']=1;", tdb->shortLabel);
            first = FALSE;
            }
        }
    jsInlineF("];");
    for(tdb = trackList; tdb;  tdb = tdb->next)
        {
        if ( sameString(tdb->grp, curGroup->name))
            {
            subTracksToClient("collectionData", tdb, TRUE);
            addSubtrackNames(dyNames, tdb);
            }
        }
    }
else
    jsInlineF("collectionData['#'] = [];");

jsInlineF("%s", dyNames->string);
jsInlineF("%s", dyLabels->string);

jsInlineF("var trackData = []; ");
struct dyString *rootChildren = dyStringNew(512);
addVisibleTracks(groupHash, rootChildren, cart, trackList);
for(curGroup = groupList; curGroup;  curGroup = curGroup->next)
    {
    if ((hubName != NULL) && sameString(curGroup->name, hubName))
        continue;
    if (!isEmpty(rootChildren->string))
        dyStringPrintf(rootChildren, ",");
    dyStringPrintf(rootChildren, "{icon:'../images/folderC.png',id:'%s', text:'%s', parent:'#', children:true,li_attr:{title:'%s'}}", curGroup->name, escapeLabel(curGroup->label), FOLDER_TITLE);
    struct trackDb *tdb;
    jsInlineF("trackData['%s'] = [", curGroup->name);
    boolean first = TRUE;
    for(tdb = trackList; tdb;  tdb = tdb->next)
        {
        if ( sameString(tdb->grp, curGroup->name))
            {
            if (!first)
                jsInlineF(",");
            trackToClient(curGroup->name, tdb, FALSE);
            first = FALSE;
            }
        }
    jsInlineF("];");
    for(tdb = trackList; tdb;  tdb = tdb->next)
        {
        if ( sameString(tdb->grp, curGroup->name))
            subTracksToClient("trackData", tdb, FALSE);
        }
    }
jsInlineF("trackData['#'] = [%s];", rootChildren->string);
jsInlineF("var collectionTitle='%s';\n", COLLECTION_TITLE);
jsInlineF("var folderTitle='%s';\n",  FOLDER_TITLE);
jsInlineF("var trackTitle='%s';\n", TRACK_TITLE);
jsInlineF("hgCollection.init();\n");
}

static void printHelp()
// print out the help page
{
puts(
"<br><a name='INFO_SECTION'></a>\n"
"    <div class='row gbsPage'>\n"
"        <div ><h1>Track Collection Builder Help</h1></div>\n"
"        <div >\n"
);
puts(
"       </div>\n"
"    </div>\n"
);
puts(
"    <div class='container-fluid'>\n"
"       <div class='gbsPage'>\n");

webIncludeFile("inc/hgCollectionHelpInclude.html");
puts(
"       </div>"
"    </div>\n"
);
}

static void doMainPage(struct cart *cart, char *db, struct grp *groupList, struct trackDb *trackList)
/* Print out initial HTML of control page. */
{
webStartGbNoBanner(cart, db, "Collections");
webIncludeResourceFile("gb.css");
//webIncludeResourceFile("../staticStyle/gbStatic.css");
webIncludeResourceFile("gbStatic.css");
webIncludeResourceFile("spectrum.min.css");
webIncludeResourceFile("hgGtexTrackSettings.css");

jsReloadOnBackButton(cart);

// Write the page HTML: the application, followed by its help doc
webIncludeFile("inc/hgCollection.html");

// output the form that will take us back to hgTracks
printf("<form id='redirectForm' action='../cgi-bin/hgTracks'><input type='hidden'  name='hgsid' value='%s'></form>",cartSessionId(cart));

char *assembly = stringBetween("(", ")", hFreezeFromDb(db));
if (assembly != NULL)
    jsInlineF("$('#assembly').text('%s');\n",assembly);
printHelp();

doTable(cart, db, groupList, trackList);

webIncludeResourceFile("jquery-ui.css");
webIncludeResourceFile("jstree-3.3.7.min.css");
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);
jsIncludeFile("jstree-3.3.7.min.js", NULL);
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("spectrum.min.js", NULL);
jsIncludeFile("hgCollection.js", NULL);
webEndGb();
}

static char *getSqlBigWig(struct sqlConnection *conn, char *db, struct trackDb *tdb)
// figure out the bigWig for native tables
{
char query[1024];
sqlSafef(query, sizeof query, "select fileName from %s", tdb->table);
return sqlQuickString(conn, query);
}

void dumpTdbAndParents(struct dyString *dy, struct trackDb *tdb, struct hash *existHash, struct hash *wantHash)
/* Put a trackDb entry into a dyString, stepping up the tree for some variables. */
{
struct hashCookie cookie = hashFirst(tdb->settingsHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (!hashLookup(existHash, hel->name) && ((wantHash == NULL) || hashLookup(wantHash, hel->name)))
        {
        dyStringPrintf(dy, "%s %s\n", hel->name, (char *)hel->val);
        hashStore(existHash, hel->name);
        }
    }

if (tdb->parent)
    {
    struct hash *newWantHash = newHash(4);
    hashStore(newWantHash, "type"); // right now we only want type from parents
    dumpTdbAndParents(dy, tdb->parent, existHash, newWantHash);
    }
}

static struct dyString *trackDbString(struct trackDb *tdb)
/* Convert a trackDb entry into a dyString. */
{
struct dyString *dy;
struct hash *existHash = newHash(5);
struct hashEl *hel;

hel = hashLookup(tdb->settingsHash, "track");
if (hel == NULL)
    errAbort("can't find track variable in tdb");

dy = dyStringNew(200);
dyStringPrintf(dy, "track %s\n", trackHubSkipHubName((char *)hel->val));
hashStore(existHash, "track");

dumpTdbAndParents(dy, tdb, existHash, NULL);

return dy;
}


static void printTdbToHub(char *db, struct sqlConnection *conn,  FILE *f,   struct trackDb *tdb, int numTabs, int priority)
// out the trackDb for one track
{
char *dataUrl = NULL;
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");

if (bigDataUrl == NULL)
    {
    if (startsWith("bigWig", tdb->type))
        {
        if (conn == NULL)
            errAbort("track hub has bigWig without bigDataUrl");
        dataUrl = getSqlBigWig(conn, db, tdb);
        hashReplace(tdb->settingsHash, "bigDataUrl", dataUrl);
        }
    }

char *tdbType = trackDbSetting(tdb, "tdbType");
if (tdbType != NULL)
    hashReplace(tdb->settingsHash, "type", tdbType);

// remove variables that will confuse us
if (hashLookup(tdb->settingsHash, "customized") == NULL)
    {
    hashRemove(tdb->settingsHash, "maxHeightPixels");
    hashRemove(tdb->settingsHash, "superTrack");
    hashRemove(tdb->settingsHash, "subGroups");
    hashRemove(tdb->settingsHash, "polished");
    hashRemove(tdb->settingsHash, "noInherit");
    hashRemove(tdb->settingsHash, "group");
    }

hashReplace(tdb->settingsHash, "customized", "on");

char priBuf[128];
safef(priBuf, sizeof priBuf, "%d", priority);
hashReplace(tdb->settingsHash, "priority", cloneString(priBuf));

struct hashEl *hel = hashLookup(tdb->settingsHash, "parent");
if (hel != NULL)
    hashReplace(tdb->settingsHash, "parent", trackHubSkipHubName((char *)hel->val));

struct dyString *dy = trackDbString(tdb);

fprintf(f, "%s\n",  dy->string);
}

static void saveTrackName(struct trackDb *tdb, char *hubName, struct hash  *collectionNameHash)
/* If this is a native track, we want to squirrel away the original track name. Also add it to the name hash. */
{
if (tdb->subtracks)
    {
    struct trackDb *subTdb;
    for (subTdb = tdb->subtracks; subTdb; subTdb = subTdb->next)
        {
        saveTrackName(subTdb, hubName, collectionNameHash);
        }
    return;
    }

if ((tdb->grp == NULL) || (hubName == NULL) || differentString(tdb->grp, hubName))
    {
    if (collectionNameHash)
        hashStore(collectionNameHash,  tdb->track);

    char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
    if (bigDataUrl == NULL)
        {
        char *table = trackDbSetting(tdb, "table");
        if (table == NULL)
            hashAdd(tdb->settingsHash, "table", tdb->track);
        }
    }
}

static void updateHub(struct cart *cart, char *db, struct track *collectionList, struct hash *nameHash)
// save our state to the track hub
{
char *filename = getHubName(cart, db, TRUE);
char *hubName = hubNameFromUrl(filename);

FILE *f = mustOpen(filename, "w");
chmod(filename, 0666);

struct hash *collectionNameHash = newHash(6);

outHubHeader(f, db);
struct track *collection;
struct sqlConnection *conn = NULL;
if (!trackHubDatabase(db))
    conn = hAllocConn(db);
int priority = 1;
for(collection = collectionList; collection; collection = collection->next)
    {
    if (collection->trackList == NULL)  // don't output composites without children
        continue;

    struct trackDb *tdb = createComposite(collection->name, collection->shortLabel, collection->longLabel, collection->color, priority++);
    struct dyString *dy = trackDbString(tdb);
    fprintf(f, "%s\n",  dy->string);

    struct track *track;
    for (track = collection->trackList; track; track = track->next)
        {
        tdb = hashMustFindVal(nameHash, track->name);
        saveTrackName(tdb, hubName, collectionNameHash);

        char colorString[64];
        safef(colorString, sizeof colorString, "%ld,%ld,%ld", (track->color >> 16) & 0xff,(track->color >> 8) & 0xff,track->color & 0xff);
        hashReplace(tdb->settingsHash, "color", colorString);

        hashReplace(tdb->settingsHash, "shortLabel", track->shortLabel);
        hashReplace(tdb->settingsHash, "longLabel", track->longLabel);
        hashReplace(tdb->settingsHash, "track", makeUniqueName(collectionNameHash, track->name));
        hashReplace(tdb->settingsHash, "parent", collection->name);

        printTdbToHub(db, conn, f, tdb, 1, priority++);
        }
    }
fclose(f);
hFreeConn(&conn);
}

static unsigned long hexStringToLong(char *str)
{
return strtol(&str[1], NULL, 16);
}

struct jsonParseData
{
struct track **collectionList;
struct hash *trackHash;
};

static void jsonObjStart(struct jsonElement *ele, char *name,
    boolean isLast, void *context)
{
struct jsonParseData *jpd = (struct jsonParseData *)context;
struct track **collectionList = jpd->collectionList;
struct hash *trackHash = jpd->trackHash;
struct track *track;

if ((name == NULL) && (ele->type == jsonObject))
    {
    struct hash *objHash = jsonObjectVal(ele, "name");

    struct jsonElement *parentEle = hashFindVal(objHash, "id");
    char *parentId = jsonStringEscape(parentEle->val.jeString);
    parentEle = hashFindVal(objHash, "parent");
    char *parentName = jsonStringEscape(parentEle->val.jeString);

    AllocVar(track);
    struct jsonElement *attEle = hashFindVal(objHash, "li_attr");
    if (attEle)
        {
        struct hash *attrHash = jsonObjectVal(attEle, "name");
        struct jsonElement *strEle = (struct jsonElement *)hashFindVal(attrHash, "name");
        if (strEle == NULL)
            return;
        track->name = jsonStringEscape(strEle->val.jeString);
        hashAdd(trackHash, parentId, track);

        strEle = (struct jsonElement *)hashMustFindVal(attrHash, "shortlabel");
        track->shortLabel = jsonStringEscape(strEle->val.jeString);
        strEle = (struct jsonElement *)hashMustFindVal(attrHash, "longlabel");
        track->longLabel = jsonStringEscape(strEle->val.jeString);
        strEle = (struct jsonElement *)hashMustFindVal(attrHash, "color");
        track->color = hexStringToLong(jsonStringEscape(strEle->val.jeString));
        }

    if (sameString(parentName, "#"))
        slAddHead(collectionList, track);
    else
        {
        struct track *parent = hashMustFindVal(trackHash, parentName);
        slAddTail(&parent->trackList, track);
        }
    }
}

static struct track *parseJsonElements( struct jsonElement *collectionElements)
// parse the JSON returned from the ap
{
struct track *collectionList = NULL;
struct hash *trackHash = hashNew(5);
struct jsonParseData jpd = {&collectionList, trackHash};
jsonElementRecurse(collectionElements, NULL, FALSE, jsonObjStart, NULL, &jpd);

slReverse(&collectionList);
return collectionList;
}

static void doAjax(struct cart *cart, char *db, char *jsonText, struct hash *nameHash)
// Save our state
{
cgiDecodeFull(jsonText, jsonText, strlen(jsonText));
struct jsonElement *collectionElements = jsonParse(jsonText);
struct track *collectionList = parseJsonElements(collectionElements);

if (slCount(collectionList))
    updateHub(cart, db, collectionList, nameHash);
}

static void buildNameHash(struct hash *nameHash, struct hash *labelHash, struct trackDb *list)
{
if (list == NULL)
    return;

struct trackDb *tdb = list;
for(tdb = list; tdb;  tdb = tdb->next)
    {
    hashAdd(nameHash, trackHubSkipHubName(tdb->track), tdb);
    if (labelHash)
        hashAdd(labelHash, tdb->shortLabel, tdb);
    buildNameHash(nameHash, NULL,  tdb->subtracks);
    }
}

static struct trackDb *traverseTree(struct trackDb *oldList, struct hash *groupHash)
// add acceptable tracks to our tree
{
struct trackDb *newList = NULL, *tdb, *tdbNext;

for(tdb = oldList;  tdb ; tdb = tdbNext)
    {
    tdbNext = tdb->next;
    if (tdb->subtracks)
        {
        tdb->subtracks = traverseTree(tdb->subtracks, groupHash);

        if (tdb->subtracks)
            {
            hashStore(groupHash, tdb->grp);
            slAddHead(&newList, tdb);
            }
        }
    else
        {
        if (trackCanBeAdded(tdb))
            {
            hashStore(groupHash, tdb->grp);
            slAddHead(&newList, tdb);
            }
        }
    }
slReverse(&newList);

return newList;
}

static void pruneTrackList(struct trackDb **fullTrackList, struct grp **fullGroupList)
// drop track types we don't grok yet
{
struct hash *groupHash = newHash(5);

*fullTrackList = traverseTree(*fullTrackList, groupHash);
struct grp *newGroupList = NULL, *grp, *nextGrp;

for (grp = *fullGroupList; grp; grp = nextGrp)
    {
    nextGrp = grp->next;

    if (hashLookup(groupHash, grp->name))
        slAddHead(&newGroupList, grp);
    }
slReverse(&newGroupList);
*fullGroupList = newGroupList;
}

static struct trackDb *addSupers(struct trackDb *trackList)
/* Insert supertracks into the hierarchy. */
{
struct trackDb *newList = NULL;
struct trackDb *tdb, *nextTdb;
struct hash *superHash = newHash(5);

for(tdb = trackList; tdb;  tdb = nextTdb)
    {
    nextTdb = tdb->next;

    if (tdb->parent)
        {
        // part of a super track
        if (hashLookup(superHash, tdb->parent->track) == NULL)
            {
            hashStore(superHash, tdb->parent->track);

            slAddHead(&newList, tdb->parent);
            }
        slAddTail(&tdb->parent->subtracks, tdb);
        }
    else
        slAddHead(&newList, tdb);
    }

slReverse(&newList);

return newList;
}

static void printTrackDbListToHub(char *db, struct sqlConnection *conn, FILE *f, char *hubName, struct trackDb *list, char *collectionName, struct trackDb *newTdb,  int numTabs, int priority)
/* Put a list of trackDb entries into a collection, adding a new track to the collection. */
{
if (list == NULL)
    return;

struct trackDb *tdb;
for(tdb = list; tdb; tdb = tdb->next)
    {
    if (tdb->grp != NULL)
        if ((hubName == NULL) || differentString(hubName, tdb->grp))
                continue;

    printTdbToHub(db, conn, f, tdb, numTabs, priority++);

    struct hashEl *hel = hashLookup(tdb->settingsHash, "track");
    if ((hel != NULL) && (hel->val != NULL) &&  sameString((char *)hel->val, collectionName))
        {
        if (newTdb->subtracks)
            {
            struct trackDb *subTdb;
            slReverse(&newTdb->subtracks);
            for(subTdb = newTdb->subtracks; subTdb; subTdb = subTdb->next)
                {
                printTdbToHub(db, conn, f, subTdb, numTabs + 1, priority++);
                }
            }
        else
            printTdbToHub(db, conn, f, newTdb, numTabs + 1, priority++);
        }

    printTrackDbListToHub(db, conn, f, hubName,  tdb->subtracks, collectionName, newTdb, numTabs + 1, priority);
    }
}

static void doAddTrack(struct cart *cart, char *db, struct trackDb *trackList,  char *trackName, char *collectionName, struct hash *nameHash)
/* Add a track to a collection in a hub. */
{
char *fileName = getHubName(cart, db, TRUE);
char *hubName = hubNameFromUrl(fileName);
FILE *f = fopen(fileName, "w");
struct trackDb *newTdb = hashMustFindVal(nameHash, trackHubSkipHubName(trackName));
if (newTdb->subtracks)
    {
    struct trackDb *subTdb;
    for(subTdb = newTdb->subtracks; subTdb; subTdb = subTdb->next)
        {
        hashReplace(subTdb->settingsHash, "track", makeUniqueName(nameHash, subTdb->track));
        hashReplace(subTdb->settingsHash, "parent", trackHubSkipHubName(collectionName));
        }
    }
else
    {
    hashReplace(newTdb->settingsHash, "track", makeUniqueName(nameHash, trackName));
    hashReplace(newTdb->settingsHash, "parent", trackHubSkipHubName(collectionName));
    }
char *tdbType = trackDbSetting(newTdb, "tdbType");
if (tdbType != NULL)
    {
    hashReplace(newTdb->settingsHash, "type", tdbType);
    hashReplace(newTdb->settingsHash, "shortLabel", trackDbSetting(newTdb, "name"));
    hashReplace(newTdb->settingsHash, "longLabel", trackDbSetting(newTdb, "description"));
    }


outHubHeader(f, db);
struct sqlConnection *conn = NULL;
if (!trackHubDatabase(db))
    conn = hAllocConn(db);
saveTrackName(newTdb, hubName, NULL);
printTrackDbListToHub(db, conn, f, hubName, trackList, collectionName, newTdb,  0, 0);

hFreeConn(&conn);
fclose(f);
}

static void doMiddle(struct cart *cart)
/* Set up globals and make web page */
{
char *userName = (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;

if (userName == NULL)
    errAbort("You must be logged in to edit collections. Visit our <A HREF=\"hgLogin?hgLogin.do.displayLoginPage=1\">login page.</A?");

char *db;
char *genome;
getDbAndGenome(cart, &db, &genome, oldVars);
initGenbankTableNames(db);
int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

struct trackDb *trackList;
struct grp *groupList;
cartTrackDbInit(cart, &trackList, &groupList, TRUE);
pruneTrackList(&trackList, &groupList);

struct trackDb *superList = addSupers(trackList);
struct hash *nameHash = newHash(5);
struct hash *labelHash = newHash(5);
buildNameHash(nameHash, labelHash, superList);

char *cmd = cartOptionalString(cart, "cmd");
if (cmd == NULL)
    {
    doMainPage(cart, db, groupList, superList);
    }
else if (sameString("addTrack", cmd))
    {
    char *trackName = cgiString("track");
    char *collectionName = cgiString("collection");
    doAddTrack(cart, db, superList, trackName, collectionName, nameHash);
    apiOut("{\"serverSays\": \"added %s to collection\"}", NULL);
    }
else if (sameString("newCollection", cmd))
    {
    char *trackName = cgiString("track");
    char *collectionName = makeUniqueName(nameHash, "coll");
    char *shortLabel = makeUniqueLabel(labelHash, "New Collection");
    char buffer[4096];
    safef(buffer, sizeof buffer, "%s description", shortLabel);
    char *longLabel = cloneString(buffer);

    struct trackDb *tdb = createComposite(collectionName, shortLabel, longLabel, 0, 0);
    slAddHead(&superList, tdb);

    doAddTrack(cart, db, superList, trackName, collectionName, nameHash);
    apiOut("{\"serverSays\": \"new %s to collection\"}", NULL);
    }
else if (sameString("saveCollection", cmd))
    {
    char *jsonIn = cgiUsualString("jsonp", NULL);
    doAjax(cart, db, jsonIn, nameHash);
    apiOut("{\"serverSays\": \"Collections saved successfully.\"}", NULL);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
cgiSpoof(&argc, argv);

boolean isCommandLine = (cgiOptionalString("cgiSpoof") != NULL);
if (!isCommandLine)
    htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
oldVars = hashNew(10);

cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);

if (! isCommandLine)
    cgiExitTime("hgCollection", enteredMainTime);
return 0;
}

