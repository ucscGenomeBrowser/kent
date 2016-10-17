/* hgGtexTrackSettings: Configure GTEx track
 *
 * Copyright (C) 2016 The Regents of the University of California
 */

#include "common.h"
#include "cart.h"
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
#include "regexHelper.h"
#include "suggest.h"
#include "trackHub.h"
#include "trix.h"
#include "web.h"

#include "gtexInfo.h"
#include "gtexTissue.h"

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */
struct hash *oldVars = NULL;          /* Old contents of cart before it was updated by CGI */
char *db = NULL;

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

static void printTrackDescription()
{
puts("<div class='row gbSectionBanner gbSimpleBanner'>Track Description</div>");
puts("<a name='TRACK_HTML'></a>");
struct sqlConnection *conn = sqlConnect(db);
char query[256];
sqlSafef(query, sizeof(query), "select html from trackDb where tableName='gtexGene'");
char *html = sqlQuickString(conn, query);
sqlDisconnect(&conn);
puts("<div class='trackDescriptionPanel'>");
puts("<div class='trackDescription'>");
puts(html);
puts("</div></div>");
puts("</div>");
}

static void printTissueTable(char *version)
/* Output HTML with tissue labels and colors, in 2 columns, to fit next to body map */
{
struct gtexTissue *tis, *tissues = gtexGetTissues(version);
struct gtexTissue **tisTable = NULL;
int count = slCount(tissues);
AllocArray(tisTable, count);
int i=0, col=0;
int cols = 2;
int last = count/2 + 1;

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

static void doMainPage()
/* Send HTML with javascript to bootstrap the user interface. */
{
// Start web page with new banner
char *genome = NULL, *clade = NULL;
getDbGenomeClade(cart, &db, &genome, &clade, oldVars);

// char *chromosome = cartUsualString(cart, "c", hDefaultChrom(database));
//char *track = cartString(cart, "g");

webStartJWestNoBanner(cart, db, "Genome Browser GTEx Track Settings");
puts("<link rel=\"stylesheet\" href=\"../style/bootstrap.min.css\">");
puts("<link rel=\"stylesheet\" href=\"../style/hgGtexTrackSettings.css\">");

// The initial visible page elements are hgGtexTrackSettings.html, 
// which is transformed into a quoted .h
// file containing a string constant that we #include and print here (see makefile).
puts(
#include "hgGtexTrackSettings.html.h"
);

//struct tdb *tdbList = NULL;
//struct trackDb *tdb = tdbForTrack(db, track, &tdbList);
//printTissueTable(gtexGetVersion(tdb->table));

char *table = "gtexGene";

printTissueTable(gtexVersion(table));
puts("</div></div></div></div>");

printTrackDescription();
puts("</div>");

// end panel, section and body layout container 

// Track description

// JS libraries
doJsIncludes();

// Main JS
puts("<script src=\"../js/hgGtexTrackSettings.js\"></script>");

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
char *excludeVars[] = {CARTJSON_COMMAND, NULL};
cgiSpoof(&argc, argv);
oldVars = hashNew(10);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}


