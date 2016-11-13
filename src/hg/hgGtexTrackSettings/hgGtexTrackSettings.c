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
#include "gtexUi.h"
#include "gtexInfo.h"
#include "gtexTissue.h"

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */
struct hash *oldVars = NULL;          /* Old contents of cart before it was updated by CGI */
char *db = NULL;
char *version;                        /* GTEx release */
struct trackDb *trackDb = NULL;

static void printTrackHeader()
/* Print top banner with track labels */
// TODO: Try to simplify layout
{
char *assembly = stringBetween("(", ")", hFreezeFromDb(db));
puts(
"<a name='TRACK_TOP'></a>\n"
"    <div class='row gbTrackTitle'>\n"
"       <div class='col-md-10'>\n"
);
printf(
"           <span class='gbTrackName'>\n"
"               %s Track\n"
"               <span class='gbAssembly'>%s</span>\n"
"           </span>\n"
"           &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; %s &nbsp;&nbsp;&nbsp;\n"
, trackDb->shortLabel, assembly, trackDb->longLabel);
puts(
"           <a href='#TRACK_HTML' title='Jump to the track description'><span class='gbFaStack fa-stack'><i class='gbGoIcon fa fa-circle fa-stack-2x'></i><i class='gbIconText fa fa-info fa-stack-1x'></i></span></a>\n"
"       </div>\n"
"       <div class='col-md-2 text-right'>\n"
"           <div class='goButtonContainer' title='Go to the Genome Browser'>\n"
"               <div class='gbGoButton'>GO</div>\n"
"               <i class='gbGoIcon fa fa-play fa-2x'></i>\n"
"           </div>\n"
"       </div>\n"
"   </div>\n");
}

static void printBodyMap()
{
puts(
"        <!-- Body Map panel -->\n"
"           <object id='bodyMapSvg' type='image/svg+xml' class='gbImage gtexBodyMap' data='/images/bodyMap.svg'>\n"
"               Body Map illustration not found\n"
"           </object>\n");
}

static void printVisSelect()
/* Track visibility dropdown */
{
enum trackVisibility vis = trackDb->visibility;
vis = hTvFromString(cartUsualString(cart, trackDb->track, hStringFromTv(vis)));
boolean canPack = TRUE;
hTvDropDownClassVisOnlyAndExtra(trackDb->track, vis, canPack, "gbSelect normalText visDD",
                                            trackDbSetting(trackDb, "onlyVisibility"), NULL);
}

static void printScoreFilter(struct cart *cart, char *track)
/* Filter on overall gene expression score */
{
char buf[512];
puts("<b>Limit to genes scored at or above:</b>\n");
safef(buf, sizeof(buf), "%s.%s",  trackDb->track, SCORE_FILTER);
int score = cartUsualInt(cart, buf, 0);
int minScore = 0, maxScore = 1000;
cgiMakeIntVarWithLimits(buf, score, "Minimum score", 0, minScore, maxScore);
printf(
"                    (range %d-%d)\n", minScore, maxScore);
}

static void printConfigPanel()
/* Controls for track configuration (except for tissues) */
{
char *track = trackDb->track;
puts(
"        <!-- Configuration panel -->\n"
"        <div class='row gbSectionBanner'>\n"
"            <div class='col-md-10'>Configuration</div>\n"
"            <div class='col-md-2 text-right'>\n");

/* Track vis dropdown */
printVisSelect();
puts(
"            </div>\n"
"        </div>\n");

/* GTEx-specific track controls, layout in 3 rows */
puts(
"        <!-- row 1 -->\n"
"        <div class='row'>\n"
"            <div class='configurator col-md-5'>\n");
gtexGeneUiGeneLabel(cart, track, trackDb);
puts(
"            </div>\n"
"            <div class='configurator col-md-7'>\n");
gtexGeneUiGeneModel(cart, track, trackDb);
puts(
"            </div>\n"
"        </div>\n");
puts(
"        <!-- row 2 -->\n"
"        <div class='row'>\n"
"            <div class='configurator col-md-5'>\n");
gtexGeneUiLogTransform(cart, track, trackDb);
puts(
"            </div>\n");
puts(
"            <div class='configurator col-md-7'>\n");
gtexGeneUiViewLimits(cart, track, trackDb);
puts(
"            </div>\n"
"        </div>\n");
puts(
"         <!-- row 3 -->\n"
"        <div class='row'>\n");
puts(
"            <div class='configurator col-md-5'>\n");
gtexGeneUiCodingFilter(cart, track, trackDb);
puts(
"            </div>\n");

/* Filter on score */
puts(
"            <div class='configurator col-md-7'>\n");
printScoreFilter(cart, track);
puts(
"            </div>\n"
"        </div>\n");
puts(
"        <!-- end configure panel -->\n");
}

