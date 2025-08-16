/* hubConnect - stuff to manage connections to track hubs.  Most of this is mediated through
 * the hubStatus table in the hgcentral database.  Here there are routines to translate between
 * hub symbolic names and hub URLs,  to see if a hub is up or down or sideways (up but badly
 * formatted) etc.  Note that there is no C structure corresponding to a row in the hubStatus 
 * table by design.  We just want field-by-field access to this. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

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
#include "hubPublic.h"
#include "genark.h"
#include "asmAlias.h"
#include "cheapcgi.h"

boolean hubsCanAddGroups()
/* can track hubs have their own groups? */
{
static boolean canHubs = FALSE;
static boolean canHubsSet = FALSE;

if (!canHubsSet)
    {
    canHubs = cfgOptionBooleanDefault("trackHubsCanAddGroups", FALSE);
    canHubsSet = TRUE;
    }

return canHubs;
}

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

    // now see if we should quicklift any hubs
    struct sqlConnection *conn = hConnectCentral();
    char query[2048];
    hubVarList = cartVarsWithPrefix(cart, "quickLift");
    for (hubVar = hubVarList; hubVar != NULL; hubVar = hubVar->next)
        {
        unsigned hubNumber = atoi(hubVar->name + strlen("quickLift."));
        sqlSafef(query, sizeof(query), "select hubUrl from hubStatus where id='%d'", hubNumber);
        char *hubUrl = sqlQuickString(conn, query);
        char *errorMessage;
        unsigned hubId = hubFindOrAddUrlInStatusTable(cart, hubUrl, &errorMessage);

        if (firstOne)
            firstOne = FALSE;
        else
            dyStringAppendC(trackHubs, ' ');
        dyStringPrintf(trackHubs, "%d:%s", hubId,(char *)hubVar->val);
        }
    hDisconnectCentral(&conn);

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
*errorMessage = NULL;

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
struct hubConnectStatus *hubConnectStatusForIdExt(struct sqlConnection *conn, int id, char *replaceDb, char *newDb, char *quickLiftChain)
/* Given a hub ID return associated status. For quickLifted hubs, replace the db with our current db and
 * keep track of the quickLiftChain for updating trackDb later.*/
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
    hub->shortLabel = cloneString(row[4]);
    if (isEmpty(row[2]) || hubTimeToCheck(hub, row[3]))
	{
	char *errorMessage = NULL;
	hub->trackHub = fetchHub( hub, &errorMessage);
        if (hub->trackHub)
            hub->trackHub->hubStatus = hub;
	hub->errorMessage = cloneString(errorMessage);
	hubUpdateStatus( hub->errorMessage, hub);
	if (!isEmpty(hub->errorMessage))
	    {
            boolean isCollection = (strstr(hub->hubUrl, "hgComposite") != NULL);
            if (isCollection)
                warn("You created a <a href=\"/cgi-bin/hgCollection\"><b>Track "
         "Collection</b></a> that has expired and been removed. Track Collections "
         "expire 48 hours after their last use. <a href=\"/cgi-bin/hgSession\"><b>"
         "Save your session</b></a> to preserve collections long-term and to allow sharing.");
            // commenting this out, but leaving it in the source because we might use it later.
            //else
                //warn("Could not connect to hub \"%s\": %s", hub->shortLabel, hub->errorMessage);
	    }
        if ((hub->trackHub != NULL) && (replaceDb != NULL))
            {
            struct trackHubGenome *genome = hub->trackHub->genomeList;

            for(; genome; genome = genome->next)
                {
                if (sameString(genome->name, replaceDb))
                    {
                    genome->name = newDb;
                    hashAdd(hub->trackHub->genomeHash, newDb, genome);
                    genome->quickLiftChain = quickLiftChain;
                    genome->quickLiftDb = replaceDb;
                    }
                }
            }
	}
    }
sqlFreeResult(&sr);
return hub;
}

