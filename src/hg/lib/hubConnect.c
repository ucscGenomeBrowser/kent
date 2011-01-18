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
#include "trackHub.h"
#include "hubConnect.h"
#include "hui.h"


boolean isHubTrack(char *trackName)
/* Return TRUE if it's a hub track. */
{
return startsWith(hubTrackPrefix, trackName);
}

boolean hubConnectTableExists()
/* Return TRUE if the hubConnect table exists. */
{
struct sqlConnection *conn = hConnectCentral();
boolean exists = sqlTableExists(conn, hubConnectTableName);
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

struct hubConnectStatus *hubConnectStatusForId(struct sqlConnection *conn, int id)
/* Given a hub ID return associated status. Returns NULL if no such hub.  If hub
 * exists but has problems will return with errorMessage field filled in. */
{
struct hubConnectStatus *hub = NULL;
char query[1024];
safef(query, sizeof(query), 
    "select id,shortLabel,longLabel,hubUrl,errorMessage,dbCount,dbList from %s where id=%d",
    hubConnectTableName, id);
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
    }
sqlFreeResult(&sr);
return hub;
}

struct hubConnectStatus *hubConnectStatusListFromCart(struct cart *cart)
/* Return list of track hubs that are turned on by user in cart. */
{
struct hubConnectStatus *hubList = NULL, *hub;
struct slName *name, *nameList = hubConnectHubsInCart(cart);
struct sqlConnection *conn = hConnectCentral();
for (name = nameList; name != NULL; name = name->next)
    {
    int id = sqlUnsigned(name->name);
    hub = hubConnectStatusForId(conn, id);
    if (hub != NULL)
        slAddHead(&hubList, hub);
    }
slFreeList(&nameList);
hDisconnectCentral(&conn);
slReverse(&hubList);
return hubList;
}

int hubIdFromTrackName(char *trackName)
/* Given something like "hub_123_myWig" return 123 */
{
assert(startsWith("hub_", trackName));
trackName += 4;
assert(isdigit(trackName[0]));
return atoi(trackName);
}

struct trackDb *hubConnectAddHubForTrackAndFindTdb(char *database, 
	char *trackName, struct trackDb **pTdbList, struct hash *trackHash)
/* Go find hub for trackName (which will begin with hub_), and load the tracks
 * for it, appending to end of list and adding to trackHash.  Return the
 * trackDb associated with trackName.  */
{
int hubId = hubIdFromTrackName(trackName);
struct sqlConnection *conn = hConnectCentral();
struct hubConnectStatus *hubStatus = hubConnectStatusForId(conn, hubId);
hDisconnectCentral(&conn);
if (hubStatus == NULL)
    errAbort("The hubId %d was not found", hubId);
if (!isEmpty(hubStatus->errorMessage))
    errAbort("Hub %s at %s has the error: %s", hubStatus->shortLabel, 
	    hubStatus->hubUrl, hubStatus->errorMessage);
char hubName[16];
safef(hubName, sizeof(hubName), "hub_%d", hubId);
struct trackHub *hub = trackHubOpen(hubStatus->hubUrl, hubName);
struct trackHubGenome *hubGenome = trackHubFindGenome(hub, database);
struct trackDb *tdbList = trackHubTracksForGenome(hub, hubGenome);
tdbList = trackDbLinkUpGenerations(tdbList);
tdbList = trackDbPolishAfterLinkup(tdbList, database);
rAddTrackListToHash(trackHash, tdbList, NULL, FALSE);
if (pTdbList != NULL)
    *pTdbList = slCat(*pTdbList, tdbList);
struct trackDb *tdb = hashFindVal(trackHash, trackName);
if (tdb == NULL)
    errAbort("Can't find track %s in %s", trackName, hubStatus->hubUrl);
return tdb;
}

