/* hgFetch - Fetch and provide remote content associated with hubs for UIs running
afoul of CORS restrictions. */
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
#include "hubConnect.h"
#include "filePath.h"
#include "trackDb.h"
#include "hdb.h"
#include "customTrack.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

/* Setting names whose file contents are safe to serve via hgFetch.
 * Only admin-configured (native track) values are checked -- never hub or custom tracks.
 * Do NOT add bigDataUrl or bigDataIndex here -- those may be restricted (we
 * might change this later to instead respect the tableBrowser setting in trackDb). */
static char *fetchableSettings[] = {"metaDataUrl", "colorSettingsUrl", NULL};

boolean fileUrlMatchesHub(char *fileUrl, struct hubConnectStatus *hubStatus)
/* Ignores fetchableSettings for now, whitelisting anything that sits inside
 * the hub.txt directory structure.  Assumes fileUrl has been canonicalized. */
{
char baseDir[2048];
splitPath(hubStatus->hubUrl, baseDir, NULL, NULL);
return startsWith(baseDir, fileUrl);
}

static boolean fileUrlMatchesTrackSetting(char *fileUrl, struct trackDb *tdb)
/* Check if fileUrl matches any whitelisted setting in this trackDb.
 * Assumes fileUrl has been canonicalized. */
{
char **p;
for (p = fetchableSettings; *p != NULL; p++)
    {
    char *val = trackDbSetting(tdb, *p);
    if (val != NULL && sameString(val, fileUrl))
        return TRUE;
    }
return FALSE;
}

void doMiddle(struct cart *theCart)
/* Set up globals, do sanity checks, and generate response */
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

char *fileUrl = cartOptionalString(cart, "fileUrl");
if (fileUrl == NULL)
    {
    puts("Status: 400 Bad Request");
    errAbort("Missing required parameter: fileUrl");
    }

char *urlClone = cloneString(fileUrl);
cgiDecode(urlClone, urlClone, strlen(urlClone));
fileUrl = resolveDotDots(urlClone);
freeMem(urlClone);

boolean matchFound = FALSE;

// Check if fileUrl falls under a connected hub's base directory
struct slName *hubIds = hubConnectHubsInCart(cart);
struct slName *thisHubId = hubIds;
while (thisHubId != NULL)
    {
    struct hubConnectStatus *hubStatus = hubFromId(sqlUnsigned(thisHubId->name));
    if (fileUrlMatchesHub(fileUrl, hubStatus))
        {
        matchFound = TRUE;
        break;
        }
    thisHubId = thisHubId->next;
    }

// For native database tracks (not hub or custom tracks), check if fileUrl matches
// a whitelisted trackDb setting.  Only native tracks are checked here because their
// settings are admin-configured and trusted.  Hub and custom track settings are
// user-controlled and could be used for SSRF attacks.
if (!matchFound)
    {
    char *track = cartOptionalString(cart, "track");
    char *sourceDb = cartOptionalString(cart, "sourceDb"); // for future quickLift use
    if (sourceDb == NULL)
        sourceDb = database;
    if (track != NULL && !isHubTrack(track) && !isCustomTrack(track))
        {
        struct trackDb *tdb = tdbForTrack(sourceDb, track, NULL);
        if (tdb != NULL)
            matchFound = fileUrlMatchesTrackSetting(fileUrl, tdb);
        }
    }

if (!matchFound)
    {
    puts("Status: 400 Bad Request");
    errAbort("Supplied fileUrl does not match any connected hubs or track settings.");
    }

// By now we know that fileUrl points to something valid to fetch and return to the user.
// Now we just have to fetch the file contents and retransmit it.
puts("Content-Type: text/plain\n\n");  // Hacky, but functional for now

char *content = udcFileReadAll(fileUrl, NULL, 0, NULL);
puts(content);
freeMem(content);
freeMem(fileUrl);
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "fileUrl", "track", "sourceDb", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
