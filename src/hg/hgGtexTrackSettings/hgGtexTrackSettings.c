/* hgGtexTrackSettings: Configure GTEx track
 *
 * Copyright (C) 2016 The Regents of the University of California
 */

#include "common.h"
#include "trackDb.h"
#include "cart.h"
#include "portable.h"
#include "gtexUi.h"

/* TODO: prune */
#include "cartJson.h"
#include "cheapcgi.h"
#include "errCatch.h"
#include "googleAnalytics.h"
#include "hCommon.h"
#include "hgConfig.h"
#include "hdb.h"
#include "htmshell.h"
#include "hubConnect.h"
#include "hui.h"
#include "jsHelper.h"
#include "jsonParse.h"
#include "obscure.h"  // for readInGulp
#include "web.h"

#include "gtexInfo.h"
#include "gtexTissue.h"

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */
struct hash *oldVars = NULL;          /* Old contents of cart before it was updated by CGI */
char *db = NULL;
char *version;                        /* GTEx release */
struct trackDb *tdb = NULL;

static void doCartJson()
/* Perform UI commands to update the cart and/or retrieve cart vars & metadata. */
{
struct cartJson *cj = cartJsonNew(cart);
//e.g. cartJsonRegisterHandler(cj, "setTaxId", setTaxId);
cartJsonExecute(cj);
}

static void doJsIncludes()
/* Include JS libraries.  From hgGateway (think about libifying) */
{
//puts("<script src=\"../js/es5-shim.4.0.3.min.js\"></script>");
//puts("<script src=\"../js/es5-sham.4.0.3.min.js\"></script>");
//puts("<script src=\"../js/lodash.3.10.0.compat.min.js\"></script>");
puts("<script src=\"../js/cart.js\"></script>");

webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("jquery-ui.js", NULL);
jsIncludeFile("jquery.watermarkinput.js", NULL);
jsIncludeFile("utils.js",NULL);
}

static void printTrackHeader()
/* Print top banner with track labels */
{
char *assembly = stringBetween("(", ")", hFreezeFromDb(db));

puts(
"<a name='TRACK_TOP'></a>\n"
"   <div class='row gbTrackTitle'>\n"
"       <div class='col-md-10'>\n"
);

printf(
"           <span class='gbTrackName'>\n"
"               %s Track\n"
"               <span class='gbAssembly'>%s</span>\n"
"           </span>\n"
"           &nbsp;&nbsp;&nbsp;&nbsp;&nbsp; %s &nbsp;&nbsp;&nbsp;\n"
, tdb->shortLabel, assembly, tdb->longLabel);

puts(
"           <a href='#TRACK_HTML' title='Jump to the track description'><span class='gbFaStack fa-stack'><i class='gbGoIcon fa fa-circle fa-stack-2x'></i><i class='gbIconText fa fa-info fa-stack-1x'></i></span>"
"           </a>"
"       </div>"
"       <div class='col-md-2 text-right'>"
"           <div class='goButtonContainer' title='Go to the Genome Browser'>"
"               <div class='gbGoButton'>GO</div>"
"               <i class='gbGoIcon fa fa-play fa-2x'></i>"
"           </div>"
"       </div>"
"   </div>"
);
}

static void printTissueTable()
/* Output HTML with tissue labels and colors, in 2 columns, to fit next to body map */
{
struct gtexTissue *tis, *tissues = gtexGetTissues(version);
struct gtexTissue **tisTable = NULL;
int count = slCount(tissues);
AllocArray(tisTable, count);
int i=0, col=0;
int cols = 2;
int last = count/2 + 1;

puts(
     " <!-- Tissue list -->"
     "<div class='row gbSectionBanner'>"
     "  <div class='col-md-1'>Tissues</div>"
     "  <div class='col-md-7 gbSectionInfo'>"
     "      Click label below or in Body Map to set or clear a tissue"
     "  </div>"
     "  <div class='col-md-4 gbButtonContainer'>"
     "      <div id='setAll' class='goButtonContainer gtButton gbWhiteButton'>set all</div>"
     "      <div id='clearAll' class='goButtonContainer gtButton gbWhiteButton'>clear all</div>"
     "  </div>"
     "</div>"
    );

puts("<table class='tissueTable'>");
puts("<tr>");
for (tis = tissues; tis != NULL; tis = tis->next)
    {
    if (tis->id < last)
        i = tis->id * 2;
    else
        i = (tis->id - last) * 2 + 1;
    tisTable[i] = tis;
    }
for (i=0; i<count; i++)
    {
    tis = tisTable[i];
    printf("<td class='tissueColor' bgcolor=%06X></td>"
           "<td class='tissueLabel' id='%s'>%s</td>", 
                tis->color, tis->name, tis->description);
    col++;
    if (col > cols-1)
        {
        puts("</tr>\n<tr>");
        col = 0;
        }
    }
puts("</tr>\n");
puts("</table>");
}

