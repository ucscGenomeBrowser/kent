/* hgGtexTrackSettings: Configure GTEx track, with tissues selected from  Body Map or list
 *
 * Copyright (C) 2016 The Regents of the University of California
 */

#include "common.h"
#include "trackDb.h"
#include "cart.h"
#include "portable.h"
#include "cheapcgi.h"
#include "web.h"
#include "hCommon.h"
#include "hui.h"
#include "jsHelper.h"
#include "gtexUi.h"
#include "gtexInfo.h"
#include "gtexTissue.h"

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */
struct hash *oldVars = NULL;          /* Old contents of cart before it was updated by CGI */
char *database = NULL;

static void printGoButton()
/* HTML for GO button and 'play' icon */
{
puts(
"           <div class='gbButtonGoContainer text-right' title='Go to the Genome Browser'>\n"
"               <div class='gbButtonGo'>GO</div>\n"
"               <i class='gbIconGo fa fa-play fa-2x'></i>\n"
"           </div>\n"
);
}

static void printBodyMap()
/* Include BodyMap SVG in HTML */
{
puts(
"        <!-- Body Map panel -->\n"
"           <object id='bodyMapSvg' type='image/svg+xml' data='/images/gtexBodyMap.svg'>\n"
"               GTEx Body Map illustration not found\n"
"           </object>\n");
puts("<div class='gbmCredit'>Credit: jwestdesign</div>\n");
}

static void printTrackHeader(char *db, struct trackDb *tdb)
/* Print top banner with track labels */
{
char *assembly = stringBetween("(", ")", hFreezeFromDb(db));
puts(
"<a name='TRACK_TOP'></a>\n"
"    <div class='row gbTrackTitleBanner'>\n"
"       <div class='col-md-10'>\n"
);
printf(
"           <span class='gbTrackName'>\n"
"               %s Track\n"
"               <span class='gbAssembly'> %s </span>\n"
"           </span>"
"           <span class='gbTrackTitle'> %s </span>\n"
, tdb->shortLabel, assembly, tdb->longLabel);
puts(
"<!-- Info icon built from stacked fa icons -->\n"
"           <a href='#INFO_SECTION' title='Jump to the track description'>\n"
"               <span class='gbIconSmall fa-stack'>\n"
"                   <i class='gbBlueDarkColor fa fa-circle fa-stack-2x'></i>\n"
"                   <i class='gbWhiteColor fa fa-info fa-stack-1x'></i>\n"
"               </span></a>\n"
);
if (tdb->parent)
    {
    // link to supertrack
    char *encodedMapName = cgiEncode(tdb->parent->track);
    char *chromosome = cartUsualString(cart, "c", hDefaultChrom(database));
    puts("&nbsp;");
    printf(
    "       <a href='%s?%s=%s&c=%s&g=%s' title='Go to container track (%s)'>\n"
    "           <i class='gbIconLevelUp fa fa-level-up'></i>\n"
    "       </a>\n",
                hgTrackUiName(), cartSessionVarName(), cartSessionId(cart),
                chromosome, encodedMapName, tdb->parent->shortLabel
    );
    freeMem(encodedMapName);
    }
puts(
"       </div>\n"
);
puts(
"       <div class='col-md-2 text-right'>\n"
);
printGoButton();
puts(
"       </div>\n"
"   </div>\n"
);
}

static void printVisSelect(struct trackDb *tdb)
/* Track visibility dropdown */
{
enum trackVisibility vis = tdb->visibility;
vis = hTvFromString(cartUsualString(cart, tdb->track, hStringFromTv(vis)));
boolean canPack = TRUE;
hTvDropDownClassVisOnlyAndExtra(tdb->track, vis, canPack, "gbSelect normalText visDD",
                                            trackDbSetting(tdb, "onlyVisibility"), NULL);
}

