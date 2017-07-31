/* hgVai - Variant Annotation Integrator. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "cartTrackDb.h"
#include "genbank.h"
#include "hgConfig.h"
#include "hgHgvs.h"
#include "hui.h"
#include "grp.h"
#include "hCommon.h"
#include "hgFind.h"
#include "hPrint.h"
#include "jsHelper.h"
#include "memalloc.h"
#include "textOut.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "twoBit.h"
#include "gpFx.h"
#include "bigGenePred.h"
#include "udc.h"
#include "knetUdc.h"
#include "md5.h"
#include "regexHelper.h"
#include "hAnno.h"
#include "trashDir.h"
#include "api.h"
#include "customComposite.h"

//#include "libifyMe.h"


/* Global Variables */
struct cart *cart;		/* CGI and other variables */
struct hash *oldVars = NULL;	/* The cart before new cgi stuff added. */
char *genome = NULL;		/* Name of genome - mouse, human, etc. */
char *database = NULL;		/* Current genome database - hg17, mm5, etc. */
char *regionType = NULL;	/* genome, ENCODE pilot regions, or specific position range. */
struct grp *fullGroupList = NULL;	/* List of all groups. */
struct trackDb *fullTrackList = NULL;	/* List of all tracks in database. */
 struct pipeline *compressPipeline = (struct pipeline *)NULL;


// Null terminated list of CGI Variables we don't want to save permanently:
char *excludeVars[] = {"Submit", "submit", "hgva_startQuery", NULL,};

struct track
{
struct track *next;
struct track *trackList;
struct trackDb *tdb;
char *name;
char *shortLabel;
char *longLabel;
char *visibility;
};

static char *getString(char **input)
{
char *ptr = *input;

if (*ptr != '"')
    errAbort("string must start with \"");
ptr++;
char *ret = ptr;
for(; *ptr != '"'; ptr++)
    ;
*ptr = 0;
ptr++;

if (*ptr == ',')
    ptr++;

*input = ptr;

return ret;
}

char *makeUnique(struct hash *nameHash, char *name)
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
{
return  (tdb->subtracks == NULL) && !startsWith("wigMaf",tdb->type) &&  (startsWith("wig",tdb->type) || startsWith("bigWig",tdb->type)) ;
}

static void printGroup(char *parent, struct trackDb *tdb, boolean folder, boolean user)
{
printf("<tr data-tt-parent-id='%s' data-tt-id='%s' %s><td><span class='%s'>%s</span></td>",  parent, trackHubSkipHubName(tdb->track),   user ? "class='user'" : "", folder ? "folder" : "file", tdb->shortLabel );
printf("<td>%s</td></tr>\n", tdb->longLabel);


if (tdb->subtracks)
    {
    struct trackDb *subTdb;

    for(subTdb = tdb->subtracks; subTdb; subTdb = subTdb->next)
        printGroup(trackHubSkipHubName(tdb->track), subTdb, FALSE, user);
    }
}

static void outHubHeader(FILE *f, char *db, char *hubName)
{
char *hubFile = strrchr(hubName, '/') + 1;

fprintf(f,"hub hub1\n\
shortLabel User Composite\n\
longLabel User Composite\n\
genomesFile %s\n\
email braney@soe.ucsc.edu\n\
descriptionUrl hub.html\n\n", hubFile);
fprintf(f,"genome %s\n\
trackDb %s\n\n", db, hubFile);  
}


static char *getHubName(char *db)
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
    outHubHeader(f, db, hubName);
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

bool isSubtrackVisible(struct trackDb *tdb)
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


