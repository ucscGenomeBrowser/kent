/* hubConnect - stuff to manage connections to track hubs.  Most of this is mediated through
 * the hubStatus table in the hgcentral database.  Here there are routines to translate between
 * hub symbolic names and hub URLs,  to see if a hub is up or down or sideways (up but badly
 * formatted) etc.  Note that there is no C structure corresponding to a row in the hubStatus 
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
#include "hgConfig.h"


boolean isHubTrack(char *trackName)
/* Return TRUE if it's a hub track. */
{
return startsWith(hubTrackPrefix, trackName);
}

static char *hubStatusTableName = NULL;

static char *getHubStatusTableName()
/* return the hubStatus table name from the environment, 
 * or hg.conf, or use the default.  Cache the result */
{
if (hubStatusTableName == NULL)
    hubStatusTableName = cfgOptionEnvDefault("HGDB_HUB_STATUS_TABLE",
	    hubStatusTableConfVariable, defaultHubStatusTableName);

return hubStatusTableName;
}

boolean hubConnectTableExists()
/* Return TRUE if the hubPublic table exists. */
{
struct sqlConnection *conn = hConnectCentral();
boolean exists = sqlTableExists(conn, getHubStatusTableName());
hDisconnectCentral(&conn);
return exists;
}

void hubConnectStatusFree(struct hubConnectStatus **pHub)
/* Free hubConnectStatus */
{
struct hubConnectStatus *hub = *pHub;
if (hub != NULL)
    {
    freeMem(hub->hubUrl);
    freeMem(hub->errorMessage);
    trackHubClose(&hub->trackHub);
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

boolean trackHubHasDatabase(struct trackHub *hub, char *database) 
/* Return TRUE if hub has contents for database */
{
if (hub != NULL)
    {
    struct trackHubGenome *genomes = hub->genomeList;	/* List of associated genomes. */

    for(; genomes; genomes = genomes->next)
	if (sameString(genomes->name, database))
	    return TRUE;
    }
return FALSE;
}

static struct trackHub *fetchHub(char *url, char **errorMessage)
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *tHub = NULL;
boolean gotWarning = FALSE;

if (errCatchStart(errCatch))
    tHub = trackHubOpen(url, "1"); // open hub.. it'll get renamed later
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    gotWarning = TRUE;
    *errorMessage = cloneString(errCatch->message->string);
    }
errCatchFree(&errCatch);

if (gotWarning)
    {
    return NULL;
    }

return tHub;
}

/* Given a hub ID return associated status. Returns NULL if no such hub.  If hub
 * exists but has problems will return with errorMessage field filled in. */
