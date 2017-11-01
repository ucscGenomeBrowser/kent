/* hgCollection - hub builder */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
char *visibility;
unsigned long color;
char *viewFunc;        // The method by which calculated tracks should be calculated
char *missingMethod;   // How should missing data be treated in calculated tracks
};

struct trackDbRef 
{
struct trackDbRef *next;
struct trackDb *tdb;
int order;
};

static char *makeUnique(struct hash *nameHash, char *name)
// Make the name of this track unique.
{
char *skipHub = trackHubSkipHubName(name);
if (hashLookup(nameHash, skipHub) == NULL)
    {
    hashStore(nameHash, name);
    return skipHub;
    }

unsigned count = 0;
char buffer[4096];

for(;; count++)
    {
    safef(buffer, sizeof buffer, "%s%d", skipHub, count);
    if (hashLookup(nameHash, buffer) == NULL)
        {
        hashStore(nameHash, buffer);
        return cloneString(buffer);
        }
    }

return NULL;
}

static boolean trackCanBeAdded(struct trackDb *tdb)
// are we allowing this track into a custom composite
{
return  (tdb->subtracks == NULL) && !startsWith("wigMaf",tdb->type) &&  (startsWith("wig",tdb->type) || startsWith("bigWig",tdb->type) || startsWith("bedGraph",tdb->type)) ;
}

static void printGroup(char *parent, struct trackDb *tdb, boolean folder, boolean user)
// output list elements for a group
{
char *userString = "";
char *prefix = "";
char *viewFunc = NULL;
char *missingMethod = NULL;

if (user)
    {
    if (tdb->parent && tdb->subtracks) 
        {
        viewFunc = trackDbSetting(tdb, "viewFunc");
        missingMethod = trackDbSetting(tdb, "missingMethod");
        userString = "data-jstree='{\\\"icon\\\":\\\"../images/folderC.png\\\"}' viewType='view' class='folder'";
        }
    else if (tdb->subtracks)
        userString = "data-jstree='{\\\"icon\\\":\\\"../images/folderC.png\\\"}' viewType='track' class='folder'";
    else
        userString = "data-jstree='{\\\"icon\\\":\\\"fa fa-minus-square\\\"}' viewType='track'";
    }
else
    {
    if (tdb->parent && tdb->subtracks) 
        userString = "data-jstree='{\\\"icon\\\":\\\"../images/folderC.png\\\"}' class='nodrop' viewType='view'";
    else if (tdb->subtracks)
        userString = "data-jstree='{\\\"icon\\\":\\\"../images/folderC.png\\\"}' class='nodrop' viewType='track'";
    else
        userString = "data-jstree='{ \\\"icon\\\":\\\"fa fa-plus\\\"}' class='nodrop' viewType='track'";
    }
    
    
#define IMAKECOLOR_32(r,g,b) ( ((unsigned int)b<<0) | ((unsigned int)g << 8) | ((unsigned int)r << 16))

char buffer[1024];
char *viewFuncString = "";
if (viewFunc != NULL)
    {
    safef(buffer, sizeof buffer, "viewFunc='%s' ", viewFunc);
    viewFuncString = cloneString(buffer);
    }

char *missingString = "";
if (missingMethod != NULL)
    {
    safef(buffer, sizeof buffer, "missingMethod='%s' ", missingMethod);
    missingString = cloneString(buffer);
    }

jsInlineF("<li shortLabel='%s' longLabel='%s' color='#%06x' %s %s visibility='%s'  name='%s%s' %s>%s",  tdb->shortLabel, tdb->longLabel,IMAKECOLOR_32(tdb->colorR,tdb->colorG,tdb->colorB), viewFuncString, missingString, hStringFromTv(tdb->visibility), prefix,  trackHubSkipHubName(tdb->track),   userString,  tdb->shortLabel );
jsInlineF(" (%s)", tdb->longLabel);


if (tdb->subtracks)
    {
    struct trackDb *subTdb;

    jsInlineF("<ul>");
    for(subTdb = tdb->subtracks; subTdb; subTdb = subTdb->next)
        printGroup(trackHubSkipHubName(tdb->track), subTdb, user && (subTdb->subtracks != NULL), user);
    jsInlineF("</ul>");
    }
jsInlineF("</li>");
}