static void printScoreFilter(struct cart *cart, char *track, struct trackDb *tdb)
/* Filter on overall gene expression score */
{
char buf[512];
puts("<b>Limit to genes scored at or above:</b>\n");
safef(buf, sizeof(buf), "%s.%s",  tdb->track, SCORE_FILTER);
int score = cartUsualInt(cart, buf, 0);
int minScore = 0, maxScore = 1000;
cgiMakeIntVarWithLimits(buf, score, "Minimum score", 0, minScore, maxScore);
printf(
"                    (range %d-%d)\n", minScore, maxScore);
}

static void printGtexEqtlConfigPanel(struct trackDb *tdb, char *track)
/* GTEx eQTL track specific controls */
{
puts(
"        <!-- row 1 -->\n"
"        <div class='row'>\n"
"            <div class='gbControl col-md-12'>\n");
gtexEqtlGene(cart, track, tdb);
puts(
"            </div>\n"
"        </div>\n");
puts(
"        <!-- row 2 -->\n"
"        <div class='row'>\n"
"            <div class='gbControl col-md-12'>\n");
gtexEqtlEffectSize(cart, track, tdb);
puts(
"            </div>\n"
"        </div>\n");
puts(
"        <!-- row 3 -->\n"
"        <div class='row'>\n"
"            <div class='gbControl col-md-12'>\n");
gtexEqtlProbability(cart, track, tdb);
puts(
"            </div>\n"
"        </div>\n");
}

static void printGtexGeneConfigPanel(struct trackDb *tdb, char *track)
/* GTEx Gene track specific controls, layout in 3 rows */
{
puts(
"        <!-- row 1 -->\n"
"        <div class='row'>\n"
"            <div class='gbControl col-md-5'>\n");
gtexGeneUiGeneLabel(cart, track, tdb);
puts(
"            </div>\n"
"            <div class='gbControl col-md-7'>\n");
gtexGeneUiGeneModel(cart, track, tdb);
puts(
"            </div>\n"
"        </div>\n");
puts(
"        <!-- row 2 -->\n"
"        <div class='row'>\n"
"            <div class='gbControl col-md-5'>\n");
gtexGeneUiLogTransform(cart, track, tdb);
puts(
"            </div>\n");
puts(
"            <div class='gbControl col-md-7'>\n");
gtexGeneUiViewLimits(cart, track, tdb);
puts(
"            </div>\n"
"        </div>\n");
puts(
"         <!-- row 3 -->\n"
"        <div class='row'>\n");
puts(
"            <div class='gbControl col-md-5'>\n");
gtexGeneUiCodingFilter(cart, track, tdb);
puts(
"            </div>\n");

/* Filter on score */
puts(
"            <div class='gbControl col-md-7'>\n");
printScoreFilter(cart, track, tdb);
puts(
"            </div>\n"
"        </div>\n");
}

static void printConfigPanel(struct trackDb *tdb)
/* Controls for track configuration (except for tissues) */
{
puts(
"        <!-- Configuration panel -->\n"
"        <div class='row gbSectionBanner'>\n"
"            <div class='col-md-8'>Configuration</div>\n"
"            <div class='col-md-4 text-right'>\n");

/* Track vis dropdown */
printVisSelect(tdb);
printGoButton();
puts(
"            </div>\n"
"        </div>\n");

/* Track-specific config */
char *track = tdb->track;
if (gtexIsGeneTrack(track))
    printGtexGeneConfigPanel(tdb, track);
else if (gtexIsEqtlTrack(track))
    printGtexEqtlConfigPanel(tdb, track);
else
    errAbort("Unknown GTEx track: %s\n", track);

puts(
"        <!-- end configure panel -->\n");
}

