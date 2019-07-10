/* hubConnect - stuff to manage connections to track hubs.  Most of this is mediated through
 * the hubStatus table in the hgcentral database.  Here there are routines to translate between
 * hub symbolic names and hub URLs,  to see if a hub is up or down or sideways (up but badly
 * formatted) etc.  Note that there is no C structure corresponding to a row in the hubStatus 
 * table by design.  We just want field-by-field access to this. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "grp.h"
#include "udc.h"

boolean isHubTrack(char *trackName)
/* Return TRUE if it's a hub track. */
{
return startsWith(hubTrackPrefix, trackName);
}

static char *hubStatusTableName = NULL;
static char *_hubPublicTableName = NULL;

static char *getHubStatusTableName()
/* return the hubStatus table name from the environment, 
 * or hg.conf, or use the default.  Cache the result */
{
if (hubStatusTableName == NULL)
    hubStatusTableName = cfgOptionEnvDefault("HGDB_HUB_STATUS_TABLE",
	    hubStatusTableConfVariable, defaultHubStatusTableName);

return hubStatusTableName;
}

char *hubPublicTableName()
/* Get the name of the table that lists public hubs.  Don't free the result. */
{
if (_hubPublicTableName == NULL)
    _hubPublicTableName = cfgOptionEnvDefault("HGDB_HUB_PUBLIC_TABLE", hubPublicTableConfVariable,
                                             defaultHubPublicTableName);
return _hubPublicTableName;
}

boolean hubConnectTableExists()
/* Return TRUE if the hubStatus table exists. */
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

