/* hubApi - access mechanism to hub data resources. */
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
#include "trackHub.h"
#include "hgConfig.h"
#include "hCommon.h"
#include "hPrint.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

static void trackList(struct trackDb *tdb)
{
if (tdb)
    {
    hPrintf("    <ul>\n");
    struct trackDb *track = tdb;
    for ( ; track; track = track->next )
	{
	    hPrintf("    <li>%s</li>\n", track->track);
	}
    hPrintf("    </ul>\n");
    }
}

static void assemblySettings(struct trackHubGenome *thg)
{
hPrintf("    <ul>\n");
struct hashEl *hel;
struct hashCookie hc = hashFirst(thg->settingsHash);
while ((hel = hashNext(&hc)) != NULL)
    {
    hPrintf("    <li>%s : %s</li>\n", hel->name, (char *)hel->val);
    if (sameWord("trackDb", hel->name))
	{
        struct trackDb *tdb = trackHubTracksForGenome(thg->trackHub, thg);
	trackList(tdb);
        }
    }
hPrintf("    </ul>\n");
}

static void genomeList (struct trackHub *hubTop)
{
struct trackHubGenome *thg = hubTop->genomeList;

hPrintf("<h4>genome sequences (and tracks) present in this track hub</h4>\n");
hPrintf("<ul>\n");
for ( ; thg; thg = thg->next )
    {
    hPrintf("<li>%s - %s - %s</li>\n", thg->organism, thg->name, thg->description);
    if (thg->settingsHash)
	assemblySettings(thg);
    }
hPrintf("</ul>\n");
}

void doMiddle(struct cart *theCart)
/* Set up globals and make web page */
{
cart = theCart;
char *database = NULL;
char *genome = NULL;
char *url = "https://genome-test.gi.ucsc.edu/~hiram/hubs/Plants/hub.txt";
getDbAndGenome(cart, &database, &genome, oldVars);
initGenbankTableNames(database);

char *docRoot = cfgOptionDefault("browser.documentRoot", DOCUMENT_ROOT);
// fprintf(stderr, "# DBG: docRoot: '%s'\n", docRoot);

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

char *pathInfo = getenv("PATH_INFO");

if ((NULL == pathInfo) || strlen(pathInfo) < 1) {
   pathInfo = cloneString("noPathInfo");
}
// fprintf(stderr, "# DBG: pathInfo: '%s'\n", pathInfo);

cartWebStart(cart, database, "access mechanism to hub data resources");


char *urlInput = cartUsualString(cart, "urlHub", url);
struct trackHub *hub = trackHubOpen(urlInput, "");
hPrintf("<form action='%s' name='hubApiUrl' id='hubApiUrl' method='GET'\n\n", "../cgi-bin/hubApi");
hPrintf("<br>Enter hub URL:&nbsp;");
hPrintf("<input type='text' name='urlHub' id='urlHub' size='60' value='%s'>\n", urlInput);
hWrites("&nbsp;");
// hPrintf("<em>after box</em>");
hButton("goApi", "go");
hPrintf("<br>\n</form>\n");

hPrintf("<p>URL: %s<br>\n", urlInput);
hPrintf("name: %s<br>\n", hub->shortLabel);
hPrintf("description: %s<br>\n", hub->longLabel);
hPrintf("default db: '%s'<br>\n", isEmpty(hub->defaultDb) ? "(none available)" : hub->defaultDb);
printf("pathInfo:'%s'<br>\ndocRoot:'%s'<br>\n", pathInfo, docRoot);

if (hub->genomeList)
    genomeList(hub);

hPrintf("<h2>cart dump</h2>");
hPrintf("<pre>\n");
cartDump(cart);
hPrintf("</pre>\n");
hPrintf("</p>\n");
cartWebEnd();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShell(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
