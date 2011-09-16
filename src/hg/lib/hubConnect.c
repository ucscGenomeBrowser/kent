/* hubConnect - stuff to manage connections to track hubs.  Most of this is mediated through
 * the hubConnect table in the hgCentral database.  Here there are routines to translate between
 * hub symbolic names and hub URLs,  to see if a hub is up or down or sideways (up but badly
 * formatted) etc.  Note that there is no C structure corresponding to a row in the hubConnect 
 * table by design.  We just want field-by-field access to this. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "sqlNum.h"
#include "jksql.h"
#include "hdb.h"
#include "net.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "hui.h"
#include "errCatch.h"
#include "obscure.h"


boolean isHubTrack(char *trackName)
/* Return TRUE if it's a hub track. */
{
return startsWith(hubTrackPrefix, trackName);
}

boolean hubConnectTableExists()
/* Return TRUE if the hubConnect table exists. */
{
struct sqlConnection *conn = hConnectCentral();
boolean exists = sqlTableExists(conn, hubPublicTableName);
hDisconnectCentral(&conn);
return exists;
}

void hubConnectStatusFree(struct hubConnectStatus **pHub)
/* Free hubConnectStatus */
{
struct hubConnectStatus *hub = *pHub;
if (hub != NULL)
    {
    freeMem(hub->shortLabel);
    freeMem(hub->longLabel);
    freeMem(hub->hubUrl);
    freeMem(hub->errorMessage);
    if (hub->dbArray)
        {
	freeMem(hub->dbArray[0]);
	freeMem(hub->dbArray);
	}
    freez(pHub);
    }
}

void hubConnectStatusFreeList(struct hubConnectStatus **pList)
/* Free a list of dynamically allocated hubConnectStatus's */
{
struct hubConnectStatus *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    hubConnectStatusFree(&el);
    }
*pList = NULL;
}

static void hubConnectRemakeTrackHubVar(struct cart *cart)
/* Remake trackHub cart variable if need be from various check box vars. */
{
if (cartVarExists(cart, hgHubConnectRemakeTrackHub))
    {
    struct slPair *hubVarList = cartVarsWithPrefix(cart, hgHubConnectHubVarPrefix);
    int prefixLength = strlen(hgHubConnectHubVarPrefix);
    struct dyString *trackHubs = dyStringNew(0);
    struct slPair *hubVar;
    boolean firstOne = TRUE;
    for (hubVar = hubVarList; hubVar != NULL; hubVar = hubVar->next)
        {
	if (cartBoolean(cart, hubVar->name))
	    {
	    if (firstOne)
		firstOne = FALSE;
	    else
		dyStringAppendC(trackHubs, ' ');
	    dyStringAppend(trackHubs, hubVar->name + prefixLength);
	    }
	}
    slPairFreeList(&hubVarList);
    cartSetString(cart, hubConnectTrackHubsVarName, trackHubs->string);
    dyStringFree(&trackHubs);
    cartRemove(cart, hgHubConnectRemakeTrackHub);
    }
}

struct slName *hubConnectHubsInCart(struct cart *cart)
/* Return list of track hub ids that are turned on. */
{
hubConnectRemakeTrackHubVar(cart);
char *trackHubString = cartOptionalString(cart, hubConnectTrackHubsVarName);
return slNameListFromString(trackHubString, ' ');
}

static struct hubConnectStatus *hubConnectStatusForIdDb(struct sqlConnection *conn, int id)
/* Given a hub ID return associated status. Returns NULL if no such hub.  If hub
 * exists but has problems will return with errorMessage field filled in. */
{
struct hubConnectStatus *hub = NULL;
char query[1024];
safef(query, sizeof(query), 
    "select id,shortLabel,longLabel,hubUrl,errorMessage,dbCount,dbList,status from %s where id=%d",
    hubStatusTableName, id);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
if (row != NULL)
    {
    AllocVar(hub);
    hub->id = sqlUnsigned(row[0]);
    hub->shortLabel = cloneString(row[1]);
    hub->longLabel = cloneString(row[2]);
    hub->hubUrl = cloneString(row[3]);
    hub->errorMessage = cloneString(row[4]);
    hub->dbCount = sqlUnsigned(row[5]);
    int sizeOne;
    sqlStringDynamicArray(row[6], &hub->dbArray, &sizeOne);
    assert(sizeOne == hub->dbCount);
    hub->status = sqlUnsigned(row[7]);
    }
sqlFreeResult(&sr);
return hub;
}

