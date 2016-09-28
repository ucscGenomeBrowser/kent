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

/* Global Variables */
struct cart *cart = NULL;             /* CGI and other variables */
struct hash *oldVars = NULL;          /* Old contents of cart before it was updated by CGI */

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

jsIncludeDataTablesLibs();

webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("jquery-ui.js", NULL);
jsIncludeFile("jquery.watermarkinput.js", NULL);
jsIncludeFile("utils.js",NULL);
}

static void doMainPage()
/* Send HTML with javascript to bootstrap the user interface. */
{
// Start web page with new banner
char *db = NULL, *genome = NULL, *clade = NULL;
getDbGenomeClade(cart, &db, &genome, &clade, oldVars);

// char *chromosome = cartUsualString(cart, "c", hDefaultChrom(database));
//char *track = cartString(cart, "g");

webStartJWest(cart, db, 
        "Genome Browser Track Settings");
puts("<link rel=\"stylesheet\" href=\"../style/bootstrap.min.css\">");
puts("<link rel=\"stylesheet\" href=\"../style/hgGtexTrackSettings.css\">");

// The visible page elements are all in ./hgGtexTrackSettings.html, which is transformed into a quoted .h
// file containing a string constant that we #include and print here (see makefile).
puts(
#include "hgGtexTrackSettings.html.h"
);

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