static void outHubHeader(FILE *f, char *db)
// output a track hub header
{
fprintf(f,"hub hub1\n\
shortLabel User Composite\n\
longLabel User Composite\n\
useOneFile on\n\
email genome-www@soe.ucsc.edu\n\n");
fprintf(f,"genome %s\n\n", db);  
}


static char *getHubName(struct cart *cart, char *db)
// get the name of the hub to use for user collections
{
struct tempName hubTn;
char buffer[4096];
safef(buffer, sizeof buffer, "%s-%s", customCompositeCartName, db);
char *hubName = cartOptionalString(cart, buffer);

if (hubName == NULL)
    {
    trashDirDateFile(&hubTn, "hgComposite", "hub", ".txt");
    hubName = cloneString(hubTn.forCgi);
    cartSetString(cart, buffer, hubName);
    FILE *f = mustOpen(hubName, "a");
    outHubHeader(f, db);
    fclose(f);
    cartSetString(cart, "hubUrl", hubName);
    cartSetString(cart, hgHubConnectRemakeTrackHub, hubName);
    }
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


static void checkForVisible(struct cart *cart, struct trackDbRef **list, struct trackDb *tdb)
/* Walk the trackDb hierarchy looking for visible leaf tracks. */
{
struct trackDb *subTdb;
char buffer[4096];

if (tdb->subtracks)
    {
    for(subTdb = tdb->subtracks; subTdb; subTdb = subTdb->next)
        checkForVisible(cart, list, subTdb);
    }
else
    {
    if (isParentVisible(cart, tdb) &&  isSubtrackVisible(cart, tdb))
        {
        struct trackDbRef *tdbRef;
        AllocVar(tdbRef);
        tdbRef->tdb = tdb;
        slAddHead(list, tdbRef);
        safef(buffer, sizeof buffer, "%s_imgOrd", tdb->track);

        tdbRef->order = cartUsualInt(cart, buffer, 0);
        }
    }
}

static int tdbRefCompare (const void *va, const void *vb)
// Compare to sort on imgTrack->order.
{
const struct trackDbRef *a = *((struct trackDbRef **)va);
const struct trackDbRef *b = *((struct trackDbRef **)vb);
return (a->order - b->order);
}       

static void addVisibleTracks(struct cart *cart, struct trackDb *trackList)
// add the visible tracks table rows.
{
struct trackDb *tdb;
struct trackDbRef *tdbRefList = NULL, *tdbRef;
//checkForVisible(fullTrackList);
for(tdb = trackList; tdb; tdb = tdb->next)
    {
    checkForVisible(cart, &tdbRefList, tdb);
    }

slSort(&tdbRefList, tdbRefCompare);

jsInlineF("<ul>");
jsInlineF("<li data-jstree='{\\\"icon\\\":\\\"../images/folderC.png\\\"}' class='nodrop' name='%s'>%s", "visibile", "Visible Tracks");
jsInlineF("<ul>");
for(tdbRef = tdbRefList; tdbRef; tdbRef = tdbRef->next)
    printGroup("visible", tdbRef->tdb, FALSE, FALSE);

jsInlineF("</ul>");
jsInlineF("</li>");
jsInlineF("</ul>");
}

static void doTable(struct cart *cart, char *db, struct grp *groupList, struct trackDb *trackList)
// output the tree table
{
char *hubName = hubNameFromUrl(getHubName(cart, db));
struct grp *curGroup;
for(curGroup = groupList; curGroup;  curGroup = curGroup->next)
    {
    if ((hubName != NULL) && sameString(curGroup->name, hubName))
        break;
    }
if (curGroup != NULL)
    {
    // print out all the tracks in all the collections
    struct trackDb *tdb;
    jsInlineF("$('#currentCollection').append(\"");
    for(tdb = trackList; tdb;  tdb = tdb->next)
        {
        if (sameString(tdb->grp, hubName))
            {
            jsInlineF("<div id='%s' shortLabel='%s'>", trackHubSkipHubName(tdb->track), tdb->shortLabel);
            jsInlineF("<ul>");
            printGroup("collections", tdb, TRUE, TRUE);
            jsInlineF("</ul>");
            jsInlineF("</div>");
            continue;
            }
        }
    jsInlineF("\");\n");
    
    // print out all the collections
    jsInlineF("$('#collectionList').append(\"");
    for(tdb = trackList; tdb;  tdb = tdb->next)
        {
        if (sameString(tdb->grp, hubName))
            {
            jsInlineF("<li data-jstree='{\\\"icon\\\":\\\"../images/folderC.png\\\"}' class='nodrop' id='%s'  name='%s'>%s</li>", trackHubSkipHubName(tdb->track),trackHubSkipHubName(tdb->track), tdb->shortLabel);
            //printGroup("collections", tdb, TRUE, TRUE);
            }
        }
    jsInlineF("\");\n");
    }
jsInlineF("$('#tracks').append(\"");
addVisibleTracks(cart, trackList);
for(curGroup = groupList; curGroup;  curGroup = curGroup->next)
    {
    if ((hubName != NULL) && sameString(curGroup->name, hubName))
        continue;
    jsInlineF("<ul>");
    jsInlineF("<li data-jstree='{\\\"icon\\\":\\\"../images/folderC.png\\\"}' class='nodrop' name='%s'>%s", curGroup->name, curGroup->label );
    struct trackDb *tdb;
    jsInlineF("<ul>");
    for(tdb = trackList; tdb;  tdb = tdb->next)
        {
        if ( sameString(tdb->grp, curGroup->name))
            {
            printGroup(curGroup->name, tdb, FALSE, FALSE);
            }
        }
    jsInlineF("</ul>");
    jsInlineF("</li>");
    jsInlineF("</ul>");

    }
jsInlineF("\");\n");
jsInlineF("hgCollection.init();\n");
}

static void onclickJumpToTop(char *id)
/* CSP-safe click handler arrows that cause scroll to top */
{
jsOnEventById("click", id, "$('html,body').scrollTop(0);");
}

static void printHelp()
// print out the help page
{
puts(
"<a name='INFO_SECTION'></a>\n"
"    <div class='row mygbSectionBanner'>\n"
"        <div class='col-md-11'>Help</div>\n"
"        <div class='col-md-1'>\n"
);
#define DATA_INFO_JUMP_ARROW_ID    "hgGtexDataInfo_jumpArrow"
printf(
"            <i id='%s' title='Jump to top of page' \n"
"               class='gbIconArrow fa fa-lg fa-arrow-circle-up'></i>\n",
DATA_INFO_JUMP_ARROW_ID
);
onclickJumpToTop(DATA_INFO_JUMP_ARROW_ID);
puts(
"       </div>\n"
"    </div>\n"
);
puts(
"    <div class='row gbTrackDescriptionPanel'>\n"
"       <div class='gbTrackDescription'>\n");
puts("<div class='dataInfo'>");
puts("</div>");
webIncludeHelpFileSubst("hgCollectionHelp", NULL, FALSE);

puts("<div class='dataInfo'>");
puts("</div>");

puts(
"     </div>\n"
"   </div>\n");
}

static void doMainPage(struct cart *cart, char *db, struct grp *groupList, struct trackDb *trackList)
/* Print out initial HTML of control page. */
{
webStartGbNoBanner(cart, db, "Collections");
webIncludeResourceFile("gb.css");
webIncludeResourceFile("spectrum.min.css");
webIncludeResourceFile("hgGtexTrackSettings.css");

webIncludeFile("inc/hgCollection.html");
char *assembly = stringBetween("(", ")", hFreezeFromDb(db));
jsInlineF("$('#assembly').text('%s');\n",assembly);

printHelp();
doTable(cart, db, groupList, trackList);

puts("<link rel='stylesheet' href='https://code.jquery.com/ui/1.10.3/themes/smoothness/jquery-ui.css'>");
puts("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/themes/default/style.min.css' />");
puts("<script src='https://cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js'></script>");
puts("<script src=\"//code.jquery.com/ui/1.10.3/jquery-ui.min.js\"></script>");
puts("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.4/jstree.min.js\"></script>\n");
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("spectrum.min.js", NULL);
jsIncludeFile("hgCollection.js", NULL);
webEndGb();
}

static char *getSqlBigWig(struct sqlConnection *conn, char *db, struct trackDb *tdb)
// figure out the bigWig for native tables
{
char buffer[4096];

safef(buffer, sizeof buffer, "NOSQLINJ select fileName from %s", tdb->table);
return sqlQuickString(conn, buffer);
}

static void outTdb(struct sqlConnection *conn, char *db, FILE *f, char *name,  struct trackDb *tdb, char *parent, char *visibility, unsigned int color, struct track *track, struct hash *nameHash, struct hash *collectionNameHash, int numTabs, int priority)
// out the trackDb for one track
{
char *dataUrl = NULL;
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
char *tabs = "\t";
if (numTabs == 2)
    tabs = "\t\t";

if (bigDataUrl == NULL)
    {
    if (startsWith("bigWig", tdb->type))
        dataUrl = getSqlBigWig(conn, db, tdb);
    }

char *tdbType = trackDbSetting(tdb, "tdbType");
if (tdbType != NULL)
    hashReplace(tdb->settingsHash, "type", tdbType);

struct hashCookie cookie = hashFirst(tdb->settingsHash);
struct hashEl *hel;
fprintf(f, "%strack %s\n",tabs, makeUnique(collectionNameHash, name));
fprintf(f, "%sshortLabel %s\n",tabs, track->shortLabel);
fprintf(f, "%slongLabel %s\n",tabs, track->longLabel);
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (differentString(hel->name, "parent") && differentString(hel->name, "polished")&& differentString(hel->name, "shortLabel")&& differentString(hel->name, "longLabel")&& differentString(hel->name, "color")&& differentString(hel->name, "visibility")&& differentString(hel->name, "track")&& differentString(hel->name, "trackNames")&& differentString(hel->name, "superTrack")&& differentString(hel->name, "priority")&& differentString(hel->name, "group"))
        fprintf(f, "%s%s %s\n", tabs,hel->name, (char *)hel->val);
    }
if (bigDataUrl == NULL)
    {
    if (dataUrl != NULL)
        fprintf(f, "%sbigDataUrl %s\n", tabs,dataUrl);
    }
fprintf(f, "%sparent %s\n",tabs,parent);
fprintf(f, "%scolor %d,%d,%d\n", tabs,(color >> 16) & 0xff,(color >> 8) & 0xff,color & 0xff);
fprintf(f, "%svisibility %s\n",tabs,visibility);
fprintf(f, "%spriority %d\n",tabs,priority);
fprintf(f, "\n");
}