boolean isHubUnlisted(struct hubConnectStatus *hub) 
/* Return TRUE if it's an unlisted hub */
{
    return (hub->status & HUB_UNLISTED);
}

struct hubConnectStatus *hubConnectStatusForId(struct sqlConnection *conn, int id)
/* Given a hub ID return associated status. Returns NULL if no such hub.  If hub
 * exists but has problems will return with errorMessage field filled in. */
 /* If the id is negative, then the hub is private and the number is the
  * offset into the private hubfile in the trash */
{
struct hubConnectStatus *hub = NULL;

hub = hubConnectStatusForIdDb(conn, id);

return hub;
}


struct hubConnectStatus *hubConnectStatusListFromCartAll(struct cart *cart)
/* Return list of all track hubs that are referenced by cart. */
{
struct hubConnectStatus *hubList = NULL, *hub;
struct slPair *pair, *pairList = cartVarsWithPrefix(cart, hgHubConnectHubVarPrefix);
struct sqlConnection *conn = hConnectCentral();
for (pair = pairList; pair != NULL; pair = pair->next)
    {
    int id = hubIdFromCartName(pair->name);
    hub = hubConnectStatusForId(conn, id);
    if (hub != NULL)
	{
        slAddHead(&hubList, hub);
	}
    }
slFreeList(&pairList);
hDisconnectCentral(&conn);
slReverse(&hubList);
return hubList;
}

struct hubConnectStatus *hubConnectStatusListFromCart(struct cart *cart)
/* Return list of track hubs that are turned on by user in cart. */
{
struct hubConnectStatus *hubList = NULL, *hub;
struct slName *name, *nameList = hubConnectHubsInCart(cart);
struct sqlConnection *conn = hConnectCentral();
for (name = nameList; name != NULL; name = name->next)
    {
    int id = sqlSigned(name->name);
    hub = hubConnectStatusForId(conn, id);
    if (hub != NULL)
	{
        slAddHead(&hubList, hub);
	}
    }
slFreeList(&nameList);
hDisconnectCentral(&conn);
slReverse(&hubList);
return hubList;
}

int hubIdFromCartName(char *cartName)
/* Given something like "hgHubConnect.hub.123" return 123 */
{
assert(startsWith("hgHubConnect.hub.", cartName));

char *ptr1 = strchr(cartName, '.');
char *ptr2 = strchr(ptr1 + 1, '.');

return sqlUnsigned(ptr2+1);
}

unsigned hubIdFromTrackName(char *trackName)
/* Given something like "hub_123_myWig" return 123 */
{
assert(startsWith("hub_", trackName));
char *ptr1 = trackName;
ptr1 += 4;
char *ptr2 = strchr(ptr1, '_');

if (ptr2 == NULL)
    errAbort("hub track %s not in correct format\n", trackName);
char save = *ptr2;
*ptr2 = 0;
unsigned val = sqlUnsigned(ptr1);
*ptr2 = save;
return  val;
}

char *hubConnectSkipHubPrefix(char *trackName)
/* Given something like "hub_123_myWig" return myWig.  Don't free this, it's not allocated */
{
assert(startsWith("hub_", trackName));
trackName += 4;
trackName = strchr(trackName, '_');
assert(trackName != NULL);
return trackName + 1;
}

struct trackHub *trackHubFromId(unsigned hubId)
/* Given a hub ID number, return corresponding trackHub structure. 
 * ErrAbort if there's a problem. */
{
struct sqlConnection *conn = hConnectCentral();
struct hubConnectStatus *status = hubConnectStatusForId(conn, hubId);
hDisconnectCentral(&conn);
if (status == NULL)
    errAbort("The hubId %d was not found", hubId);
if (!isEmpty(status->errorMessage))
    errAbort("Hub %s at %s has the error: %s", status->shortLabel, 
	    status->hubUrl, status->errorMessage);
char hubName[16];
safef(hubName, sizeof(hubName), "hub_%u", hubId);
struct trackHub *hub = trackHubOpen(status->hubUrl, hubName);
hubConnectStatusFree(&status);
return hub;
}

static struct trackDb *findSuperTrack(struct trackDb *tdbList, char *trackName)
/*  discover any supertracks, and if there are some add them 
 *  to the subtrack list of the supertrack */
{
struct trackDb *tdb;
struct trackDb *p = NULL;
struct trackDb *next;

for(tdb = tdbList; tdb; tdb = next)
    {
    /* save away the next pointer becuase we may detach this node and
     * add it to its supertrack parent */
    next = tdb->next;
    if (tdb->parent != NULL && sameString(trackName, tdb->parent->track))
	{
	/* found a supertrack with the right name, add this child */
	p = tdb->parent;
	slAddHead(&p->subtracks, tdb);
	}
    }

return p;
}

