/* hgMyData - CGI for managing a users' tracks and other data. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "udc.h"
#include "knetUdc.h"
#include "jsHelper.h"
#include "cartJson.h"
#include "wikiLink.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

char *getUserName()
{
return (loginSystemEnabled() || wikiLinkEnabled()) ? wikiLinkUserName() : NULL;
}

void removeTrack(struct cartJson *cj, struct hash *paramHash)
/* Removes a custom track for this user */
{
//char *userName = getUserName();
}

void uploadTrack(struct cartJson *cj, struct hash *paramHash)
/* Saves a new track to the persistent storage for this user */
{
//char *userName = getUserName();
}

void listTracks(struct cartJson *cj, struct hash *paramHash)
/* Return the list of:
 *   - Only if logged in:
 *       - custom tracks in saved sessions
 *       - non-public hubs in saved sessions
 *       - any other files stored in the per user directory
 *   - custom tracks in non-saved sessions
 *   - non-public hubs in non-saved sessions
 * Present data as a plain array for a table view */
{
char *userName = getUserName();
if (userName)
    {
    return;
    }
// throw in the custom tracks that are in the current cart that may not be saved yet
// throw in the attached hubs that are non-public hubs
}

void getUiState(struct cartJson *cj, struct hash *paramHash)
/* Get just the JSON needed to show the initial web page */
{
}


void printMainPageIncludes()
{
webIncludeResourceFile("gb.css");
webIncludeResourceFile("gbStatic.css");
webIncludeResourceFile("spectrum.min.css");
webIncludeResourceFile("hgGtexTrackSettings.css");
puts("<link rel='stylesheet' href='https://code.jquery.com/ui/1.10.3/themes/smoothness/jquery-ui.css'>");
puts("<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.2.1/themes/default/style.min.css' />");
puts("<script src='https://cdnjs.cloudflare.com/ajax/libs/jquery/1.12.1/jquery.min.js'></script>");
puts("<script src=\"//code.jquery.com/ui/1.10.3/jquery-ui.min.js\"></script>");
puts("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.7/jstree.min.js\"></script>\n");
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("lodash.3.10.0.compat.min.js", NULL);
jsIncludeFile("cart.js", NULL);
jsIncludeFile("hgMyData.js", NULL);

// Write the skeleton HTML, which will get filled out by the javascript
webIncludeFile("inc/hgMyData.html");
webIncludeFile("inc/gbFooter.html");
}

void doMainPage()
{
char *database = NULL;
char *genome = NULL;
getDbAndGenome(cart, &database, &genome, oldVars);

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

webStartGbNoBanner(cart, database, "Manage My Data");
printMainPageIncludes();
char *hgsid = cartSessionId(cart);

jsInlineF("var hgsid='%s';\n", hgsid);
struct cartJson *cj = cartJsonNew(cart);
jsInlineF("%s;\n", cartJsonDumpJson(cj));
// Call our init function to fill out the page
jsInline("hgMyData.init();\n");
webEndGb();
}

void doCartJson()
/* Register functions that return JSON to client */
{
struct cartJson *cj = cartJsonNew(cart);
cartJsonRegisterHandler(cj, "getUiState", getUiState);
cartJsonRegisterHandler(cj, "remove", removeTrack);
cartJsonRegisterHandler(cj, "upload", uploadTrack);
cartJsonRegisterHandler(cj, "list", listTracks);
cartJsonExecute(cj);
}

void doMiddle(struct cart *theCart)
/* Set up globals and dispatch appropriately */
{
cart = theCart;
if (cgiOptionalString(CARTJSON_COMMAND))
    doCartJson();
else
    doMainPage();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
