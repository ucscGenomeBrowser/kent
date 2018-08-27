/* hgs - redirects to a session. */
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
#include "hdb.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

void hgs(struct cart *theCart)
/* Redirect to hgTracks with the userName and sessionName from the REQUEST_URI variable */
{
int partLen = 0;
char *request = cloneString(getenv("REQUEST_URI"));
if (request != NULL)
    partLen = chopString(request, "/", NULL, 0);
if (partLen < 4)
    {
    warn("Provide a userName and sessionName in the url, for example: %shgs/userName/sessionName", hLocalHostCgiBinUrl());
    return;
    }
else
    {
    // redirect, same as hgLinkIn
    char *parts[partLen];
    chopString(request, "/", parts, partLen);
    char url[2048];
    safef(url, sizeof(url), 
            "../../hgTracks?hgS_doOtherUser=submit&hgS_otherUserName=%s&hgS_otherUserSessionName=%s", parts[partLen-2], parts[partLen-1]);
    char redirect[4096];
    printf("<br>Redirecting to <a href=\"%s\">%s</a>.", url, url);
    safef(redirect, sizeof(redirect), "window.location='%s';", url);
    jsInline(redirect);
    return;
    }
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

cartWebStart(cart, database, "Redirects to a session");
hgs(cart);
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers();
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