struct trackDb *hubConnectAddHubForTrackAndFindTdb( char *database, 
    char *trackName, struct trackDb **pTdbList, struct hash *trackHash)
/* Go find hub for trackName (which will begin with hub_), and load the tracks
 * for it, appending to end of list and adding to trackHash.  Return the
 * trackDb associated with trackName. This will also fill in the html fields,
 * but just for that track and it's parents. */ 
{
unsigned hubId = hubIdFromTrackName(trackName);
struct trackHub *hub = trackHubFromId(hubId);
struct trackHubGenome *hubGenome = trackHubFindGenome(hub, database);
struct trackDb *tdbList = trackHubTracksForGenome(hub, hubGenome);
tdbList = trackDbLinkUpGenerations(tdbList);
tdbList = trackDbPolishAfterLinkup(tdbList, database);
rAddTrackListToHash(trackHash, tdbList, NULL, FALSE);
if (pTdbList != NULL)
    *pTdbList = slCat(*pTdbList, tdbList);
struct trackDb *tdb = hashFindVal(trackHash, trackName);
if (tdb == NULL) 
    // superTracks aren't in the hash... look in tdbList
    tdb = findSuperTrack(tdbList, trackName);

if (tdb == NULL) 
    errAbort("Can't find track %s in %s", trackName, hub->url);

/* Add html for track and parents. */
/* Note: this does NOT add the HTML for supertrack kids */
struct trackDb *parent;
for (parent = tdb; parent != NULL; parent = parent->parent)
    {
    char *simpleName = hubConnectSkipHubPrefix(tdb->track);
    char *url = trackHubRelativeUrl(hubGenome->trackDbFile, simpleName);
    char buffer[10*1024];
    safef(buffer, sizeof buffer, "%s.html", url);

    parent->html = netReadTextFileIfExists(buffer);
    freez(&url);
    }
trackHubClose(&hub);

return tdb;
}

static void enterHubInStatus(struct trackHub *tHub, boolean unlisted)
/* put the hub status in the hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();

/* calculate dbList */
struct dyString *dy = newDyString(1024);
struct hashEl *hel;
struct hashCookie cookie = hashFirst(tHub->genomeHash);
int dbCount = 0;

while ((hel = hashNext(&cookie)) != NULL)
    {
    dbCount++;
    dyStringPrintf(dy,"%s,", hel->name);
    }


char query[512];
safef(query, sizeof(query), "insert into %s (hubUrl,status,shortLabel, longLabel, dbList, dbCount) values (\"%s\",%d,\"%s\",\"%s\", \"%s\", %d)",
    hubStatusTableName, tHub->url, unlisted ? 1 : 0,
    tHub->shortLabel, tHub->longLabel,
    dy->string, dbCount);
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}

static unsigned getHubId(char *url, char **errorMessage)
/* find id for url in hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
char **row;
boolean foundOne = FALSE;
int id = 0;

safef(query, sizeof(query), "select id,errorMessage from %s where hubUrl = \"%s\"", hubStatusTableName, url);

struct sqlResult *sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    if (foundOne)
	errAbort("more than one line in %s with hubUrl %s\n", 
	    hubStatusTableName, url);

    foundOne = TRUE;

    char *thisId = row[0], *thisError = row[1];

    if (!isEmpty(thisError))
	*errorMessage = cloneString(thisError);

    id = sqlUnsigned(thisId);
    }
sqlFreeResult(&sr);

hDisconnectCentral(&conn);

return id;
}

static boolean hubHasDatabase(unsigned id, char *database)
/* check to see if hub specified by id supports database */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

safef(query, sizeof(query), "select dbList from %s where id=%d", 
    hubStatusTableName, id); 
char *dbList = sqlQuickString(conn, query);
boolean gotIt = FALSE;

if (nameInCommaList(database, dbList))
    gotIt = TRUE;

hDisconnectCentral(&conn);

freeMem(dbList);

return gotIt;
}

static boolean fetchHub(char *database, char *url, boolean unlisted)
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *tHub = NULL;
boolean gotWarning = FALSE;
unsigned id = 0;

if (errCatchStart(errCatch))
    tHub = trackHubOpen(url, "1"); // open hub.. it'll get renamed later
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    gotWarning = TRUE;
    warn("%s", errCatch->message->string);
    }
errCatchFree(&errCatch);