bool isParentVisible( struct trackDb *tdb)
// Are this track's parents visible?
{
if (tdb->parent == NULL)
    return TRUE;

if (!isParentVisible(tdb->parent))
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


void addVisibleTracks()
{
printf("<tr data-tt-id='visible' ><td><span class='file'>All Visible</td><td>All the tracks visible in hgTracks</td></tr>\n");
struct trackDb *tdb;
for(tdb = fullTrackList; tdb; tdb = tdb->next)
    {
    if (isParentVisible(tdb) &&  isSubtrackVisible(tdb))
        {
        printGroup("visible", tdb, FALSE, FALSE);
        }
    }
}

static void doHeader()
{
puts(  
"<a name='TRACK_TOP'></a>\n"  
"    <div class='row gbTrackTitleBanner'>\n"  
"       <div class='col-md-10'>\n"  
);  
printf(  
"           <span class='gbTrackName'>\n"  
"               My Collections \n"  
"           </span>"  
"           <span class='gbTrackTitle'> Build Custom Collections of Tracks </span>\n"  );
puts(  
"<!-- Info icon built from stacked fa icons -->\n"  
"           <a href='#INFO_SECTION' title='Jump to the track description'>\n"  
"               <span class='gbIconSmall fa-stack'>\n"  
"                   <i class='gbBlueDarkColor fa fa-circle fa-stack-2x'></i>\n" 
"                   <i class='gbWhiteColor fa fa-info fa-stack-1x'></i>\n"  
"               </span></a>\n"  
"           <div class='gbButtonGoContainer text-right' title='Save Collections'>\n"
"           </div>\n"

"       </div>\n"  
"       <div class='col-md-2 text-right'>\n"  
"           <div class='gbButtonGoContainer text-right' title='Go to the Genome Browser'>\n"

"               <div class='gbButtonGo' id='saveCollections' >Save</div>\n"
"               <div class='gbButtonGo' id='discardChanges'>Discard Changes</div>\n"
"       </div>\n"  );
puts(  
"       </div>\n"  
"   </div>\n"  
);  
}

static void doTable()
{
puts(
"        <!-- Configuration panel -->\n"
"        <div class='row gbSectionBanner'>\n"
"            <div class='col-md-8'>All Tracks</div>\n"
"            <div class='col-md-4 text-right'>\n");

puts(
"           <div class='gbButtonGoContainer text-right' title='Make New Collection'>\n"
"               <div id='newCollection' class='gbButton'>Make New Collection </div>\n"
"           </div>\n"
);
puts(
"            </div>\n"
"        </div>\n");

char *hubName = hubNameFromUrl(getHubName(database));
printf("<table id='tracks'><tbody>\n");
struct grp *curGroup;
printf("<tr data-tt-id='collections'><td><span class='file'>My Collections</td>");
printf("<td>Your collected tracks.</td></tr>\n");
for(curGroup = fullGroupList; curGroup;  curGroup = curGroup->next)
    {
    if ((hubName != NULL) && sameString(curGroup->name, hubName))
        break;
    }
if (curGroup != NULL)
    {
    // print out all the tracks in this group
    struct trackDb *tdb;
    for(tdb = fullTrackList; tdb;  tdb = tdb->next)
        {
        if (sameString(tdb->grp, hubName))
            printGroup("collections", tdb, TRUE, TRUE);
        }
    }
//addVisibleTracks();
for(curGroup = fullGroupList; curGroup;  curGroup = curGroup->next)
    {
    if ((hubName != NULL) && sameString(curGroup->name, hubName))
        continue;
    printf("<tr data-tt-id='%s'><td><span class='file'>%s</span></td><td></td></tr>\n", curGroup->name, curGroup->label );
    struct trackDb *tdb;
    for(tdb = fullTrackList; tdb;  tdb = tdb->next)
        {
        if ( sameString(tdb->grp, curGroup->name))
            {
            printGroup(curGroup->name, tdb, FALSE, FALSE);
            }
        }
    }
printf("</tbody></table>\n");
}

static void doAttributes()
{
puts(
"        <!-- Configuration panel -->\n"
"        <div class='row gbSectionBanner'>\n"
"            <div class='col-md-8'>Set Attributes</div>\n"
"            <div class='col-md-4 text-right'>\n");
puts(
"           <div class='gbButtonGoContainer text-right' title='Save Props'>\n"
"               <div id='propsSave' class='gbButton'>Save</div>\n"
"           </div>\n"
);
puts(
"           <div class='gbButtonGoContainer text-right' title='Discard Changes'>\n"
"               <div id='propsDiscard' class='gbButton'>Discard Changes</div>\n"
"           </div>\n"
);
puts(
"</div></div>\n");
// trackDb track options (none at the moment)
puts(
"<div id='TrackDbOptions' style=\"display: none;\">"
"Drag to custom composite to copy to Collections"
"</div>"
);

// mathwig options
puts(
"<div class='jwInputLabel'  id='MathWigOptions' style=\"display: none;\">"
"<label for='name'>Name:</label>"
"<input type='text' name='mathWigName' id='mathWigName' value='' class='text ui-widget-content ui-corner-all'>\n"
"<label for='description'>Description:</label>\n"
"<input type='text' name='mathWigDescription' id='mathWigDescription' value='' class='text ui-widget-content ui-corner-all'>\n"
"<label for='mathWigVis'>Visibility:</label>"
"<SELECT ID='mathWigVis' >\n"
"<OPTION>hide</OPTION>\n"
"<OPTION SELECTED>dense</OPTION>\n"
"<OPTION>squish</OPTION>\n"
"<OPTION>pack</OPTION>\n"
"<OPTION>full</OPTION>\n"
"</SELECT>\n"
"<label for='mathWigFunction'>Function:</label>"
"<SELECT ID='mathWigFunction' >\n"
"<OPTION SELECTED>add</OPTION>\n"
"<OPTION >subtract</OPTION>\n"
"</SELECT>\n"
" <p>Highlight color: <input type='text' id='mathColorInput' value='0xffffff'>&nbsp;&nbsp;<input id='mathColorPicker'>"
"</div>\n"
);

// custom composite options
puts(
"<div class='jwInputLabel'  id='CustomCompositeOptions' style=\"display: none;\">"
"<label for='name'>Name:</label>"
"<input type='text' name='collectionName' id='collectionName' value='' class='text ui-widget-content ui-corner-all'>\n"
"<label for='description'>Description:</label>\n"
"<input type='text' name='collectionDescription' id='collectionDescription' value='' class='text ui-widget-content ui-corner-all'>\n"
"<label for='collectionVis'>Visibility:</label>"
"<SELECT ID='collectionVis' style='width: 70px'>\n"
"<OPTION>hide</OPTION>\n"
"<OPTION SELECTED>dense</OPTION>\n"
"<OPTION>squish</OPTION>\n"
"<OPTION>pack</OPTION>\n"
"<OPTION>full</OPTION>\n"
"</SELECT>\n"
"<input type='button' value='Create MathWig' name='createMathWig' id='createMathWig'>\n"
"</div>\n"
);

// custom track  options
puts(
"<div class='jwInputLabel'  id='CustomTrackOptions' style=\"display: none;\">"
"<label for='name'>Name:</label>"
"<input type='text' name='customName' id='customName' value='' class='text ui-widget-content ui-corner-all'>\n"
"<label for='description'>Description:</label>\n"
"<input type='text' name='customDescription' id='customDescription' value='' class='text ui-widget-content ui-corner-all'>\n"
"<label for='customVis'>Visibility:</label>"
"<SELECT ID='customVis' style='width: 70px'>\n"
"<OPTION>hide</OPTION>\n"
"<OPTION SELECTED>dense</OPTION>\n"
"<OPTION>squish</OPTION>\n"
"<OPTION>pack</OPTION>\n"
"<OPTION>full</OPTION>\n"
"</SELECT>\n"
" <p>Highlight color: <input type='text' id='trackColorInput' value='0xffffff'>&nbsp;&nbsp;<input id='trackColorPicker'>"
"</div>\n"

);

}

static void onclickJumpToTop(char *id)
/* CSP-safe click handler arrows that cause scroll to top */
{
jsOnEventById("click", id, "$('html,body').scrollTop(0);");
}

static void printHelp()
{
puts(
"<a name='INFO_SECTION'></a>\n"
"    <div class='row gbSectionBanner'>\n"
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
webIncludeHelpFileSubst("hgCompositeHelp", NULL, FALSE);

puts("<div class='dataInfo'>");
puts("</div>");

puts(
"     </div>\n"
"   </div>\n");


puts("<script src=\"//code.jquery.com/jquery-1.9.1.min.js\"></script>");
puts("<script src=\"//code.jquery.com/ui/1.10.3/jquery-ui.min.js\"></script>");
jsIncludeFile("jquery.treetable.js", NULL);
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("hgTracks.js", NULL);
jsIncludeFile("spectrum.min.js", NULL);
jsIncludeFile("hgCollection.js", NULL);
}

void doMainPage()
/* Print out initial HTML of control page. */
{
webStartGbNoBanner(cart, database, "Collections");
webIncludeResourceFile("jquery.treetable.css");
webIncludeResourceFile("jquery.treetable.theme.default.css");
webIncludeResourceFile("gb.css");
webIncludeResourceFile("jWest.css");
webIncludeResourceFile("spectrum.min.css");
webIncludeResourceFile("hgGtexTrackSettings.css");

//webIncludeFile("inc/hgCollection.html");

printf(
"<form action='%s' name='MAIN_FORM' method=%s>\n\n",
                hgTracksName(), cartUsualString(cart, "formMethod", "POST"));

doHeader();
puts(
"<!-- Track Configuration Panels -->\n"
"    <div class='row'>\n"
"        <div class='col-md-6'>\n");
doTable();
puts(
"        </div>\n"
"        <div class='col-md-6'>\n");
doAttributes();
puts(
"        </div>\n"
"    </div>\n"
);
puts(
"</form>");
printHelp();


}

static char *getSqlBigWig(struct sqlConnection *conn, char *db, struct trackDb *tdb)
{
char buffer[4096];

safef(buffer, sizeof buffer, "NOSQLINJ select fileName from %s", tdb->table);
return sqlQuickString(conn, buffer);
}

char *getUrl(struct sqlConnection *conn, char *db,  struct track *track, struct hash *nameHash)
{
struct trackDb *tdb = hashMustFindVal(nameHash, track->name);

if (tdb == NULL)
    errAbort("cannot find trackDb for %s\n", track->name);

char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
if (bigDataUrl == NULL)
    {
    if (startsWith("bigWig", tdb->type))
        bigDataUrl = getSqlBigWig(conn, db, tdb);
    }
return bigDataUrl;
}

void outTdb(struct sqlConnection *conn, char *db, FILE *f, char *name,  struct trackDb *tdb, char *parent, unsigned int color, struct track *track, struct hash *nameHash, struct hash *collectionNameHash)
{
char *dataUrl = NULL;
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");

if (bigDataUrl == NULL)
    {
    if (startsWith("bigWig", tdb->type))
        dataUrl = getSqlBigWig(conn, db, tdb);
    }
struct hashCookie cookie = hashFirst(tdb->settingsHash);
struct hashEl *hel;
fprintf(f, "\ttrack %s\n", makeUnique(collectionNameHash, name));
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (sameString(hel->name, "mathDataUrl"))
        {
/*
        fprintf(f, "\ttrackNames ");
        struct mathTrack *mt = (struct mathTrack *)track;
        struct track *tr = mt->trackList;
        for(;  tr; tr = tr->next)
            {
            fprintf(f, "%s ", tr->name);
            }
        fprintf(f, "\n");

        fprintf(f, "\tmathDataUrl ");
        tr = mt->trackList;
        if ((mt->function == NULL) || sameString(mt->function, "add"))
            fprintf(f, "+  ");
        else
            fprintf(f, "-  ");
        for(;  tr; tr = tr->next)
            {
            fprintf(f, "%s ", getUrl(conn, db, wigTracks, tr, nameHash));
            }
        fprintf(f, "\n");
*/
        }
    else if (differentString(hel->name, "parent") && differentString(hel->name, "polished")&& differentString(hel->name, "color")&& differentString(hel->name, "track")&& differentString(hel->name, "trackNames")&& differentString(hel->name, "superTrack"))
        fprintf(f, "\t%s %s\n", hel->name, (char *)hel->val);
    }
if (bigDataUrl == NULL)
    {
    if (dataUrl != NULL)
        fprintf(f, "\tbigDataUrl %s\n", dataUrl);
    }
fprintf(f, "\tparent %s\n",parent);
fprintf(f, "\tcolor %d,%d,%d\n", (color >> 16) & 0xff,(color >> 8) & 0xff,color & 0xff);
fprintf(f, "\n");
}

static void outComposite(FILE *f, struct track *collection)
{
char *parent = collection->name;
char *shortLabel = collection->shortLabel;
char *longLabel = collection->longLabel;
fprintf(f,"track %s\n\
shortLabel %s\n\
compositeTrack on\n\
aggregate none\n\
longLabel %s\n\
%s on\n\
type wig \n\
visibility full\n\n", parent, &shortLabel[2], longLabel, CUSTOM_COMPOSITE_SETTING);
}

static int snakePalette2[] =
{
0x1f77b4, 0xaec7e8, 0xff7f0e, 0xffbb78, 0x2ca02c, 0x98df8a, 0xd62728, 0xff9896, 0x9467bd, 0xc5b0d5, 0x8c564b, 0xc49c94, 0xe377c2, 0xf7b6d2, 0x7f7f7f, 0xc7c7c7, 0xbcbd22, 0xdbdb8d, 0x17becf, 0x9edae5
};


static void outMathWig(FILE *f, struct sqlConnection *conn, char *db, struct track *mathWig, struct hash *nameHash)
{
fprintf(f,"\ttrack %s\n\
\tshortLabel %s\n\
\tlongLabel %s\n\
\ttype mathWig \n\
\tvisibility full\n", mathWig->name, &mathWig->shortLabel[2], mathWig->longLabel);
fprintf(f,"\tmathDataUrl + ");
struct track *track = mathWig->trackList;
for(; track; track = track->next)
    {
    fprintf(f, "%s ",  getUrl(conn, db,  track, nameHash));
    }

fprintf(f, "\n");

}

void updateHub(char *db, struct track *collectionList, struct hash *nameHash)
{
char *hubName = getHubName(db);

chmod(hubName, 0666);
FILE *f = mustOpen(hubName, "w");
struct hash *collectionNameHash = newHash(6);

outHubHeader(f, db, hubName);
int useColor = 0;
struct track *collection;
struct sqlConnection *conn = hAllocConn(db);
for(collection = collectionList; collection; collection = collection->next)
    {
    outComposite(f, collection);
    struct trackDb *tdb;
    struct track *track;
    for (track = collection->trackList; track; track = track->next)
        {
        if (track->trackList != NULL)
            {
            outMathWig(f, conn, db, track, nameHash);
            }
        else
            {
            tdb = hashMustFindVal(nameHash, track->name);

            outTdb(conn, db, f, track->name,tdb, collection->name, snakePalette2[useColor], track,  nameHash, collectionNameHash);
            useColor++;
            if (useColor == (sizeof snakePalette2 / sizeof(int)))
                useColor = 0;
            }
        }
    }
fclose(f);
hFreeConn(&conn);
}

static struct track *parseJson(char *jsonText)
{
struct hash *trackHash = newHash(5);
struct track *collectionList = NULL;
struct track *track;
char *ptr = jsonText;
if (*ptr != '[')
    errAbort("element didn't start with [");
ptr++;

do
    {
    if (*ptr != '[')
        errAbort("element didn't start with [");
    ptr++;

    AllocVar(track);
    char *parentName = getString(&ptr);
    if (sameString(parentName, "collections"))
        slAddHead(&collectionList, track);
    else
        {
        struct track *parent = hashMustFindVal(trackHash, parentName);
        slAddHead(&parent->trackList, track);
        }

    track->shortLabel = getString(&ptr);
    track->longLabel = getString(&ptr);
    track->name = getString(&ptr);
    track->visibility = getString(&ptr);
    hashAdd(trackHash, track->name, track);
    if (*ptr != ']')
        errAbort("element didn't end with ]");
    ptr++;
    if (*ptr == ',')
        ptr++;
    } while (*ptr != ']');

return collectionList;
}

void doAjax(char *db, char *jsonText, struct hash *nameHash)
{
struct track *collectionList = parseJson(jsonText);

updateHub(db, collectionList, nameHash);
}

static struct hash *buildNameHash(struct trackDb *list)
// TODO;  needs to go down one more layer
{
struct hash *nameHash = newHash(8);
struct trackDb *tdb;
for(tdb = list; tdb;  tdb = tdb->next)
    {
    hashAdd(nameHash, trackHubSkipHubName(tdb->track), tdb);
    struct trackDb *subTdb = tdb->subtracks;
    for(; subTdb; subTdb = subTdb->next)
        {
        hashAdd(nameHash, trackHubSkipHubName(subTdb->track), subTdb);
        }
    }
return nameHash;
}

static struct trackDb *traverseTree(struct trackDb *oldList, struct hash *groupHash)
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

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
getDbAndGenome(cart, &database, &genome, oldVars);
int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();
cartTrackDbInit(cart, &fullTrackList, &fullGroupList, TRUE);
pruneTrackList(&fullTrackList, &fullGroupList);
struct hash *nameHash = buildNameHash(fullTrackList);

char *jsonIn = cgiUsualString("jsonp", NULL);
fprintf(stderr, "BRANEY %s\n", jsonIn);
if (jsonIn != NULL)
    {
    doAjax(database, jsonIn, nameHash);
    apiOut("{\"serverSays\": \"bit me\"}", NULL);
    }
else
    {
    doMainPage();
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();

initGenbankTableNames(database);

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