static void printTissueTable(struct trackDb *tdb)
/* Output HTML with tissue labels and colors, in 2 columns, to fit next to body map */
{
char *version = gtexVersion(tdb->track);
struct gtexTissue *tis, *tissues = gtexGetTissues(version);
char var[512];
safef(var, sizeof var, "%s.%s", tdb->track, GTEX_TISSUE_SELECT);
struct hash *selectedHash = cartHashList(cart, var);
struct gtexTissue **tisTable = NULL;
int count = slCount(tissues);
AllocArray(tisTable, count);
int i=0, col=0;
int cols = 2;
int last = count/2 + 1;

puts(
 " <!-- Tissue list -->\n"
 "<div class='row gbSectionBanner'>\n"
 "  <div class='col-md-1'>Tissues</div>\n"
 "  <div class='col-md-7 gbSectionInfo'>\n"
 "      Click label below or in Body Map to set or clear a tissue\n"
 "  </div>\n"
 "  <div class='col-md-4 gbButtonContainer text-right'>\n"
 "      <div id='setAll' class='gbButtonSetClear gbButton'>set all</div>\n"
 "      <div id='clearAll' class='gbButtonSetClear gbButton'>clear all</div>\n"
 "  </div>\n"
 "</div>\n"
);

puts(
"<table class='gbmTissueTable'>\n");
puts(
"<tr>\n");
for (tis = tissues; tis != NULL; tis = tis->next)
    {
    if (tis->id < last)
        i = tis->id * 2;
    else
        i = (tis->id - last) * 2 + 1;
    tisTable[i] = tis;
    }
boolean all = (hashNumEntries(selectedHash) == 0) ? TRUE : FALSE;
for (i=0; i<count; i++)
    {
    tis = tisTable[i];
    boolean isChecked = all || (hashLookup(selectedHash, tis->name) != NULL);
    printf(
            "<td class='gbmTissueColorPatch %s' "
                "data-tissueColor=#%06X ",
                    isChecked ? "" : "gbmTissueNotSelectedColor", tis->color);
    printf(
                "style='background-color: #%06X;"
                    "border-color: #%06X;'></td>\n",
                    isChecked ? tis->color : 0xFFFFFF, tis->color);
    printf(
            "<td class='gbmTissueLabel %s' id='%s'>%s",
                isChecked ? "gbmTissueSelected" : "", tis->name, tis->description);
    // Hidden checkbox stores value for cart
    printf(
            "<input type='checkbox' name='%s' value='%s' %s style='display: none;'>", 
                var, tis->name, isChecked ? "checked" : "");
    puts(
            "</td>");
    col++;
    if (col > cols-1)
        {
        puts("</tr>\n<tr>");
        col = 0;
        }
    }
puts(
"</tr>\n");
puts(
"</table>");
char buf[512];
safef(buf, sizeof(buf), "%s%s.%s", cgiMultListShadowPrefix(), tdb->track, GTEX_TISSUE_SELECT);
cgiMakeHiddenVar(buf, "0");
}

static void printTrackConfig(struct trackDb *tdb)
/* Print track configuration panels, including Body Map.
The layout is 2-column.  Right column is body map SVG.
Left column has a top panel for configuration settings (non-tissue),
and a lower panel with a tissue selection list.
*/
{
puts(
"<!-- Track Configuration Panels -->\n"
"    <div class='row'>\n"
"        <div class='col-md-6'>\n");
printConfigPanel(tdb);
printTissueTable(tdb);
puts(
"        </div>\n"
"        <div class='col-md-6'>\n");
printBodyMap();
puts(
"        </div>\n"
"    </div>\n"
);
}

static void onclickJumpToTop(char *id)
/* CSP-safe click handler arrows that cause scroll to top */
{
jsOnEventById("click", id, "$('html,body').scrollTop(0);");
}

