/* Copyright (C) 2017 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

/* hgLinkIn - Link external IDs to an assembly and a position. */

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
#include "genbank.h"
#include "memalloc.h"

#include "linkInHandlers.h"
#include "handlerList.h"


/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;


void displayLinkInResults(char *linkInId, char *linkInResource, struct linkInResult *results)
/* Take the list of results and display them.  Here, that generally means redirecting to
 * hgTracks with a db and position. */
{
/* If only one result, jump to it.  Otherwise, for now, jump to the first */
int hitCount = slCount(results);
if (hitCount == 0)
    {
    printf ("Error: No results found for ID %s", linkInId);
    if (linkInResource != NULL)
        printf (" in database %s\n", linkInResource);
    }
else if (hitCount == 1)
    {
    char url[2048];
    safef(url, sizeof(url), "../cgi-bin/hgTracks?db=%s&position=%s&%s=pack",
            results->db, results->position, results->trackToView);
    char redirect[4096];
    safef(redirect, sizeof(redirect), "window.location='%s';", url);
    jsInline(redirect);
    printf("Redirecting to <a href='%s'>%s</a>.", url, url);
    }
else
    {
    // Multiple hits
    char url[2048];
    safef(url, sizeof(url), "../cgi-bin/hgTracks?db=%s&position=%s&%s=pack",
            results->db, results->position, results->trackToView);
    char redirect[4096];
    safef(redirect, sizeof(redirect), "window.location='%s';", url);
    jsInline(redirect);
    printf("Redirecting to <a href='%s'>%s</a>.", url, url);
    }
}


void hgLinkIn(struct cart *theCart)
/* Perform a search for an identifier supplied in the cart, or squack that
 * no identifier was provided. */
{
struct linkInResult *results = NULL;
registerLinkInHandlers();

char *linkInResource = cgiOptionalString("resource");
char *linkInId = cgiOptionalString("id");

if (isEmpty(linkInId))
    {
    /* user never gave us an ID; display interface */
    warn("No ID supplied - must specify id=&ltid&gt in URL. Interactive "
            "mode not yet implemented.");
        return;
    }

if (isEmpty(linkInResource) || sameString(linkInResource, "all"))
    {
    /* try all possible linkIns for a list of results */
    results = checkAllLinkInHandlers(linkInId);
    }
else
    {
    /* dispatch a single linkIn handler based on the id (if we have that handler) */
    results = checkLinkInHandlerForResource(linkInResource, linkInId);
    }
displayLinkInResults(linkInId, linkInResource, results);
}


void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
char *database = NULL;
char *genome = NULL;
getDbAndGenome(cart, &database, &genome, oldVars);
initGenbankTableNames(database);

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

cartWebStart(cart, database, "Link external IDs to an assembly and a position");
hgLinkIn(cart);
cartWebEnd();
}



/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "resource", "id", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(LIMIT_2or6GB);
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
cgiSpoof(&argc, argv);

cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);

return 0;
}