static void outComposite(FILE *f, struct track *collection)
// output a composite header for user composite
{
char *parent = collection->name;
char *shortLabel = collection->shortLabel;
char *longLabel = collection->longLabel;
fprintf(f,"track %s\n\
shortLabel %s\n\
compositeTrack on\n\
autoScale on\n\
maxHeightPixels 100:30:11 \n\
aggregate  none\n\
longLabel %s\n\
%s on\n\
color %ld,%ld,%ld \n\
type wig \n\
visibility full\n\n", parent, shortLabel, longLabel, CUSTOM_COMPOSITE_SETTING,
 0xff& (collection->color >> 16),0xff& (collection->color >> 8),0xff& (collection->color));

}

static void modifyName(struct trackDb *tdb, char *hubName, struct hash  *collectionNameHash)
/* If this is a new track in the collection we want to make sure
 * it gets a different name than the track in trackDb.
 * If it's a native track, we want to squirrel away the original track name. */
{
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

static int outView(FILE *f, struct sqlConnection *conn, char *db, struct track *view, char *parent, struct hash *nameHash, struct hash *collectionNameHash, int priority, char *hubName)
// output a view to a trackhub
{
fprintf(f,"\ttrack %s\n\
\tshortLabel %s\n\
\tlongLabel %s\n\
\tview %s \n\
\tcontainer mathWig\n\
\tautoScale on  \n\
\tparent %s \n\
\tcolor %ld,%ld,%ld \n\
\tpriority %d\n\
\tviewFunc %s \n\
\tmissingMethod %s \n\
\tvisibility %s\n", view->name, view->shortLabel, view->longLabel, view->name, parent, 0xff& (view->color >> 16),0xff& (view->color >> 8),0xff& (view->color), priority++, view->viewFunc, view->missingMethod, view->visibility);
fprintf(f, "\n");

struct track *track = view->trackList;
for(; track; track = track->next)
    {
    struct trackDb *tdb = hashMustFindVal(nameHash, track->name);
    modifyName(tdb, hubName, collectionNameHash);

    outTdb(conn, db, f, track->name,tdb, view->name, track->visibility, track->color, track,  nameHash, collectionNameHash, 2, priority++);
    }

return priority;
}

static void updateHub(struct cart *cart, char *db, struct track *collectionList, struct hash *nameHash)
// save our state to the track hub
{
char *filename = getHubName(cart, db);
char *hubName = hubNameFromUrl(filename);

FILE *f = mustOpen(filename, "w");
chmod(filename, 0666);

struct hash *collectionNameHash = newHash(6);

outHubHeader(f, db);
struct track *collection;
struct sqlConnection *conn = hAllocConn(db);
for(collection = collectionList; collection; collection = collection->next)
    {
    outComposite(f, collection);
    struct trackDb *tdb;
    struct track *track;
    int priority = 1;
    for (track = collection->trackList; track; track = track->next)
        {
        if (track->viewFunc != NULL)
            {
            priority = outView(f, conn, db, track, collection->name,  nameHash, collectionNameHash, priority, hubName);
            }
        else
            {
            tdb = hashMustFindVal(nameHash, track->name);
            modifyName(tdb, hubName, collectionNameHash);

            outTdb(conn, db, f, track->name,tdb, collection->name, track->visibility, track->color, track,  nameHash, collectionNameHash, 1, priority++);
            }
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
    if (sameString(parentName, "#"))
        slAddHead(collectionList, track);
    else
        {
        struct track *parent = hashMustFindVal(trackHash, parentName);
        slAddTail(&parent->trackList, track);
        }

    struct jsonElement *attEle = hashFindVal(objHash, "li_attr");
    if (attEle)
        {
        struct hash *attrHash = jsonObjectVal(attEle, "name");
        struct jsonElement *strEle = (struct jsonElement *)hashMustFindVal(attrHash, "name");
        track->name = jsonStringEscape(strEle->val.jeString);
        hashAdd(trackHash, parentId, track);

        strEle = (struct jsonElement *)hashMustFindVal(attrHash, "shortlabel");
        track->shortLabel = jsonStringEscape(strEle->val.jeString);
        strEle = (struct jsonElement *)hashMustFindVal(attrHash, "longlabel");
        track->longLabel = jsonStringEscape(strEle->val.jeString);
        strEle = (struct jsonElement *)hashMustFindVal(attrHash, "visibility");
        track->visibility = jsonStringEscape(strEle->val.jeString);
        strEle = (struct jsonElement *)hashMustFindVal(attrHash, "color");
        track->color = hexStringToLong(jsonStringEscape(strEle->val.jeString));
        strEle = (struct jsonElement *)hashFindVal(attrHash, "viewfunc");
        if (strEle)
            track->viewFunc = jsonStringEscape(strEle->val.jeString);
        strEle = (struct jsonElement *)hashFindVal(attrHash, "missingMethod");
        if (strEle)
            track->missingMethod = jsonStringEscape(strEle->val.jeString);
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

updateHub(cart, db, collectionList, nameHash);
}

static void buildNameHash(struct hash *nameHash, struct trackDb *list)
{
if (list == NULL)
    return;

struct trackDb *tdb = list;
for(tdb = list; tdb;  tdb = tdb->next)
    {
    hashAdd(nameHash, trackHubSkipHubName(tdb->track), tdb);
    buildNameHash(nameHash, tdb->subtracks);
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
        slAddHead(&tdb->parent->subtracks, tdb);
        }
    else
        slAddHead(&newList, tdb);
    }

slReverse(&newList);

return newList;
}

static void outOneTdb(char *db, struct sqlConnection *conn, FILE *f, struct trackDb *tdb, int numTabs)
/* Put out a single trackDb entry to our collections hub. */
{
char *tabs = "";
if (numTabs == 1)
    tabs = "\t";
else if (numTabs == 2)
    tabs = "\t\t";
else if (numTabs == 3)
    tabs = "\t\t\t";

struct hashEl *hel = hashLookup(tdb->settingsHash, "track");
fprintf(f, "%s%s %s\n", tabs,hel->name, trackHubSkipHubName((char *)hel->val));

char *dataUrl = NULL;
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
if (bigDataUrl == NULL)
    {
    if (startsWith("bigWig", tdb->type))
        dataUrl = getSqlBigWig(conn, db, tdb);
    }

struct hashCookie cookie = hashFirst(tdb->settingsHash);
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (sameString("parent", hel->name))
        fprintf(f, "%s%s %s\n", tabs,hel->name, trackHubSkipHubName((char *)hel->val));
    else if (!(sameString("track", hel->name) || sameString("polished", hel->name) || sameString("group", hel->name)))
        fprintf(f, "%s%s %s\n", tabs,hel->name, (char *)hel->val);
    }

if (bigDataUrl == NULL)
    {
    if (dataUrl != NULL)
        fprintf(f, "%sbigDataUrl %s\n", tabs,dataUrl);
    }

fprintf(f, "\n");
}

static void outTrackDbList(char *db, struct sqlConnection *conn, FILE *f, char *hubName, struct trackDb *list, char *collectionName, struct trackDb *newTdb,  int numTabs)
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

    outOneTdb(db, conn, f, tdb, numTabs);

    struct hashEl *hel = hashLookup(tdb->settingsHash, "track");
    if ((hel != NULL) && (hel->val != NULL) &&  sameString((char *)hel->val, collectionName))
        outOneTdb(db, conn, f, newTdb, numTabs + 1);

    outTrackDbList(db, conn, f, hubName,  tdb->subtracks, collectionName, newTdb, numTabs + 1);
    }
}