static void printTissueTable()
/* Output HTML with tissue labels and colors, in 2 columns, to fit next to body map */
{
struct gtexTissue *tis, *tissues = gtexGetTissues(version);
char var[512];
safef(var, sizeof var, "%s.%s", trackDb->track, GTEX_TISSUE_SELECT);
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
 "  <div class='col-md-4 gbButtonContainer'>\n"
 "      <div id='setAll' class='gbButtonContainer gtButton gbWhiteButton'>set all</div>\n"
 "      <div id='clearAll' class='gbButtonContainer gtButton gbWhiteButton'>clear all</div>\n"
 "  </div>\n"
 "</div>\n"
);

puts(
"<table class='tissueTable'>\n");
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
            "<td class='tissueColor %s' "
                "data-tissueColor=#%06X ",
                    isChecked ? "" : "tissueNotSelectedColor", tis->color);
    printf(
                "style='background-color: #%06X;"
                    "border-style: solid; border-width: 2px;"
                    "border-color: #%06X;'></td>\n",
                    isChecked ? tis->color : 0xFFFFFF, tis->color);
    printf(
            "<td class='tissueLabel %s' id='%s'>%s",
                isChecked ? "tissueSelected" : "", tis->name, tis->description);
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
safef(buf, sizeof(buf), "%s%s.%s", cgiMultListShadowPrefix(), trackDb->track, GTEX_TISSUE_SELECT);
cgiMakeHiddenVar(buf, "0");
}

static void printTrackConfig()
/* Print track configuration panels, including Body Map.
The layout is 2-column.  Left column is body map SVG.
Right column has a top panel for configuration settings (non-tissue),
and a lower panel with a tissue selection list.
*/
{
puts(
"<!-- Track Configuration Panels -->\n"
"    <div class='row'>\n"
"        <div class='col-md-6'>\n");
printBodyMap();
puts(
"        </div>\n"
"        <div class='col-md-6'>\n");
printConfigPanel();
printTissueTable();
puts(
"        </div>\n"
"    </div>\n");
}

static void printTrackDescription()
{
puts(
"<a name='TRACK_HTML'></a>\n"
"    <div class='row gbSectionBanner gbSimpleBanner'>\n"
"        <div class='col-md-11'>Track Description</div>\n"
"        <div class='col-md-1'>\n"
"            <a href='#TRACK_TOP' title='Jump to top of page'>\n"
"                <i class='gbBannerIcon gbGoIcon fa fa-lg fa-arrow-circle-up'></i>\n"
"            </a>\n"
"       </div>\n"
"    </div>\n"
"    <div class='trackDescriptionPanel'>\n"
"       <div class='trackDescription'>\n");
puts(trackDb->html);
puts(
"       </div>\n"
"   </div>\n");
}

static struct trackDb *getTrackDb(char *database, char *track)
/* Check if this is an assembly with GTEx track and get trackDb */
{
struct sqlConnection *conn = sqlConnect(db);
if (conn == NULL)
    errAbort("Can't connect to database %s\n", db);
char where[256];
safef(where, sizeof(where), "tableName='%s'", track);
// TODO: use hdb, hTrackDbList to get table names of trackDb, 
struct trackDb *tdb = trackDbLoadWhere(conn, "trackDb", where);
sqlDisconnect(&conn);
return tdb;
}

static void doMiddle(struct cart *theCart)
/* Send HTML with javascript to display the user interface. */
{
cart = theCart;

// Start web page with new-style header
webStartJWestNoBanner(cart, db, "Genome Browser GTEx Track Settings");
puts("<link rel='stylesheet' href='../style/bootstrap.min.css'>");
puts("<link rel='stylesheet' href='../style/hgGtexTrackSettings.css'>");

char *genome = NULL, *clade = NULL;
getDbGenomeClade(cart, &db, &genome, &clade, oldVars);
char *track = cartString(cart, "g");
trackDb = getTrackDb(db, track);
if (!trackDb)
    errAbort("No GTEx track %s found in database %s\n", track, db);
version = gtexVersion(track);

// Container for bootstrap grid layout
puts(
"<div class='container-fluid'>\n");

// Print form with configuration HTML, and track description
printf(
"<form action='%s' name='MAIN_FORM' method=%s>\n\n",
                hgTracksName(), cartUsualString(cart, "formMethod", "POST"));
printTrackHeader();
printTrackConfig();
puts(
"</form>");
if (trackDb->html)
    printTrackDescription();
puts(
"</div>");

// Initialize illustration display and handle mouseover and clicks
puts("<script src='../js/hgGtexTrackSettings.js'></script>");

webIncludeFile("inc/jWestFooter.html");
webEndJWest();
}


int main(int argc, char *argv[])
/* Process CGI / command line. */
{
/* Null terminated list of CGI Variables we don't want to save to cart */
/* TODO: check these */
char *excludeVars[] = {"submit", "Submit", "g", NULL};
long enteredMainTime = clock1000();
cgiSpoof(&argc, argv);
oldVars = hashNew(10);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgGtexTrackSettings", enteredMainTime);
return 0;
}


