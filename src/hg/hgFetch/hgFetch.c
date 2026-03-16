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

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars = NULL;

boolean fileUrlMatchesHub(char *fileUrl, struct hubConnectStatus *hubStatus)
{
char baseDir[2048];
splitPath(hubStatus->hubUrl, baseDir, NULL, NULL);
return startsWith(baseDir, fileUrl);
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

cgiDecode(fileUrl, fileUrl, strlen(fileUrl));
boolean matchFound = FALSE;

// Next task: Need to first check local dirs for genark hubs, valid local tracks
// 

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
if (!matchFound)
    {
    puts("Status: 400 Bad Request");
    errAbort("Supplied fileUrl does not match any connected hubs.");
    }

// By now we know that fileUrl points to something valid to fetch and return to the user.
// Now we just have to fetch the file contents and retransmit it.
puts("Content-Type: text/plain\n\n");  // Hacky, but functional for now

char *content = udcFileReadAll(fileUrl, NULL, 0, NULL);
puts(content);
freeMem(content);
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "fileUrl", NULL,};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
cartEmptyShellNoContent(doMiddle, hUserCookie(), excludeVars, oldVars);
return 0;
}