static void doAddTrack(struct cart *cart, char *db, struct trackDb *trackList,  char *trackName, char *collectionName, struct hash *nameHash)
/* Add a track to a collection in a hub. */
{
char *fileName = getHubName(cart, db);
char *hubName = hubNameFromUrl(fileName);
FILE *f = fopen(fileName, "w");
struct trackDb *newTdb = hashMustFindVal(nameHash, trackHubSkipHubName(trackName));
hashReplace(newTdb->settingsHash, "track", makeUnique(nameHash, trackName));
hashReplace(newTdb->settingsHash, "parent", trackHubSkipHubName(collectionName));

outHubHeader(f, db);
struct sqlConnection *conn = hAllocConn(db);
modifyName(newTdb, hubName, NULL);
outTrackDbList(db, conn, f, hubName, trackList, collectionName, newTdb,  0);

hFreeConn(&conn);
fclose(f);
}

static void doMiddle(struct cart *cart)
/* Set up globals and make web page */
{
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
buildNameHash(nameHash, superList);

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
    char *collectionName = makeUnique(nameHash, "coll");
    char *shortLabel = "New Collection";
    char *longLabel = "Description of New Collection";
    struct trackDb *tdb;

    AllocVar(tdb);
    slAddHead(&superList, tdb);
    tdb->settingsHash = newHash(5);
    tdb->type = cloneString("wig");

    hashAdd(tdb->settingsHash, "track", collectionName);
    hashAdd(tdb->settingsHash, "shortLabel", shortLabel);
    hashAdd(tdb->settingsHash, "longLabel", longLabel);
    hashAdd(tdb->settingsHash, "autoScale", "on");
    hashAdd(tdb->settingsHash, "compositeTrack", "on");
    hashAdd(tdb->settingsHash, "aggregate", "none");
    hashAdd(tdb->settingsHash, "type", "wig");
    hashAdd(tdb->settingsHash, "visibility", "full");
    hashAdd(tdb->settingsHash, "color", "0,0,0");
    hashAdd(tdb->settingsHash, CUSTOM_COMPOSITE_SETTING, "on");

    doAddTrack(cart, db, superList, trackName, collectionName, nameHash);
    apiOut("{\"serverSays\": \"new %s to collection\"}", NULL);
    }
else if (sameString("saveCollection", cmd))
    {
    char *jsonIn = cgiUsualString("jsonp", NULL);
    doAjax(cart, db, jsonIn, nameHash);
    apiOut("{\"serverSays\": \"Collections gaved successfully.\"}", NULL);
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