static void printBodyMap()
{
puts(
"       <!-- Body Map panel -->"
"           <object id='bodyMapSvg' type='image/svg+xml' class='gbImage gtexBodyMap' data='/images/bodyMap.svg'>"
"               Body Map illustration not found"
"           </object>"
);
}

static void printVisSelect()
{
enum trackVisibility vis = tdb->visibility;
vis = hTvFromString(cartUsualString(cart, tdb->track, hStringFromTv(vis)));
boolean canPack = TRUE;
hTvDropDownClassVisOnlyAndExtra(tdb->track, vis, canPack, "gbSelect normalText visDD",
                                            trackDbSetting(tdb, "onlyVisibility"), NULL);
}

static void printConfigPanel()
/* Controls for track configuration (except for tissues) */
{
char cartVar[1024];
char buf[512];

char *track = tdb->track;

puts(
"            <!-- Configuration panel -->\n"

/* Blue section header and track vis dropdown */
"            <div class='row gbSectionBanner'>\n"
"                <div class='col-md-10'>Configuration</div>\n"
"                <div class='col-md-2 text-right'>\n"
);
printVisSelect();
puts(
"                </div>\n"
);
puts(
"            </div>\n"
);

/* First row of config controls */
puts(
"            <!-- row 1 -->\n"
"            <div class='row'>\n"

/* Gene labels */
"                <div class='configurator col-md-5'>\n"
"<b>Label</b>:&nbsp;&nbsp;"
);
char *geneLabel = cartUsualStringClosestToHome(cart, tdb, isNameAtParentLevel(tdb, track),
                        GTEX_LABEL, GTEX_LABEL_DEFAULT);
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_LABEL);
cgiMakeRadioButton(cartVar, GTEX_LABEL_SYMBOL , sameString(GTEX_LABEL_SYMBOL, geneLabel));
printf("%s ", "gene symbol");
cgiMakeRadioButton(cartVar, GTEX_LABEL_ACCESSION, sameString(GTEX_LABEL_ACCESSION, geneLabel));
printf("%s ", "accession");
cgiMakeRadioButton(cartVar, GTEX_LABEL_BOTH, sameString(GTEX_LABEL_BOTH, geneLabel));
printf("%s ", "both");
puts(
"                </div>\n"

/* Show exons in gene model */
"                <div class='configurator col-md-7'>\n"
);
puts("<b>Show GTEx gene model</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_SHOW_EXONS);
boolean showExons = cartCgiUsualBoolean(cart, cartVar, GTEX_SHOW_EXONS_DEFAULT);
cgiMakeCheckBox(cartVar, showExons);
puts(
"                </div>\n"
);
puts(
"            </div>\n"
);

/* Row 2 */
puts(
"            <!-- row 2 -->\n"
"            <div class='row'>\n"
);

/* Log transform. When selected, the next control (view limits max) is disabled */
puts(
"                <div class='configurator col-md-5'>\n"
);
puts("<b>Log10 transform:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_LOG_TRANSFORM);
boolean isLogTransform = cartCgiUsualBoolean(cart, cartVar, GTEX_LOG_TRANSFORM_DEFAULT);
safef(buf, sizeof buf, "onchange='gtexTransformChanged(\"%s\")'", track);
cgiMakeCheckBoxJS(cartVar, isLogTransform, buf);
puts(
"                </div>\n"
);
/* Viewing limits max.  This control is disabled if log transform is selected */
// construct class so JS can toggle
puts(
"                <div class='configurator col-md-7'>\n"
);
safef(buf, sizeof buf, "%sViewLimitsMaxLabel %s", track, isLogTransform ? "disabled" : "");
printf("<span class='%s'><b>View limits maximum:</b></span>\n", buf);
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_MAX_LIMIT);
int viewMax = cartCgiUsualInt(cart, cartVar, GTEX_MAX_LIMIT_DEFAULT);
cgiMakeIntVarWithExtra(cartVar, viewMax, 4, isLogTransform ? "disabled" : "");
char *version = gtexVersion(tdb->table);
printf("<span class='%s'>  RPKM (range 0-%d)</span>\n", buf, round(gtexMaxMedianScore(version)));
puts(
"                </div>\n"
);
puts(
"            </div>\n"
);

/* Row 3 */
puts(
"            <!-- row 3 -->\n"
"            <div class='row'>\n"
);
/* Filter on coding genes */
puts(
"                <div class='configurator col-md-5'>\n"
);
puts("<b>Limit to protein coding genes:</b>\n");
safef(cartVar, sizeof(cartVar), "%s.%s", track, GTEX_CODING_GENE_FILTER);
boolean isCodingOnly = cartCgiUsualBoolean(cart, cartVar, GTEX_CODING_GENE_FILTER_DEFAULT);
cgiMakeCheckBox(cartVar, isCodingOnly);
puts(
"                </div>\n"
);

/* Filter on score */
puts(
"                <div class='configurator col-md-7'>\n"
);
puts("<b>Limit to genes scored at or above:</b>\n");
puts(
"                    <input id='scoreInput' value='' size='8'>\n"
"                    (range 0-1000)\n"
);
puts(
"               </div>\n"
);
puts(
"           </div>\n"
);
puts(
"            <!-- end configure panel -->\n"
);
}

