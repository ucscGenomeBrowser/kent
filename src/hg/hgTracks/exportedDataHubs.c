
// Code to parse list of exported data track hubs from table and print as browser dialog
//      for client dialog (js)
//
/* Copyright (C) 2024 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "dystring.h"
#include "hCommon.h"
#include "hgConfig.h"
#include "htmshell.h"
#include "hash.h"
#include "web.h"
#include "ra.h"
#include "hgTracks.h"
#include "hgFind.h"
#include "obscure.h"
#include "net.h"
#include "hubConnect.h"
#include "trackHub.h"
#include "exportedDataHubs.h"
#include "grp.h"
#include "cartTrackDb.h"

#define COLLECTION_TITLE  "Double-click to edit name and color"
#define FOLDER_TITLE      "Click to open node"
#define TRACK_TITLE       "Press Green Plus to add track to collection"

static struct slName *getFromDbs(char *toDb, struct sqlConnection *conn)
/* See if there are any assemblies with quickLiftChains to our assembly */
{
struct slName *fromDbs = NULL;
char **row;
struct dyString *query = dyStringNew(0);
sqlDyStringPrintf(query, "select distinct(fromDb) from quickLiftChain where toDb='%s'", trackHubSkipHubName(toDb));
struct sqlResult *sr = sqlGetResult(conn, dyStringCannibalize(&query));
while ( (row = sqlNextRow(sr)) != NULL)
    {
    struct slName *new = newSlName(cloneString(row[0]));
    slAddHead(&fromDbs, new);
    }

// this is probably not necessary but in case we want to rank assemblies later:
slReverse(&fromDbs);
return fromDbs;
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

struct trackDbRef 
    {
    struct trackDbRef *next;
    struct trackDb *tdb;
    struct grp *grp;
    double priority;
    int order;
    };

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

static bool isSubtrackVisibleCart(struct cart *cart, struct trackDb *tdb)
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


static bool isParentVisibleCart(struct cart *cart, struct trackDb *tdb)
// Are this track's parents visible?
{
if (tdb->parent == NULL)
    return TRUE;

if (!isParentVisibleCart(cart, tdb->parent))
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
    else if (isParentVisibleCart(cart, tdb) &&  isSubtrackVisibleCart(cart, tdb))
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

static void addVisibleTracks(char *db, struct hash *groupHash, struct dyString *rootChildren, struct cart *cart, struct trackDb *trackList)
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

jsInlineF("trackList['%s']['visible'] = [", db);
for(tdbRef = tdbRefList; tdbRef; tdbRef = tdbRef->next)
    {
    trackToClient("visible", tdbRef->tdb,  FALSE);
    if (tdbRef->next != NULL)
        jsInlineF(",");
    }
jsInlineF("];");
}


void subTracksToClient(char *db, char *arrayName, struct trackDb *parentTdb, boolean user)
{
if (parentTdb->subtracks == NULL)
    return;
jsInlineF("%s['%s']['%s'] = [", arrayName, db, trackHubSkipHubName(parentTdb->track));
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
    subTracksToClient(db, arrayName,tdb, user);
}

static void doTable(struct cart *cart, char *db, struct grp *groupList, struct trackDb *trackList)
{
struct grp *curGroup = NULL;
struct hash *groupHash = newHash(10);
int count = 0;

for(curGroup = groupList; curGroup;  curGroup = curGroup->next)
    {
    if (curGroup->priority == 0)
        curGroup->priority = count--;
    hashAdd(groupHash, curGroup->name, curGroup);
    }

jsInlineF("var trackList = {'%s': {}};\n", db);
struct dyString *rootChildren = dyStringNew(512);
addVisibleTracks(db, groupHash, rootChildren, cart, trackList);
for(curGroup = groupList; curGroup;  curGroup = curGroup->next)
    {
    if (!isEmpty(rootChildren->string))
        dyStringPrintf(rootChildren, ",");
    dyStringPrintf(rootChildren, "{icon:'../images/folderC.png',id:'%s', text:'%s', parent:'#', children:true,li_attr:{title:'%s'}}", curGroup->name, escapeLabel(curGroup->label), FOLDER_TITLE);
    struct trackDb *tdb;
    jsInlineF("trackList['%s']['%s'] = [", db, curGroup->name);
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
    jsInlineF("];\n");
    for(tdb = trackList; tdb;  tdb = tdb->next)
        {
        if ( sameString(tdb->grp, curGroup->name))
            subTracksToClient(db, "trackList", tdb, FALSE);
        }
    }
jsInlineF("trackList['%s']['#'] = [%s];\n", db, rootChildren->string);
}

void writeTrackSelections(char *toDb, struct slName *fromDbs, struct sqlConnection *conn)
/* put list of tracks into a checkbox list for selection */
{
struct slName *fromDb;
for (fromDb = fromDbs; fromDb != NULL; fromDb = fromDb->next)
    {
    // initialize list of tracks for the from assembly, note that this includes connected hubs
    struct trackDb *tdbList = NULL;
    struct grp *grpList = NULL;
    cartTrackDbInitForApi(cart, fromDb->name, &tdbList, &grpList, TRUE);
    doTable(cart, fromDb->name, grpList, tdbList);
    }
}

// write up form to select a db
void writeFromDbSelection(char *toDb, struct slName *fromDbs, struct sqlConnection *conn)
/* Look up the chains to our current db (toDb) and write them in a dropdown
 * so users can pick tracks to lift out of the fromDb. There will probably
 * not be many of these options (probably on from hg38/hg19) */
{
struct slName *fromDb;
boolean doneOne = FALSE;
hPrintf("<div>Please pick an assembly to port tracks FROM</div>");
for (fromDb = fromDbs; fromDb != NULL; fromDb = fromDb->next)
    {
    if (!doneOne)
        {
        doneOne = TRUE;
        hPrintf("<select name='fromDb' id='quickLiftFromDb'>");
        hPrintf("<option value='%s' selected>%s</option>", fromDb->name, fromDb->name);
        }
    else
        {
        hPrintf("<option value='%s'>%s</option>", fromDb->name, fromDb->name);
        }
    }
if (!doneOne)
    hPrintf("No quickLift chains available to this assembly. Please email us to request one");
else
    hPrintf("</select>");
}

void printExportedDataHubs(char *db)
/* Fill out exported data hubs popup. */
{
if (!exportedDataHubsEnabled())
    return;

puts("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/themes/default/style.min.css' />");
puts("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.7/jstree.min.js\"></script>\n");
jsIncludeFile("quickLift.js", NULL);
hPrintf("<div style='display:none;' id='exportedDataHubsPopup' title='Add Tracks From Other Genomes'>\n");
struct sqlConnection *conn = hConnectCentral();
/*
char **row;
char query[2048];
sqlSafef(query, sizeof(query), "select x.id,q.id,x.db,x.label,x.description,x.path,q.path from quickLiftChain q,exportedDataHubs x where q.fromDb=x.db and q.toDb='%s' and x.label not like 'Private';", trackHubSkipHubName(db));

hPrintf("<table style=\"border: 1px solid black\">\n");
struct sqlResult *sr;
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<tr><td>%s</td><td><A HREF=\"./hgTracks?quickLift.%s.%s=%s&%s=on&hgsid=%s\">%s</A></td><td>%s</td></tr>",row[2], row[0],trackHubSkipHubName(db), row[1], hgHubConnectRemakeTrackHub,cartSessionId(cart),  row[3],row[4]);
    }
hPrintf("</table>\n");
hPrintf("</div>\n");
*/
struct slName *fromDbs = getFromDbs(db, conn);
if (fromDbs != NULL)
    {
    writeFromDbSelection(db, fromDbs, conn);
    writeTrackSelections(db, fromDbs, conn);
    }
hDisconnectCentral(&conn);
}