struct hubConnectStatus *hubConnectStatusForId(struct sqlConnection *conn, int id)
{
return hubConnectStatusForIdExt(conn, id, NULL, NULL, NULL);
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

void removeQuickListReference(struct cart *cart, unsigned hubId, char *toDb)
{
// for now we're deleting quickLifted ups that aren't to the current datbase
char buffer[4096];

safef(buffer, sizeof buffer, "quickLift.%d.%s", hubId, toDb);
cartRemove(cart, buffer);
cartSetString(cart, hgHubConnectRemakeTrackHub, "on");
}

struct hubConnectStatus *hubConnectStatusListFromCart(struct cart *cart, char *db)
/* Return list of track hubs that are turned on by user in cart. */
{
struct hubConnectStatus *hubList = NULL;
struct slName *name, *nameList = hubConnectHubsInCart(cart);
struct sqlConnection *conn = hConnectCentral();
for (name = nameList; name != NULL; name = name->next)
    {
    // items in trackHub statement may need to be quickLifted.  This is implied
    // by the hubStatus id followed by a colon and then a index into the quickLiftChain table
    struct hubConnectStatus *hub = NULL;
    char *copy = cloneString(name->name);
    char *colon = strchr(copy, ':');
    if (colon)
        *colon++ = 0;
    int id = sqlSigned(copy);
    if (colon == NULL)  // not quickLifted
        hub = hubConnectStatusForId(conn, id);
    else
        {
        char query[4096];
        sqlSafef(query, sizeof(query), "select fromDb, toDb, path from %s where id = \"%s\"", "quickLiftChain", colon);
        struct sqlResult *sr = sqlGetResult(conn, query);
        char **row;
        char *replaceDb = NULL;
        char *quickLiftChain = NULL;
        char *toDb = NULL;
        while ((row = sqlNextRow(sr)) != NULL)
            {
            replaceDb = cloneString(row[0]);
            toDb = cloneString(row[1]);
            quickLiftChain = cloneString(row[2]);
            break; // there's only one
            }
        sqlFreeResult(&sr);

        // don't load quickLift hubs that aren't for us
        //if ((db == NULL) || sameOk(toDb, hubConnectSkipHubPrefix(db)))
        hub = hubConnectStatusForIdExt(conn, id, replaceDb, toDb, quickLiftChain);
        }
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

char *hubNameFromGroupName(char *groupName)
/* Given something like "hub_123_myWig" return hub_123 */
{
char *hubName = cloneString(groupName);

char *ptr1 = hubName;
ptr1 += 4;
char *ptr2 = strchr(ptr1, '_');

if (ptr2 != NULL)
    *ptr2 = 0;

return hubName;
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

struct hubConnectStatus *hubFromIdNoAbort(unsigned hubId)
/* Given a hub ID number, return corresponding trackHub structure. */
{
struct sqlConnection *conn = hConnectCentral();
struct hubConnectStatus *status = hubConnectStatusForId(conn, hubId);
hDisconnectCentral(&conn);
return status;
}

struct hubConnectStatus *hubFromId(unsigned hubId)
/* Given a hub ID number, return corresponding trackHub structure. 
 * ErrAbort if there's a problem. */
{
struct hubConnectStatus *status = hubFromIdNoAbort(hubId);
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

static void addQuickToHash(struct hash *hash, char *chain, char *db)
{
hashAdd(hash, "quickLiftUrl", chain);
hashAdd(hash, "quickLiftDb", db);
}

static void assignQuickLift(struct trackDb *tdbList, char *quickLiftChain, char *db)
/* step through a trackDb list and assign a quickLift chain to each track */
{
if (tdbList == NULL)
    return;

struct trackDb *tdb;
for(tdb = tdbList; tdb; tdb = tdb->next)
    {
    assignQuickLift(tdb->subtracks, quickLiftChain, db);

    addQuickToHash( tdb->settingsHash, quickLiftChain, db);

    if (tdb->parent)
        {
        addQuickToHash( tdb->parent->settingsHash, quickLiftChain, db);
        }
    }
}

// a string to define trackDb for quickLift chain
static char *chainTdbString = 
    "shortLabel chain to %s\n"
    "longLabel chain to %s\n"
    "type bigChain %s\n"
    "chainType reverse\n"
    "bigDataUrl %s\n"
    "quickLiftUrl %s\n"
    "quickLiftDb %s\n";

static struct trackDb *makeQuickLiftChainTdb(struct trackHubGenome *hubGenome,  struct hubConnectStatus *hub)
// make a trackDb entry for a quickLift chain
{
struct trackDb *tdb;

AllocVar(tdb);

char buffer[4096];
safef(buffer, sizeof buffer, "hub_%d_quickLiftChain", hub->id);
tdb->table = tdb->track = cloneString(buffer);
safef(buffer, sizeof buffer, chainTdbString, hubGenome->quickLiftDb, hubGenome->quickLiftDb, hubGenome->quickLiftDb, hubGenome->quickLiftChain, hubGenome->quickLiftChain, hubGenome->quickLiftDb);
tdb->settings = cloneString(buffer);
tdb->settingsHash = trackDbSettingsFromString(tdb, buffer);
trackDbFieldsFromSettings(tdb);
tdb->visibility = tvDense;

return tdb;
}

static struct trackDb *fixForQuickLift(struct trackDb *tdbList, struct trackHubGenome *hubGenome, struct hubConnectStatus *hub)
// assign a quickLift chain to the tdbList and make a trackDb entry for the chain. 
{
assignQuickLift(tdbList, hubGenome->quickLiftChain, hub->trackHub->defaultDb);

struct trackDb *quickLiftTdb = makeQuickLiftChainTdb(hubGenome, hub);
quickLiftTdb->grp = tdbList->grp;
slAddHead(&tdbList, quickLiftTdb);

return tdbList;
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
if (hubGenome == NULL)
    errAbort("Cannot find genome %s in hub %s", database, hub->hubUrl);
boolean foundFirstGenome = FALSE;
struct hash *trackDbNameHash = newHash(5);
struct trackDb *tdbList = hubAddTracks(hub, database, &foundFirstGenome, trackDbNameHash, NULL);
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
struct dyString *dy = dyStringNew(1024);
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

sqlGetLockWithTimeout(conn, "central_hubStatus", 15);

// Try to grab a row right before we insert but after the lock.
sqlSafef(query, sizeof(query), "select id from %s where hubUrl = \"%s\"", statusTable, url);
struct sqlResult *sr = sqlGetResult(conn, query);

if (sqlNextRow(sr) == NULL)  // if we got something from this query, someone added it right before we locked it
    {
    if (sqlFieldIndex(conn, statusTable, "firstAdded") >= 0)
        sqlSafef(query, sizeof(query), "insert into %s (hubUrl,shortLabel,longLabel,dbCount,dbList,status,lastOkTime,lastNotOkTime,errorMessage,firstAdded) values (\"%s\",\"\",\"\",0,NULL,0,\"\",\"\",\"\",now())", statusTable, url);
    else
        sqlSafef(query, sizeof(query), "insert into %s (hubUrl) values (\"%s\")",
	statusTable, url);
    sqlUpdate(conn, query);
    }
sqlFreeResult(&sr);
sqlReleaseLock(conn, "central_hubStatus");
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
if (errorMessage != NULL)
    hub->errorMessage = cloneString(errorMessage);

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

unsigned hubFindOrAddUrlInStatusTable(struct cart *cart, char *url, char **errorMessage)
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
/* see if the user just typed in a new hub url, or we have one or more hubUrl 
 * on the command line.  Return the new database if there is one. */
{
char *newDatabase = NULL;
boolean doClear = FALSE;
char *assemblyDb = cartOptionalString(cart, hgHubGenome);
char *wantFirstDb = cartOptionalString(cart, hgHubDoFirstDb);

struct slName *urls = cartOptionalSlNameList(cart, hgHubDataClearText);
if (urls)
    doClear = TRUE;
else
    urls  = cartOptionalSlNameList(cart, hgHubDataText);

if (urls == NULL)
    return NULL;

for(; urls; urls = urls->next)
    {
    char *url = cloneString(urls->name);
    if (doClear)
        disconnectHubsSamePrefix(cart, url);

    trimSpaces(url);

    // go and grab the hub and set the cart variables to connect it
    struct hubConnectStatus *hub;
    gNewHub = hub = getAndSetHubStatus( cart, url, TRUE);

    if (newDatabase == NULL)  // if we haven't picked a new database yet
        {
        if ((wantFirstDb != NULL) && (hub->trackHub != NULL)) // choose the first db
            newDatabase = hub->trackHub->defaultDb;
        else if (assemblyDb != NULL)
            {
            // Check to see if the user specified an assembly within
            // an assembly hub.
            struct trackHub *trackHub = hub->trackHub;
            if (trackHub != NULL)
                {
                struct trackHubGenome *genomeList = trackHub->genomeList;

                for(; genomeList; genomeList = genomeList->next)
                    {
                    if (sameString(assemblyDb, hubConnectSkipHubPrefix(genomeList->name)))
                        {
                        if (hub->errorMessage)
                            errAbort("Hub error: %s", hub->errorMessage);
                        newDatabase = genomeList->name;
                        break;
                        }
                    }
                }
            }
        }
    }

cartRemove(cart, hgHubDataClearText);
cartRemove(cart, hgHubDataText);
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
char query[1024 * 1024];
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

static void grpListAddHubName(struct grp *grpList, struct trackHub *hub)
/* Add the hub name to the groups defined by a hub. */
{
char buffer[4096];

for (; grpList; grpList = grpList->next)
    {
    safef(buffer, sizeof buffer, "%s_%s", hub->name, grpList->name);
    grpList->name = cloneString(buffer);
    safef(buffer, sizeof buffer, "Hub: %s : %s", hub->shortLabel, grpList->label);
    grpList->label = cloneString(buffer);
    }
}

struct trackDb *hubAddTracks(struct hubConnectStatus *hub, char *database, boolean *foundFirstGenome, struct hash *trackDbNameHash, struct grp **hubGroups)
/* Load up stuff from data hub and append to list. The hubUrl points to
 * a trackDb.ra format file. Only the first example of a genome gets to 
 * populate groups, the others get a group for the trackHub.  A particular 
 * trackDb is only read once even if referenced from more than one hub.  */
{
struct trackDb *tdbList = NULL;
/* Load trackDb.ra file and make it into proper trackDb tree */
struct trackHub *trackHub = hub->trackHub;

if (trackHub != NULL)
    {
    struct trackHubGenome *hubGenome = trackHubFindGenome(trackHub, database);
    if ((hubGenome == NULL) || hashLookup(trackDbNameHash,  hubGenome->trackDbFile))
        hubGenome = NULL; // we already saw this trackDb, so ignore this stanza
    else
        hashStore(trackDbNameHash,  hubGenome->trackDbFile);

    if (hubGenome != NULL)
	{
        // add groups
	if ((hubGenome->groups != NULL) && hubsCanAddGroups())
            {
            struct grp *list = readGroupRa(hubGenome->groups);
            grpListAddHubName(list, hub->trackHub);

            if (hubGroups)
                *hubGroups = slCat(*hubGroups, list);
            }

        tdbList = trackHubAddTracksGenome(hubGenome);

        if (tdbList && hubGenome->quickLiftChain)
            tdbList = fixForQuickLift(tdbList, hubGenome, hub);
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
if (startsWith("Quick", hub->shortLabel))
    grp->label = hub->shortLabel;
else
    {
    char buffer[4096];
    safef(buffer, sizeof buffer, "Hub: %s", hub->shortLabel);
    grp->label = cloneString(buffer);
    }
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
boolean foundFirstGenome = FALSE;
struct hash *trackDbNameHash = newHash(5);
for (hub = hubList; hub != NULL; hub = hub->next)
    {
    if (isEmpty(hub->errorMessage))
	{
        /* error catching in so it won't just abort  */
        struct errCatch *errCatch = errCatchNew();
        if (errCatchStart(errCatch))
	    {
	    struct trackDb *thisList = hubAddTracks(hub, database, &foundFirstGenome, trackDbNameHash, &hubGroups);
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
    else
        {
        /* create an empty group to hold the error message. */
        struct grp *grp = grpFromHub(hub);
        grp->errMessage = hub->errorMessage;
        slAddHead(&hubGroups, grp);
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
if (!startsWith("hub_", db))
    return NULL;
unsigned hubId = hubIdFromTrackName(db);
struct hubConnectStatus *status;
for (status = globalHubList;  status != NULL;  status = status->next)
    {
    if (status->id == hubId)
        return status->trackHub;
    }
return NULL;
}

static unsigned lookForUndecoratedDb(char *name)
// Look for this undecorated in the attached assembly hubs
{
struct trackHubGenome *genome = trackHubGetGenomeUndecorated(name);

if (genome == NULL)
    return FALSE;

struct trackHub *trackHub = genome->trackHub;

if ((trackHub != NULL) && (trackHub->hubStatus != NULL))
    return trackHub->hubStatus->id;
return 0;
}

static boolean lookForLonelyHubs(struct cart *cart, struct hubConnectStatus  *hubList, char **newDatabase, char *genarkPrefix)
// We go through the hubs and see if any of them reference an assembly
// that is NOT currently loaded, but we know a URL to load it.
{
struct sqlConnection *conn = hConnectCentral();
if (!sqlTableExists(conn, genarkTableName()))
    return FALSE;

boolean added = FALSE;

struct hubConnectStatus  *hub;
for(hub = hubList; hub; hub = hub->next)
    {
    struct trackHub *tHub = hub->trackHub;
    if (tHub == NULL)
        continue;

    struct trackHubGenome *genomeList = tHub->genomeList, *genome;

    for(genome = genomeList; genome; genome = genome->next)
        {
        char *name = genome->name;

        name = asmAliasFind(name);
        if (!hDbIsActive(name) )
            {
            char buffer[4096];
            unsigned newId = 0;

            // look with undecorated name for an attached assembly hub
            if (!(newId = lookForUndecoratedDb(name)))
                {
                // see if genark has this assembly
                char query[4096];
                sqlSafef(query, sizeof query, "select hubUrl from %s where gcAccession='%s'", genarkTableName(), name);
                if (sqlQuickQuery(conn, query, buffer, sizeof buffer))
                    {
                    char url[4096];
                    safef(url, sizeof url, "%s/%s", genarkPrefix, buffer);

                    struct hubConnectStatus *status = getAndSetHubStatus( cart, url, TRUE);

                    if (status)
                        {
                        newId = status->id;
                        added = TRUE;
                        }
                    }
                }

            // if we found an id, change some names to use it as a decoration
            if (newId)
                {
                safef(buffer, sizeof buffer, "hub_%d_%s", newId, name);

                genome->name = cloneString(buffer);
    
                // if our new database is an undecorated db, decorate it
                if (*newDatabase && sameString(*newDatabase, name))
                    *newDatabase = cloneString(buffer);
                }

            }
        }
    }

hDisconnectCentral(&conn);
return added;
}

static char *getCuratedHubPrefix()
/* figure out what sandbox we're in. */
{
char *curatedHubPrefix = cfgOption("curatedHubPrefix");
if (isEmpty(curatedHubPrefix))
    curatedHubPrefix = "public";

return curatedHubPrefix;
}


boolean hubConnectGetCuratedUrl(char *db, char **hubUrl)
/* Check to see if this db is a curated hub and if so return its hubUrl */
{
struct sqlConnection *conn = hConnectCentral();
char query[4096];
sqlSafef(query, sizeof query, "SELECT nibPath from %s where name = '%s' AND nibPath like '%s%%'",
          dbDbTable(), db, hubCuratedPrefix);

char *dir = sqlQuickString(conn, query);
boolean ret = !isEmpty(dir);
hDisconnectCentral(&conn);

if (hubUrl != NULL) // if user passed in hubUrl, calculate what it should be
    {
    *hubUrl = NULL;
    if (!isEmpty(dir))   // this is a curated hub
        {
        char *path = dir + sizeof(hubCuratedPrefix) - 1;
        char url[4096];
        safef(url, sizeof url, "%s/%s/hub.txt", path, getCuratedHubPrefix());
        *hubUrl = cloneString(url);
        }
    }

return ret;
}

boolean hubConnectIsCurated(char *db)
/* Look in the dbDb table to see if this hub is curated. */
{
return hubConnectGetCuratedUrl(db, NULL);
}

static int lookForCuratedHubs(struct cart *cart, char *db,  char *curatedHubPrefix)
/* Check to see if db is a curated hub which will require the hub to be attached. 
 * The variable curatedHubPrefix has the release to use (alpha, beta, public, or a user name ) */
{
struct sqlConnection *conn = hConnectCentral();
char query[4096];
sqlSafef(query, sizeof query, "SELECT nibPath from %s where name = '%s' AND nibPath like '%s%%'",
          dbDbTable(), db, hubCuratedPrefix);

char *dir = cloneString(sqlQuickString(conn, query));
hDisconnectCentral(&conn);

if (!isEmpty(dir))
    {
    char *path = &dir[sizeof hubCuratedPrefix - 1];
    char urlBuf[4096];
    safef(urlBuf, sizeof urlBuf, "%s/%s/hub.txt", path, curatedHubPrefix);
    char *url = hReplaceGbdb(urlBuf);

    struct hubConnectStatus *status = getAndSetHubStatus( cart, url, TRUE);

    if (status && isEmpty(status->errorMessage))
        {
        char buffer[4096];
        safef(buffer, sizeof buffer, "hub_%d_%s", status->id, db);

        cartSetString(cart, "db", buffer);
        if (cgiOptionalString("db"))
            {
            /* user specified db on URL, we need to decorate and put it back. */
            cgiVarSet("db",  cloneString(buffer));
            }

        return status->id;
        }
    else
        {
        if (!isEmpty(status->errorMessage))
            errAbort("Hub error: url %s: error  %s.", url, status->errorMessage);
        else
            errAbort("Cannot open hub %s.", url);
        }

    }
return 0;
}

static void portHubStatus(struct cart *cart)
/* When a session has been saved on a different host it may have cart variables that reference hubStatus id's
 * that are different than what the local machine has.  Look for these cases using the "assumesHub" cart variable
 * that maps id numbers to URL's. */
{
char *assumesHubStr = cartOptionalString(cart, "assumesHub");
if (assumesHubStr == NULL)
    return;

struct slPair *pairs = slPairListFromString(assumesHubStr, FALSE);
for(; pairs; pairs = pairs->next)
    {
    char *url = (char *)pairs->val;
    unsigned sessionHubId = atoi(pairs->name);
    char *errMessage;
    unsigned localHubId = hubFindOrAddUrlInStatusTable(cart, url, &errMessage);

    if (localHubId != sessionHubId)  // the hubStatus id on the local machine is different than in the session
        {
        // first look for visibility and settings for tracks on this hub
        char wildCard[4096];

        safef(wildCard, sizeof wildCard, "hub_%d_*", sessionHubId);
        struct slPair *cartVars = cartVarsLike(cart, wildCard);

        for(; cartVars; cartVars = cartVars->next)
            {
            char *rest = trackHubSkipHubName(cartVars->name);
            char newVarName[4096];
            
            // add the new visibility/setting
            safef(newVarName, sizeof newVarName, "hub_%d_%s", localHubId, rest);
            cartSetString(cart, newVarName, cartVars->val);

            // remove the old visibility/setting
            cartRemove(cart, cartVars->name);
            }

        // turn on this remapped hub
        char hubName[4096];
        cartSetString(cart, hgHubConnectRemakeTrackHub, "on");
        safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, localHubId);
        cartSetString(cart, hubName, "1");

        // remove the old hub connection
        safef(hubName, sizeof(hubName), "%s%u", hgHubConnectHubVarPrefix, sessionHubId);
        cartRemove(cart, hubName);
        }
    }

cartRemove(cart, "assumesHub");
}

char *hubConnectLoadHubs(struct cart *cart)
/* load the track data hubs.  Set a static global to remember them */
{
char *dbSpec = asmAliasFind(cartOptionalString(cart, "db"));
char *curatedHubPrefix = getCuratedHubPrefix();
if (dbSpec != NULL)
    lookForCuratedHubs(cart, trackHubSkipHubName(dbSpec), curatedHubPrefix);

char *newDatabase = checkForNew( cart);
newDatabase = asmAliasFind(newDatabase);
cartSetString(cart, hgHubConnectRemakeTrackHub, "on");

portHubStatus(cart);

struct hubConnectStatus  *hubList =  hubConnectStatusListFromCart(cart, dbSpec);

char *genarkPrefix = cfgOption("genarkHubPrefix");
if (genarkPrefix && lookForLonelyHubs(cart, hubList, &newDatabase, genarkPrefix))
    hubList = hubConnectStatusListFromCart(cart, dbSpec);

globalHubList = hubList;

return newDatabase;
}

char *hubNameFromUrl(char *hubUrl)
/* Given the URL for a hub, return its hub_# name. */
{
if (hubUrl == NULL)
    return NULL;

char query[PATH_LEN*4];
sqlSafef(query, sizeof(query), "select concat('hub_', id) from %s where hubUrl = '%s'",
         getHubStatusTableName(), hubUrl);
struct sqlConnection *conn = hConnectCentral();
char *name = sqlQuickString(conn, query);
hDisconnectCentral(&conn);
return name;
}

void addPublicHubsToHubStatus(struct cart *cart, struct sqlConnection *conn, char *publicTable, char  *statusTable)
/* Add urls in the hubPublic table to the hubStatus table if they aren't there already */
{
char query[1024];
sqlSafef(query, sizeof(query),
        "select %s.hubUrl from %s left join %s on %s.hubUrl = %s.hubUrl where %s.hubUrl is NULL",
        publicTable, publicTable, statusTable, publicTable, statusTable, statusTable);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *errorMessage = NULL;
    char *url = row[0];

    // add this url to the hubStatus table
    hubFindOrAddUrlInStatusTable(cart, url, &errorMessage);
    }
}

struct hash *buildPublicLookupHash(struct sqlConnection *conn, char *publicTable, char *statusTable,
        struct hash **pHash)
/* Return a hash linking hub URLs to struct hubEntries.  Also make pHash point to a hash that just stores
 * the names of the public hubs (for use later when determining if hubs were added by the user) */
{
struct hash *hubLookup = newHash(5);
struct hash *publicLookup = newHash(5);
char query[512];
bool hasDescription = sqlColumnExists(conn, publicTable, "descriptionUrl");
if (hasDescription)
    sqlSafef(query, sizeof(query), "select p.hubUrl,p.shortLabel,p.longLabel,p.dbList,"
            "s.errorMessage,s.id,p.descriptionUrl from %s p,%s s where p.hubUrl = s.hubUrl",
	    publicTable, statusTable);
else
    sqlSafef(query, sizeof(query), "select p.hubUrl,p.shortLabel,p.longLabel,p.dbList,"
            "s.errorMessage,s.id from %s p,%s s where p.hubUrl = s.hubUrl",
	    publicTable, statusTable);

struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hubEntry *hubInfo = hubEntryTextLoad(row, hasDescription);
    hubInfo->tableHasDescriptionField = hasDescription;
    hashAddUnique(hubLookup, hubInfo->hubUrl, hubInfo);
    hashStore(publicLookup, hubInfo->hubUrl);
    }
sqlFreeResult(&sr);
*pHash = publicLookup;
return hubLookup;
}