if (gotWarning)
    {
    return 0;
    }

if (hashLookup(tHub->genomeHash, database) != NULL)
    {
    enterHubInStatus(tHub, unlisted);
    }
else
    {
    warn("requested hub at %s does not have data for %s\n", url, database);
    return 0;
    }

trackHubClose(&tHub);

char *errorMessage = NULL;
id = getHubId(url, &errorMessage);
return id;
}

static unsigned getAndSetHubStatus(char *database, struct cart *cart, char *url, 
    boolean set, boolean unlisted)
/* look in the hubStatus table for this url, add it if it isn't in there
 * Set the cart variable to turn the hub on if set == TRUE.  
 * Return id from that status table*/
{
char *errorMessage = NULL;
unsigned id;

if ((id = getHubId(url, &errorMessage)) == 0)
    {
    if ((id = fetchHub(database, url, unlisted)) == 0)
	return id;
    }
else if (!hubHasDatabase(id, database))
    {
    warn("requested hub at %s does not have data for %s\n", url, database);
    return id;
    }

char hubName[32];
safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, id);
if (set)
    cartSetString(cart, hubName, "1");

return id;
}

unsigned hubFindOrAddUrlInStatusTable(char *database, struct cart *cart,
    char *url, char **errorMessage)
/* find this url in the status table, and return its id and errorMessage (if an errorMessage exists) */
{
int id = 0;

*errorMessage = NULL;

if ((id = getHubId(url, errorMessage)) > 0)
    return id;

getAndSetHubStatus(database, cart, url, FALSE, FALSE);

if ((id = getHubId(url, errorMessage)) == 0)
    errAbort("inserted new hubUrl %s, but cannot find it", url);

return id;
}

unsigned hubCheckForNew(char *database, struct cart *cart)
/* see if the user just typed in a new hub url, return id if so */
{
char *url = cartOptionalString(cart, hgHubDataText);

if (url != NULL)
    {
    trimSpaces(url);
    unsigned id = getAndSetHubStatus(database, cart, url, TRUE, TRUE);
    cartRemove(cart, hgHubDataText);
    return id;
    }
return 0;
}

unsigned hubResetError(char *url)
/* clear the error for this url in the hubStatus table,return the id */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

safef(query, sizeof(query), "select id from %s where hubUrl = \"%s\"", hubStatusTableName, url);
unsigned id = sqlQuickNum(conn, query);

if (id == 0)
    errAbort("could not find url %s in status table (%s)\n", 
	url, hubStatusTableName);

safef(query, sizeof(query), "update %s set errorMessage=\"\" where hubUrl = \"%s\"", hubStatusTableName, url);

sqlUpdate(conn, query);
hDisconnectCentral(&conn);

return id;
}

unsigned hubClearStatus(char *url)
/* drop the information about this url from the hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

safef(query, sizeof(query), "select id from %s where hubUrl = \"%s\"", hubStatusTableName, url);
unsigned id = sqlQuickNum(conn, query);

if (id == 0)
    errAbort("could not find url %s in status table (%s)\n", 
	url, hubStatusTableName);

safef(query, sizeof(query), "delete from %s where hubUrl = \"%s\"", hubStatusTableName, url);

sqlUpdate(conn, query);
hDisconnectCentral(&conn);

return id;
}

void hubDisconnect(struct cart *cart, char *url)
/* drop the information about this url from the hubStatus table, and 
 * the cart variable the references this hub */
{
/* clear the hubStatus table */
unsigned id = hubClearStatus(url);

/* remove the cart variable */
char buffer[1024];
safef(buffer, sizeof buffer, "hgHubConnect.hub.%d", id);
cartRemove(cart, buffer);
}

void hubSetErrorMessage(char *errorMessage, unsigned id)
/* set the error message in the hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();
char query[4096];

if (errorMessage != NULL)
    {
    // make sure there is no newline at the end.  This should be unneccesary
    // but there are many, many places where newlines are added in calls
    // to warn and errAbort
    char buffer[4096];
    safecpy(buffer, sizeof buffer, errorMessage);
    while (lastChar(buffer) == '\n')
	buffer[strlen(buffer) - 1] = '\0';
    safef(query, sizeof(query),
	"update %s set errorMessage=\"%s\", lastNotOkTime=now() where id=%d",
	hubStatusTableName, buffer, id);
    }
else
    {
    safef(query, sizeof(query),
	"update %s set errorMessage=\"\", lastOkTime=now() where id=%d",
	hubStatusTableName, id);
    }
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}