struct hubConnectStatus *hubConnectStatusForId(struct sqlConnection *conn, int id)
{
struct hubConnectStatus *hub = NULL;
char query[1024];
safef(query, sizeof(query), 
    "select hubUrl,status, errorMessage from %s where id=%d", getHubStatusTableName(), id);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
if (row != NULL)
    {
    AllocVar(hub);
    hub->id = id;
    hub->hubUrl = cloneString(row[0]);
    hub->status = sqlUnsigned(row[1]);
    hub->errorMessage = cloneString(row[2]);

    if (isEmpty(row[2]))
	{
	char *errorMessage = NULL;
	hub->trackHub = fetchHub( hub->hubUrl, &errorMessage);
	if (errorMessage != NULL)
	    {
	    hub->errorMessage = cloneString(errorMessage);
	    warn("%s", hub->errorMessage);
	    hubUpdateStatus( hub->errorMessage, hub);
	    }
	}
    }
sqlFreeResult(&sr);
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
if(!startsWith("hub_", trackName))
    return trackName;

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
    errAbort("%s", status->errorMessage);
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

static void addOneDescription(char *trackDbFile, struct trackDb *tdb)
/* Fetch tdb->track's html description and store in tdb->html. */
{
/* html setting should always be set because we set it at load time */
char *htmlName = trackDbSetting(tdb, "html");
assert(htmlName != NULL);

char *simpleName = hubConnectSkipHubPrefix(htmlName);
char *url = trackHubRelativeUrl(trackDbFile, simpleName);
char buffer[10*1024];
safef(buffer, sizeof buffer, "%s.html", url);
tdb->html = netReadTextFileIfExists(buffer);
freez(&url);
}

static void addDescription(char *trackDbFile, struct trackDb *tdb)
/* Fetch tdb->track's html description (or nearest ancestor's non-empty description)
 * and store in tdb->html. */
{
addOneDescription(trackDbFile, tdb);
if (isEmpty(tdb->html))
    {
    struct trackDb *parent;
    for (parent = tdb->parent;  isEmpty(tdb->html) && parent != NULL;  parent = parent->parent)
	{
	addOneDescription(trackDbFile, parent);
	if (isNotEmpty(parent->html))
	    tdb->html = cloneString(parent->html);
	}
    }
}

void hubConnectAddDescription(char *database, struct trackDb *tdb)
/* Fetch tdb->track's html description (or nearest ancestor's non-empty description)
 * and store in tdb->html. */
{
unsigned hubId = hubIdFromTrackName(tdb->track);
struct trackHub *hub = trackHubFromId(hubId);
struct trackHubGenome *hubGenome = trackHubFindGenome(hub, database);
trackHubPolishTrackNames(hub, tdb);
addDescription(hubGenome->trackDbFile, tdb);
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
trackHubPolishTrackNames(hub, tdbList);
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
    addDescription(hubGenome->trackDbFile, parent);
trackHubClose(&hub);

return tdb;
}

static char *getDbList(struct trackHub *tHub, int *pCount)
/* calculate dbList for hubStatus table from trackHub */
{
struct hashEl *hel;
struct dyString *dy = newDyString(1024);
struct hashCookie cookie = hashFirst(tHub->genomeHash);
int dbCount = 0;
while ((hel = hashNext(&cookie)) != NULL)
    {
    dbCount++;
    dyStringPrintf(dy,"%s,", hel->name);
    }
*pCount = dbCount;

return dy->string;
}

static void insertHubUrlInStatus(char *url)
/* add a url to the hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];
char *statusTable = getHubStatusTableName();

if (sqlFieldIndex(conn, statusTable, "firstAdded") >= 0)
    safef(query, sizeof(query), "insert into %s (hubUrl,firstAdded) values (\"%s\",now())",
	statusTable, url);
else
    safef(query, sizeof(query), "insert into %s (hubUrl) values (\"%s\")",
	statusTable, url);
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

char *statusTableName = getHubStatusTableName();
safef(query, sizeof(query), "select id,errorMessage from %s where hubUrl = \"%s\"", statusTableName, url);

struct sqlResult *sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    if (foundOne)
	errAbort("more than one line in %s with hubUrl %s\n", 
	    statusTableName, url);

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

static void getAndSetHubStatus(char *database, struct cart *cart, char *url, 
    boolean set)
/* make sure url is in hubStatus table, fetch the hub to get latest
 * labels and db information.
 * Set the cart variable to turn the hub on if set == TRUE.  
 */
{
char *errorMessage = NULL;
unsigned id;

/* first see if url is in hubStatus table */
if ((id = getHubId(url, &errorMessage)) == 0)
    {
    /* the url is not in the hubStatus table, add it */
    insertHubUrlInStatus(url);
    if ((id = getHubId(url, &errorMessage)) == 0)
	{
	errAbort("opened hub, but could not get it out of the hubStatus table");
	}
    }

/* allocate a hub */
struct hubConnectStatus *hub = NULL;

AllocVar(hub);
hub->id = id;
hub->hubUrl = cloneString(url);

/* new fetch the contents of the hub to fill in the status table */
struct trackHub *tHub = fetchHub( url, &errorMessage);
if (tHub != NULL)
    hub->trackHub = tHub;

/* update the status table with the lastest label and database information */
hubUpdateStatus( errorMessage, hub);

/* if we're turning on the hub, set the cart variable */
if (set)
    {
    char hubName[32];
    safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, id);
    cartSetString(cart, hubName, "1");
    }

hubConnectStatusFree(&hub);
}