static struct trackHub *fetchHub(struct hubConnectStatus *hubStatus, char **errorMessage)
{
struct errCatch *errCatch = errCatchNew();
struct trackHub *tHub = NULL;
boolean gotWarning = FALSE;
char *url = hubStatus->hubUrl;

char hubName[64];
safef(hubName, sizeof(hubName), "hub_%d", hubStatus->id);

if (errCatchStart(errCatch))
    tHub = trackHubOpen(url, cloneString(hubName)); // open hub
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

static boolean
hubTimeToCheck(struct hubConnectStatus *hub, char *notOkStatus)
/* check to see if enough time has passed to re-check a hub that
 * has an error status.  Use udcTimeout as the length of time to wait.*/
{
time_t checkTime = udcCacheTimeout();
return dateIsOlderBy(notOkStatus, "%F %T", checkTime);
}

/* Given a hub ID return associated status. Returns NULL if no such hub.  If hub
 * exists but has problems will return with errorMessage field filled in. */
struct hubConnectStatus *hubConnectStatusForId(struct sqlConnection *conn, int id)
{
struct hubConnectStatus *hub = NULL;
char query[1024];
sqlSafef(query, sizeof(query), 
    "select hubUrl,status, errorMessage,lastNotOkTime, shortLabel from %s where id=%d", getHubStatusTableName(), id);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row = sqlNextRow(sr);
if (row != NULL)
    {
    AllocVar(hub);
    hub->id = id;
    hub->hubUrl = cloneString(row[0]);
    hub->status = sqlUnsigned(row[1]);
    hub->errorMessage = cloneString(row[2]);
    char *shortLabel = row[4];

    if (isEmpty(row[2]) || hubTimeToCheck(hub, row[3]))
	{
	char *errorMessage = NULL;
	hub->trackHub = fetchHub( hub, &errorMessage);
	hub->errorMessage = cloneString(errorMessage);
	hubUpdateStatus( hub->errorMessage, hub);
	if (!isEmpty(hub->errorMessage))
	    {
            boolean isCollection = (strstr(hub->hubUrl, "hgComposite") != NULL);
            if (isCollection)
                warn("Your Track Collections have been removed by our trash collectors.  If you'd like your Track Collections to stay on our servers, you need to save them in a session." );
            else
                warn("Could not connect to hub \"%s\": %s", shortLabel, hub->errorMessage);
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
    // is this hub turned connected??
    if (differentString(pair->val, "1"))
        continue;

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
	if (!isEmpty(hub->errorMessage) && (strstr(hub->hubUrl, "hgComposite") != NULL))
            {
            // custom collection hub has disappeared.   Remove it from cart
            cartSetString(cart, hgHubConnectRemakeTrackHub, "on");
            char buffer[1024];
            safef(buffer, sizeof buffer, "hgHubConnect.hub.%d", id);
            cartRemove(cart, buffer);
            }
        else
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

struct hubConnectStatus *hubFromId(unsigned hubId)
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
return status;
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


void hubConnectAddDescription(char *database, struct trackDb *tdb)
/* Fetch tdb->track's html description (or nearest ancestor's non-empty description)
 * and store in tdb->html. */
{
unsigned hubId = hubIdFromTrackName(tdb->track);
struct hubConnectStatus *hub = hubFromId(hubId);
struct trackHubGenome *hubGenome = trackHubFindGenome(hub->trackHub, database);
trackHubPolishTrackNames(hub->trackHub, tdb);
trackHubAddDescription(hubGenome->trackDbFile, tdb);
}

struct trackDb *hubConnectAddHubForTrackAndFindTdb( char *database, 
    char *trackName, struct trackDb **pTdbList, struct hash *trackHash)
/* Go find hub for trackName (which will begin with hub_), and load the tracks
 * for it, appending to end of list and adding to trackHash.  Return the
 * trackDb associated with trackName. This will also fill in the html fields,
 * but just for that track and it's parents. */ 
{
unsigned hubId = hubIdFromTrackName(trackName);
struct hubConnectStatus *hub = hubFromId(hubId);
struct trackHubGenome *hubGenome = trackHubFindGenome(hub->trackHub, database);
struct trackDb *tdbList = trackHubTracksForGenome(hub->trackHub, hubGenome);
tdbList = trackDbLinkUpGenerations(tdbList);
tdbList = trackDbPolishAfterLinkup(tdbList, database);
//this next line causes warns to print outside of warn box on hgTrackUi
//trackDbPrioritizeContainerItems(tdbList);
trackHubPolishTrackNames(hub->trackHub, tdbList);
char *fixTrackName = cloneString(trackName);
trackHubFixName(fixTrackName);
rAddTrackListToHash(trackHash, tdbList, NULL, FALSE);
if (pTdbList != NULL)
    *pTdbList = slCat(*pTdbList, tdbList);
struct trackDb *tdb = hashFindVal(trackHash, fixTrackName);
if (tdb == NULL) 
    // superTracks aren't in the hash... look in tdbList
    tdb = findSuperTrack(tdbList, fixTrackName);

if (tdb == NULL) 
    errAbort("Can't find track %s in %s", fixTrackName, hub->trackHub->url);

/* Add html for track and parents. */
/* Note: this does NOT add the HTML for supertrack kids */
struct trackDb *parent;
for (parent = tdb; parent != NULL; parent = parent->parent)
    trackHubAddDescription(hubGenome->trackDbFile, parent);

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
char query[4096];
char *statusTable = getHubStatusTableName();

if (sqlFieldIndex(conn, statusTable, "firstAdded") >= 0)
    sqlSafef(query, sizeof(query), "insert into %s (hubUrl,shortLabel,longLabel,dbCount,dbList,status,lastOkTime,lastNotOkTime,errorMessage,firstAdded) values (\"%s\",\"\",\"\",0,NULL,0,\"\",\"\",\"\",now())", statusTable, url);
else
    sqlSafef(query, sizeof(query), "insert into %s (hubUrl) values (\"%s\")",
	statusTable, url);
sqlUpdate(conn, query);
hDisconnectCentral(&conn);
}

static unsigned getHubId(char *url, char **errorMessage)
/* find id for url in hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();
char query[4096];
char **row;
boolean foundOne = FALSE;
int id = 0;

char *statusTableName = getHubStatusTableName();
sqlSafef(query, sizeof(query), "select id,errorMessage from %s where hubUrl = \"%s\"", statusTableName, url);

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

static struct hubConnectStatus *getAndSetHubStatus( struct cart *cart, char *url, 
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
struct trackHub *tHub = fetchHub( hub, &errorMessage);
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

return hub;
}

unsigned hubFindOrAddUrlInStatusTable(char *database, struct cart *cart,
    char *url, char **errorMessage)
/* find this url in the status table, and return its id and errorMessage (if an errorMessage exists) */
{
int id = 0;

*errorMessage = NULL;

if ((id = getHubId(url, errorMessage)) > 0)
    return id;

getAndSetHubStatus( cart, url, FALSE);

if ((id = getHubId(url, errorMessage)) == 0)
    errAbort("inserted new hubUrl %s, but cannot find it", url);

return id;
}

// global to hold hubUrl we added if any
static struct hubConnectStatus *gNewHub = NULL;

struct hubConnectStatus  *hubConnectNewHub()
/* return the hub for the hubUrl we added (if any) */
{
return gNewHub;
}

static void disconnectHubsSamePrefix(struct cart *cart, char *url)
/* disconnect all the hubs with the same prefix */
{
char *prefix = cloneString(url);
char *ptr = strrchr(prefix, '/');
if (ptr == NULL)
    return;
*ptr = 0;

char query[2048];
sqlSafef(query, sizeof(query), "select id from %s where hubUrl like  \"%s%%\"", getHubStatusTableName(), prefix);

struct sqlConnection *conn = hConnectCentral();
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    int id = sqlUnsigned(row[0]);
    char buffer[512];

    safef(buffer, sizeof(buffer), "hgHubConnect.hub.%d", id);
    cartRemove(cart, buffer);
    }
sqlFreeResult(&sr);

hDisconnectCentral(&conn);
}

static char  *checkForNew( struct cart *cart)
/* see if the user just typed in a new hub url, return id if so */
{
struct hubConnectStatus *hub;
char *url = cartOptionalString(cart, hgHubDataClearText);

if (url != NULL)
    disconnectHubsSamePrefix(cart, url);
else
    url = cartOptionalString(cart, hgHubDataText);

if (url == NULL)
    return NULL;

trimSpaces(url);

gNewHub = hub = getAndSetHubStatus( cart, url, TRUE);
    
cartRemove(cart, hgHubDataClearText);
cartRemove(cart, hgHubDataText);

char *wantFirstDb = cartOptionalString(cart, hgHubDoFirstDb);
char *newDatabase = NULL;
if ((wantFirstDb != NULL) && (hub->trackHub != NULL))
    newDatabase = hub->trackHub->defaultDb;
else 
    {
    // Check to see if the user specified an assembly within
    // an assembly hub.
    char *assemblyDb = cartOptionalString(cart, hgHubGenome);
    if (assemblyDb != NULL)
        {
        char buffer[512];

        safef(buffer, sizeof buffer, "hub_%d_%s",  hub->id, assemblyDb);
        newDatabase = cloneString(buffer);
        }
    }

cartRemove(cart, hgHubDoFirstDb);
cartRemove(cart, hgHubGenome);
return newDatabase;
}

unsigned hubResetError(char *url)
/* clear the error for this url in the hubStatus table,return the id */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

sqlSafef(query, sizeof(query), "select id from %s where hubUrl = \"%s\"", getHubStatusTableName(), url);
unsigned id = sqlQuickNum(conn, query);

if (id == 0)
    errAbort("could not find url %s in status table (%s)\n", 
	url, getHubStatusTableName());

sqlSafef(query, sizeof(query), "update %s set errorMessage=\"\" where hubUrl = \"%s\"", getHubStatusTableName(), url);

sqlUpdate(conn, query);
hDisconnectCentral(&conn);

return id;
}

unsigned hubClearStatus(char *url)
/* drop the information about this url from the hubStatus table */
{
struct sqlConnection *conn = hConnectCentral();
char query[512];

sqlSafef(query, sizeof(query), "select id from %s where hubUrl = \"%s\"", getHubStatusTableName(), url);
unsigned id = sqlQuickNum(conn, query);

if (id == 0)
    errAbort("could not find url %s in status table (%s)\n", 
	url, getHubStatusTableName());

sqlSafef(query, sizeof(query), "delete from %s where hubUrl = \"%s\"", getHubStatusTableName(), url);

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
char query[64 * 1024];
struct trackHub *tHub = NULL;

if (hub != NULL)
    tHub = hub->trackHub;

if (errorMessage != NULL)
    {
    // make sure there is no newline at the end.  This should be unneccesary
    // but there are many, many places where newlines are added in calls
    // to warn and errAbort
    char buffer[64 * 1024];
    safecpy(buffer, sizeof buffer, errorMessage);
    while (lastChar(buffer) == '\n')
	buffer[strlen(buffer) - 1] = '\0';
    sqlSafef(query, sizeof(query),
	"update %s set errorMessage=\"%s\", lastNotOkTime=now() where id=%d",
	getHubStatusTableName(), buffer, hub->id);
    sqlUpdate(conn, query);
    }
else if (tHub != NULL)
    {
    int dbCount = 0;
    char *dbList = getDbList(tHub, &dbCount);
    // users may include quotes in their hub names requiring escaping
    sqlSafef(query, sizeof(query),
	"update %s set shortLabel=\"%s\",longLabel=\"%s\",dbCount=\"%d\",dbList=\"%s\",errorMessage=\"\",lastOkTime=now() where id=%d",
	getHubStatusTableName(), tHub->shortLabel, tHub->longLabel, 
	dbCount, dbList,
	hub->id);
    sqlUpdate(conn, query);
    }
hDisconnectCentral(&conn);
}

struct trackDb *hubAddTracks(struct hubConnectStatus *hub, char *database)
/* Load up stuff from data hub and return list. */
{
/* Load trackDb.ra file and make it into proper trackDb tree */
struct trackDb *tdbList = NULL;
struct trackHub *trackHub = hub->trackHub;

if (trackHub != NULL)
    {
    struct trackHubGenome *hubGenome = trackHubFindGenome(trackHub, database);
    if (hubGenome != NULL)
	{
        boolean doCache = trackDbCacheOn();

        if (doCache)
            {
            // we have to open the trackDb file to get the udc cache to check for an update
            struct udcFile *checkCache = udcFileMayOpen(hubGenome->trackDbFile, NULL);
            time_t time = udcUpdateTime(checkCache);
            udcFileClose(&checkCache);

            struct trackDb *cacheTdb = trackDbHubCache(hubGenome->trackDbFile, time);

            if (cacheTdb != NULL)
                return cacheTdb;

            memCheckPoint(); // we want to know how much memory is used to build the tdbList
            }

        tdbList = trackHubTracksForGenome(trackHub, hubGenome);
        tdbList = trackDbLinkUpGenerations(tdbList);
        tdbList = trackDbPolishAfterLinkup(tdbList, database);
        trackDbPrioritizeContainerItems(tdbList);
        trackHubPolishTrackNames(trackHub, tdbList);

        if (doCache)
            trackDbHubCloneTdbListToSharedMem(hubGenome->trackDbFile, tdbList, memCheckPoint());
	}
    }
return tdbList;
}

static struct grp *grpFromHub(struct hubConnectStatus *hub)
/* Make up a grp structur from hub */
{
struct grp *grp;
AllocVar(grp);
char name[16];
safef(name, sizeof(name), "hub_%d", hub->id);
grp->name = cloneString(name);
grp->label = cloneString(hub->trackHub->shortLabel);
return grp;
}

struct trackDb *hubCollectTracks( char *database,  struct grp **pGroupList)
/* Generate trackDb structures for all the tracks in attached hubs.  
 * Make grp structures for each hub. Returned group list is reversed. */
{
// return the cached copy if it exists
static struct trackDb *hubTrackDbs;
static struct grp *hubGroups;

if (hubTrackDbs != NULL)
    {
    if (pGroupList != NULL)
	*pGroupList = hubGroups;
    return hubTrackDbs;
    }

struct hubConnectStatus *hub, *hubList =  hubConnectGetHubs();
struct trackDb *tdbList = NULL;
for (hub = hubList; hub != NULL; hub = hub->next)
    {
    if (isEmpty(hub->errorMessage))
	{
        /* error catching in so it won't just abort  */
        struct errCatch *errCatch = errCatchNew();
        if (errCatchStart(errCatch))
	    {
	    struct trackDb *thisList = hubAddTracks(hub, database);
	    tdbList = slCat(tdbList, thisList);
	    }
        errCatchEnd(errCatch);
        if (errCatch->gotError)
	    {
	    warn("%s", errCatch->message->string);
	    hubUpdateStatus( errCatch->message->string, hub);
	    }
	else
	    {
            struct grp *grp = grpFromHub(hub);
            slAddHead(&hubGroups, grp);
	    hubUpdateStatus(NULL, hub);
	    }

        errCatchFree(&errCatch);
	}
    }

hubTrackDbs = tdbList;
if (pGroupList != NULL)
    *pGroupList = hubGroups;
return tdbList;
}

static struct hubConnectStatus *globalHubList;

struct hubConnectStatus *hubConnectGetHubs()
/* return the static global to the track data hubs */
{
return globalHubList;
}

struct trackHub *hubConnectGetHub(char *hubUrl)
/* Return the connected hub for hubUrl, or NULL if not found.  Do not free result. */
{
struct hubConnectStatus *status;
for (status = globalHubList;  status != NULL;  status = status->next)
    {
    if (sameString(status->hubUrl, hubUrl))
        return status->trackHub;
    }
return NULL;
}

struct trackHub *hubConnectGetHubForDb(char *db)
/* Return the connected hub for db, or NULL if not found.  Do not free result. */
{
unsigned hubId = hubIdFromTrackName(db);
struct hubConnectStatus *status;
for (status = globalHubList;  status != NULL;  status = status->next)
    {
    if (status->id == hubId)
        return status->trackHub;
    }
return NULL;
}

char *hubConnectLoadHubs(struct cart *cart)
/* load the track data hubs.  Set a static global to remember them */
{
char *newDatabase = checkForNew( cart);
cartSetString(cart, hgHubConnectRemakeTrackHub, "on");
struct hubConnectStatus  *hubList =  hubConnectStatusListFromCart(cart);
globalHubList = hubList;

return newDatabase;
}

char *hubNameFromUrl(char *hubUrl)
/* Given the URL for a hub, return its hub_# name. */
{
char query[PATH_LEN*4];
sqlSafef(query, sizeof(query), "select concat('hub_', id) from %s where hubUrl = '%s'",
         getHubStatusTableName(), hubUrl);
struct sqlConnection *conn = hConnectCentral();
char *name = sqlQuickString(conn, query);
hDisconnectCentral(&conn);
return name;
}