static void printTrackConfig()
/* Print track configuration panels, including Body Map.
The layout is 2-column.  Left column is body map SVG.
Right column has a top panel for configuration settings (non-tissue),
and a lower panel with a tissue selection list.
*/
{
puts(
"  <div class='row'>\n"
);

// TODO: remove bodyMap id here
puts(
"        <div id='bodyMap' class='col-md-6'>\n"
);
printBodyMap();
puts(
"        </div>\n"
);
puts(
"        <div class='col-md-6'>\n"
);
printConfigPanel();
printTissueTable();
puts(
"        </div>\n"
);
puts(
"  </div>\n"
);
}

static void printTrackDescription()
{
puts("<a name='TRACK_HTML'></a>");
puts("<div class='row gbSectionBanner gbSimpleBanner'>");
puts("<div class='col-md-11'>Track Description</div>");
puts("<div class='col-md-1'>"
        "<a href='#TRACK_TOP' title='Jump to top of page'>"
        "<i class='gbBannerIcon gbGoIcon fa fa-lg fa-arrow-circle-up'></i>"
        //"<i class='gbBannerIcon gbGoIcon fa fa-lg fa-level-up'></i>"
        "</a></div>");
puts("</div>");
if (tdb->html != NULL)
    {
    puts("<div class='trackDescriptionPanel'>");
    puts("<div class='trackDescription'>");
puts(tdb->html);
    }
puts("</div></div>");
puts("</div>");
}

static void doMainPage()
/* Send HTML with javascript to bootstrap the user interface. */
{
// Start web page with new banner

webStartJWestNoBanner(cart, db, "Genome Browser GTEx Track Settings");
puts("<link rel=\"stylesheet\" href=\"../style/bootstrap.min.css\">");
puts("<link rel=\"stylesheet\" href=\"../style/hgGtexTrackSettings.css\">");

char *genome = NULL, *clade = NULL;
// char *chromosome = cartUsualString(cart, "c", hDefaultChrom(database));
//char *track = cartString(cart, "g");
getDbGenomeClade(cart, &db, &genome, &clade, oldVars);

// Check if this is an assembly with GTEx track
struct sqlConnection *conn = sqlConnect(db);
if (conn == NULL)
    errAbort("Can't connect to database %s\n", db);

char *table = "gtexGene";
version = gtexVersion(table);

// TODO: use hdb, hTrackDbList to get table names of trackDb, 
tdb = trackDbLoadWhere(conn, "trackDb", "tableName = 'gtexGene'");
sqlDisconnect(&conn);
if (!tdb)
    errAbort("No GTEx track found in database %s\n", db);

puts(
"<div class='container-fluid'>\n"
);
printTrackHeader();
printTrackConfig();
printTrackDescription();
puts("</div>");

// end panel, section and body layout container 

// Track description

// JS libraries
doJsIncludes();

// Main JS
puts("<script src='../js/hgGtexTrackSettings.js'></script>");

webIncludeFile("inc/jWestFooter.html");

webEndJWest();
}


void doMiddle(struct cart *theCart)
/* Display the main page or execute a command and update the page */
{
cart = theCart;
if (cgiOptionalString(CARTJSON_COMMAND))
    doCartJson();
else
    doMainPage();
}

int main(int argc, char *argv[])
/* Process CGI / command line. */
{
/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
/* TODO: check these */
char *excludeVars[] = { "submit", "Submit", "g", NULL, "ajax", NULL,};
//char *excludeVars[] = {CARTJSON_COMMAND, NULL};
long enteredMainTime = clock1000();
cgiSpoof(&argc, argv);
oldVars = hashNew(10);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
cgiExitTime("hgGtexTrackSettings", enteredMainTime);
return 0;
}