unsigned hubFindOrAddUrlInStatusTable(char *database, struct cart *cart,
    char *url, char **errorMessage)
/* find this url in the status table, and return its id and errorMessage (if an errorMessage exists) */
{
int id = 0;

*errorMessage = NULL;

if ((id = getHubId(url, errorMessage)) > 0)
    return id;

getAndSetHubStatus(database, cart, url, FALSE);

if ((id = getHubId(url, errorMessage)) == 0)
    errAbort("inserted new hubUrl %s, but cannot find it", url);

return id;
}

void hubCheckForNew(char *database, struct cart *cart)
/* see if the user just typed in a new hub url, return id if so */
{
char *url = cartOptionalString(cart, hgHubDataText);

if (url != NULL)
    {
    trimSpaces(url);
    getAndSetHubStatus(database, cart, url, TRUE);
    cartRemove(cart, hgHubDataText);
    }
}

unsigned hubResetError(char *url)
/* clear the error for this url in the hubStatus table,return the id */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

safef(query, sizeof(query), "select id from %s where hubUrl = \"%s\"", getHubStatusTableName(), url);
unsigned id = sqlQuickNum(conn, query);

if (id == 0)
    errAbort("could not find url %s in status table (%s)\n", 
	url, getHubStatusTableName());

safef(query, sizeof(query), "update %s set errorMessage=\"\" where hubUrl = \"%s\"", getHubStatusTableName(), url);

sqlUpdate(conn, query);
hDisconnectCentral(&conn);

return id;
}

unsigned hubClearStatus(char *url)
/* drop the information about this url from the hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

safef(query, sizeof(query), "select id from %s where hubUrl = \"%s\"", getHubStatusTableName(), url);
unsigned id = sqlQuickNum(conn, query);

if (id == 0)
    errAbort("could not find url %s in status table (%s)\n", 
	url, getHubStatusTableName());

safef(query, sizeof(query), "delete from %s where hubUrl = \"%s\"", getHubStatusTableName(), url);

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

void hubUpdateStatus(char *errorMessage, struct hubConnectStatus *hub)
/* set the error message in the hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();
char query[4096];
struct trackHub *tHub = hub->trackHub;

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
	getHubStatusTableName(), buffer, hub->id);
    sqlUpdate(conn, query);
    }
else if (tHub != NULL)
    {
    int dbCount = 0;
    char *dbList = getDbList(tHub, &dbCount);

    safef(query, sizeof(query),
	"update %s set shortLabel=\"%s\",longLabel=\"%s\",dbCount=\"%d\",dbList=\"%s\",errorMessage=\"\",lastOkTime=now() where id=%d",
	getHubStatusTableName(), tHub->shortLabel, tHub->shortLabel, 
	dbCount, dbList,
	hub->id);
    sqlUpdate(conn, query);
    }
hDisconnectCentral(&conn);
}

struct trackDb *hubAddTracks(struct hubConnectStatus *hub, char *database,
	struct trackHub **pHubList)
/* Load up stuff from data hub and append to list. The hubUrl points to
 * a trackDb.ra format file.  */
{
/* Load trackDb.ra file and make it into proper trackDb tree */
struct trackDb *tdbList = NULL;
struct trackHub *trackHub = hub->trackHub;

if (trackHub != NULL)
    {
    char hubName[64];
    safef(hubName, sizeof(hubName), "hub_%d", hub->id);
    trackHub->name = cloneString(hubName);

    struct trackHubGenome *hubGenome = trackHubFindGenome(trackHub, database);
    if (hubGenome != NULL)
	{
	tdbList = trackHubTracksForGenome(trackHub, hubGenome);
	tdbList = trackDbLinkUpGenerations(tdbList);
	tdbList = trackDbPolishAfterLinkup(tdbList, database);
	trackDbPrioritizeContainerItems(tdbList);
	trackHubPolishTrackNames(trackHub, tdbList);
	if (tdbList != NULL)
	    slAddHead(pHubList, trackHub);
	}
    }
return tdbList;
}