static void printDataInfo(char *db, struct trackDb *tdb)
{
puts(
"<a name='INFO_SECTION'></a>\n"
"    <div class='row gbSectionBanner'>\n"
"        <div class='col-md-11'>Data Information</div>\n"
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
printUpdateTime(db, tdb, NULL);
puts("</div>");

puts("<div class='dataInfo'>");
makeSchemaLink(db, tdb, "View table schema");
puts("</div>");

puts(
"     </div>\n"
"   </div>\n");
}

static void printTrackDescription(struct trackDb *tdb)
{
puts(
"<a name='TRACK_HTML'></a>\n"
"    <div class='row gbSectionBanner'>\n"
"        <div class='col-md-11'>Track Description</div>\n"
"        <div class='col-md-1'>\n"
);
#define TRACK_INFO_JUMP_ARROW_ID    "hgGtexTrackInfo_jumpArrow"
printf(
"            <i id='%s' title='Jump to top of page' \n"
"               class='gbIconArrow fa fa-lg fa-arrow-circle-up'></i>\n",
TRACK_INFO_JUMP_ARROW_ID
);
onclickJumpToTop(TRACK_INFO_JUMP_ARROW_ID);
puts(
"       </div>\n"
"    </div>\n"
"    <div class='row gbTrackDescriptionPanel'>\n"
"       <div class='gbTrackDescription'>\n");
puts(tdb->html);
puts(
"       </div>\n"
"   </div>\n");
}

static struct trackDb *getTrackDb(char *db, char *track)
/* Check if this is an assembly with GTEx track and get trackDb */
{
struct sqlConnection *conn = sqlConnect(db);
if (conn == NULL)
    errAbort("Can't connect to database %s\n", db);
char where[256];
safef(where, sizeof(where), "tableName='%s'", track);
// WARNING: this will break in sandboxes unless trackDb entry is pushed to hgwdev.
// The fix of using hTrackDbList() would slow for all users, so leaving as is.
struct trackDb *tdb = trackDbLoadWhere(conn, "trackDb", where);
trackDbAddTableField(tdb);
char *parent = trackDbLocalSetting(tdb, "parent");
struct trackDb *parentTdb;
if (parent)
    {
    safef(where, sizeof(where), "tableName='%s'", parent);
    parentTdb = trackDbLoadWhere(conn, "trackDb", where);
    if (parentTdb)
        tdb->parent = parentTdb;
    }
sqlDisconnect(&conn);
return tdb;
}

static void doMiddle(struct cart *theCart)
/* Send HTML with javascript to display the user interface. */
{
cart = theCart;
char *db = NULL, *genome = NULL, *clade = NULL;
getDbGenomeClade(cart, &db, &genome, &clade, oldVars);
database = db;

// Start web page with new-style header
webStartGbNoBanner(cart, db, "Genome Browser GTEx Track Settings");
puts("<link rel='stylesheet' href='../style/gb.css'>");         // NOTE: This will likely go to web.c
puts("<link rel='stylesheet' href='../style/hgGtexTrackSettings.css'>");

char *track = cartUsualString(cart, "g", "gtexGene");
struct trackDb *tdb = getTrackDb(db, track);
if (!tdb)
    errAbort("No GTEx track %s found in database %s\n", track, db);

// Container for bootstrap grid layout
puts(
"<div class='container-fluid'>\n");

// Print form with configuration HTML, and track description
printf(
"<form action='%s' name='MAIN_FORM' method=%s>\n\n",
                hgTracksName(), cartUsualString(cart, "formMethod", "POST"));
printTrackHeader(db, tdb);
printTrackConfig(tdb);
puts(
"</form>");
printDataInfo(db, tdb);
if (tdb->html)
    printTrackDescription(tdb);
puts(
"</div>");

// Initialize illustration display and handle mouseover and clicks
jsIncludeFile("utils.js", NULL);
jsIncludeFile("hgGtexTrackSettings.js", NULL);

webIncludeFile("inc/gbFooter.html");
webEndJWest();
}


int main(int argc, char *argv[])
/* Process CGI / command line. */
{
/* Null terminated list of CGI Variables we don't want to save to cart */
char *excludeVars[] = {"submit", "Submit", "g", NULL};
long enteredMainTime = clock1000();
cgiSpoof(&argc, argv);
oldVars = hashNew(10);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgGtexTrackSettings", enteredMainTime);
return 0;
}


