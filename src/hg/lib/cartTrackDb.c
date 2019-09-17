/* cartTrackDb - Combine custom tracks, hub tracks & hTrackDb to get unified groups/tracks/tables */

#include "common.h"
#include "cartTrackDb.h"
#include "cheapcgi.h"
#include "customTrack.h"
#include "hash.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgConfig.h"
#include "hgMaf.h"
#include "hubConnect.h"
#include "joiner.h"
#include "trackHub.h"
#include "wikiTrack.h"

/* Static globals */
static boolean useAC = FALSE;
static struct slRef *accessControlTrackRefList = NULL;

static struct trackDb *getFullTrackList(struct cart *cart, char *db, struct grp **pHubGroups)
{
struct trackDb *list = hTrackDb(db);

/* add wikiTrack if enabled */
if (wikiTrackEnabled(db, NULL))
    slAddHead(&list, wikiTrackDb());
slSort(&list, trackDbCmp);

// Add hub tracks at head of list
struct trackDb *hubTdbList = hubCollectTracks(db, pHubGroups);
list = slCat(list, hubTdbList);

/* exclude any track with a 'tableBrowser off' setting */
struct trackDb *tdb, *nextTdb, *newList = NULL;
for (tdb = list;  tdb != NULL;  tdb = nextTdb)
    {
    nextTdb = tdb->next;
    if (tdbIsDownloadsOnly(tdb) || tdb->table == NULL)
        {
        //freeMem(tdb);  // should not free tdb's.
        // While hdb.c should and says it does cache the tdbList, it doesn't.
        // The most notable reason that the tdbs are not cached is this hgTables CGI !!!
        // It needs to be rewritten to make tdbRef structures for the lists it creates here!
        continue;
        }

    char *tbOff = trackDbSetting(tdb, "tableBrowser");
    if (useAC && tbOff != NULL &&
        (startsWithWord("off", tbOff) || startsWithWord("noGenome", tbOff)))
        {
        slAddHead(&accessControlTrackRefList, slRefNew(tdb));
        if (! startsWithWord("off", tbOff))
            slAddHead(&newList, tdb);
        }
    else
	slAddHead(&newList, tdb);
    }
slReverse(&newList);
list = newList;

// Add custom tracks at head of list
struct customTrack *ctList, *ct;
ctList = customTracksParseCart(db, cart, NULL, NULL);
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    slAddHead(&list, ct->tdb);
    }

return list;
}

static struct grp *makeGroupList(char *db, struct trackDb *trackList, struct grp **pHubGrpList,
                                 boolean allTablesOk)
/* Get list of groups that actually have something in them. */
{
struct grp *groupsAll, *groupList = NULL, *group;
struct hash *groupsInTrackList = newHash(0);
struct hash *groupsInDatabase = newHash(0);
struct trackDb *track;

/* Stream through track list building up hash of active groups. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInTrackList,track->grp))
        hashAdd(groupsInTrackList, track->grp, NULL);
    }

/* Scan through group table, putting in ones where we have data. */
groupsAll = hLoadGrps(db);
for (group = slPopHead(&groupsAll); group != NULL; group = slPopHead(&groupsAll))
    {
    if (hashLookup(groupsInTrackList, group->name))
	{
	slAddTail(&groupList, group);
	hashAdd(groupsInDatabase, group->name, group);
	}
    else
        grpFree(&group);
    }

/* if we have custom tracks, we want to add the track hubs
 * after that group */
struct grp *addAfter = NULL;
if ((groupList != NULL) && sameString(groupList->name, "user"))
    addAfter = groupList;

/* Add in groups from hubs. */
for (group = slPopHead(pHubGrpList); group != NULL; group = slPopHead(pHubGrpList))
    {
    // if the group isn't represented in any track, don't add it to list
    if (!hashLookup(groupsInTrackList,group->name))
	continue;
    /* check to see if we're inserting hubs rather than
     * adding them to the front of the list */
    struct grp *newGrp = grpDup(group);
    if (addAfter != NULL)
	{
	newGrp->next = addAfter->next;
	addAfter->next = newGrp;
	}
    else
	slAddHead(&groupList, newGrp);
    hashAdd(groupsInDatabase, newGrp->name, newGrp);
    }

/* Do some error checking for tracks with group names that are
 * not in database.  Just warn about them. */
if (!trackHubDatabase(db))
    for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInDatabase, track->grp))
         warn("Track %s has group %s, which isn't in grp table",
	 	track->table, track->grp);
    }

/* Create dummy group for all tracks. */
AllocVar(group);
group->name = cloneString("allTracks");
group->label = cloneString("All Tracks");
slAddTail(&groupList, group);

/* Create another dummy group for all tables. */
if (allTablesOk)
    {
    AllocVar(group);
    group->name = cloneString("allTables");
    group->label = cloneString("All Tables");
    slAddTail(&groupList, group);
    }

hashFree(&groupsInTrackList);
hashFree(&groupsInDatabase);
return groupList;
}

void cartTrackDbInit(struct cart *cart, struct trackDb **retFullTrackList,
                     struct grp **retFullGroupList, boolean useAccessControl)
/* Get lists of all tracks and of groups that actually have tracks in them.
 * If useAccessControl, exclude tracks with 'tableBrowser off' nor tables listed
 * in the table tableAccessControl. */
{
char *db = cartString(cart, "db");
useAC = useAccessControl;
struct grp *hubGrpList = NULL;
struct trackDb *fullTrackList = getFullTrackList(cart, db, &hubGrpList);
boolean allTablesOk = hAllowAllTables() && !trackHubDatabase(db);
struct grp *fullGroupList = makeGroupList(db, fullTrackList, &hubGrpList, allTablesOk);
if (retFullTrackList != NULL)
    *retFullTrackList = fullTrackList;
if (retFullGroupList != NULL)
    *retFullGroupList = fullGroupList;
}

static char *chopAtFirstDot(char *string)
/* Terminate string at first '.' if found.  Return string for convenience. */
{
char *ptr = strchr(string, '.');
if (ptr != NULL)
    *ptr = '\0';
return string;
}

struct accessControl
/* Restricted permission settings for a table */
    {
    struct slName *hostList;       // List of hosts that are allowed to view this table
    boolean isNoGenome;            // True if it's OK for position range but not genome-wide query
    };

void accessControlAddHost(struct accessControl *ac, char *host)
/* Alloc an slName for host (unless NULL or empty) and add it to ac->hostList. */
{
if (isNotEmpty(host))
    slAddHead(&ac->hostList, slNameNew(host));
}

struct accessControl *accessControlNew(char *host, boolean isNoGenome)
/* Alloc, init & return accessControl. */
{
struct accessControl *ac;
AllocVar(ac);
accessControlAddHost(ac, host);
ac->isNoGenome = isNoGenome;
return ac;
}

static void acHashAddOneTable(struct hash *acHash, char *table, char *host, boolean isNoGenome)
/* If table is already in acHash, update its accessControl fields; otherwise store a
 * new accessControl for table. */
{
struct hashEl *hel = hashLookup(acHash, table);
if (hel == NULL)
    {
    struct accessControl *ac = accessControlNew(host, isNoGenome);
    hashAdd(acHash, table, ac);
    }
else
    {
    struct accessControl *ac = hel->val;
    ac->isNoGenome = isNoGenome;
    accessControlAddHost(ac, host);
    }
}

static struct hash *accessControlInit(char *db)
/* Return a hash associating restricted table/track names in the given db/conn
 * with virtual hosts -- hash is empty if there is no tableAccessControl table and no
 * accessControlTrackRefList (see getFullTrackList). */
{
struct hash *acHash = hashNew(0);
if (! trackHubDatabase(db))
    {
    struct sqlConnection *conn = hAllocConn(db);
    if (sqlTableExists(conn, "tableAccessControl"))
        {
        struct sqlResult *sr = NULL;
        char **row = NULL;
        acHash = newHash(0);
        sr = sqlGetResult(conn, NOSQLINJ "select name,host from tableAccessControl");
        while ((row = sqlNextRow(sr)) != NULL)
            acHashAddOneTable(acHash, row[0], chopAtFirstDot(row[1]), FALSE);
        sqlFreeResult(&sr);
        }
    hFreeConn(&conn);
    }
struct slRef *tdbRef;
for (tdbRef = accessControlTrackRefList; tdbRef != NULL; tdbRef = tdbRef->next)
    {
    struct trackDb *tdb = tdbRef->val;
    char *tbOff = cloneString(trackDbSetting(tdb, "tableBrowser"));
    if (isEmpty(tbOff))
        errAbort("accessControlInit bug: tdb for %s does not have tableBrowser setting",
                 tdb->track);
    // First word is "off" or "noGenome":
    char *type = nextWord(&tbOff);
    boolean isNoGenome = sameString(type, "noGenome");
    // Add track table to acHash:
    acHashAddOneTable(acHash, tdb->table, NULL, isNoGenome);
    // Remaining words are additional table names to add:
    char *tbl;
    while ((tbl = nextWord(&tbOff)) != NULL)
        acHashAddOneTable(acHash, tbl, NULL, isNoGenome);
    }
return acHash;
}

static struct hash *getCachedAcHash(char *db)
/* Returns a hash that maps table names to accessControl, creating it if necessary. */
{
static struct hash *dbToAcHash = NULL;
if (dbToAcHash == NULL)
    dbToAcHash = hashNew(0);
struct hash *acHash = hashFindVal(dbToAcHash, db);
if (acHash == NULL)
    {
    acHash = accessControlInit(db);
    hashAdd(dbToAcHash, db, acHash);
    }
return acHash;
}

static char *getCachedCurrentHost()
/* Return the current host name chopped at the first '.' or NULL if not in the CGI environment. */
{
static char *currentHost = NULL;
if (currentHost == NULL)
    {
    currentHost = cloneString(cgiServerName());
    if (currentHost == NULL)
	return NULL;
    else
	chopAtFirstDot(currentHost);
    }
return currentHost;
}

boolean cartTrackDbIsAccessDenied(char *db, char *table)
/* Return TRUE if useAccessControl=TRUE was passed to cartTrackDbInit and
 * if access to table is denied (at least on this host) by 'tableBrowser off'
 * or by the tableAccessControl table. */
{
if (!useAC)
    return FALSE;

struct hash *acHash = getCachedAcHash(db);
struct accessControl *ac = hashFindVal(acHash, table);
if (ac == NULL)
    return FALSE;
if (ac->isNoGenome && ac->hostList == NULL)
    return FALSE;
char *currentHost = getCachedCurrentHost();
if (currentHost == NULL)
    warn("accessControl: unable to determine current host");
return (! slNameInList(ac->hostList, currentHost));
}

boolean cartTrackDbIsNoGenome(char *db, char *table)
/* Return TRUE if range queries, but not genome-queries, are permitted for this table. */
{
struct hash *acHash = getCachedAcHash(db);
struct accessControl *ac = hashFindVal(acHash, table);
return (ac != NULL && ac->isNoGenome);
}

static void addTablesAccordingToTrackType(char *db, struct slName **pList, struct hash *uniqHash,
                                          struct trackDb *track)
/* Parse out track->type and if necessary add some tables from it. */
{
struct slName *name;
char *trackDupe = cloneString(track->type);
if (trackDupe != NULL && trackDupe[0] != 0)
    {
    char *s = trackDupe;
    char *type = nextWord(&s);
    if (sameString(type, "wigMaf"))
        {
	static char *wigMafAssociates[] = {"frames", "summary"};
	int i;
	for (i=0; i<ArraySize(wigMafAssociates); ++i)
	    {
	    char *setting = wigMafAssociates[i];
	    char *table = trackDbSetting(track, setting);
            if (table != NULL)
                {
                name = slNameNew(table);
                slAddHead(pList, name);
                hashAdd(uniqHash, table, NULL);
                }
	    }
        /* include conservation wiggle tables */
        struct consWiggle *wig, *wiggles = wigMafWiggles(db, track);
        slReverse(&wiggles);
        for (wig = wiggles; wig != NULL; wig = wig->next)
            {
            name = slNameNew(wig->table);
            slAddHead(pList, name);
            hashAdd(uniqHash, wig->table, NULL);
            }
	}
    if (track->subtracks)
        {
        struct slName *subList = NULL;
	struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(track->subtracks);
	slSort(&tdbRefList, trackDbRefCmp);
	struct slRef *tdbRef;
	for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
            {
	    struct trackDb *subTdb = tdbRef->val;
	    name = slNameNew(subTdb->table);
	    slAddTail(&subList, name);
	    hashAdd(uniqHash, subTdb->table, NULL);
            }
        pList = slCat(pList, subList);
        }
    }
freez(&trackDupe);
}

struct slName *cartTrackDbTablesForTrack(char *db, struct trackDb *track, boolean useJoiner)
/* Return list of all tables associated with track.  If useJoiner, the result can include
 * non-positional tables that are related to track by all.joiner. */
{
static struct joiner *allJoiner = NULL;
struct hash *uniqHash = newHash(8);
struct slName *name, *nameList = NULL;
char *trackTable = track->table;

hashAdd(uniqHash, trackTable, NULL);
if (useJoiner)
    {
    if (allJoiner == NULL)
        allJoiner = joinerRead("all.joiner");
    struct joinerPair *jpList, *jp;
    jpList = joinerRelate(allJoiner, db, trackTable, db);
    for (jp = jpList; jp != NULL; jp = jp->next)
	{
	struct joinerDtf *dtf = jp->b;
	if (cartTrackDbIsAccessDenied(dtf->database, dtf->table))
	    continue;
	char buf[256];
	char *s;
	if (sameString(dtf->database, db))
	    s = dtf->table;
	else
	    {
	    safef(buf, sizeof(buf), "%s.%s", dtf->database, dtf->table);
	    s = buf;
	    }
	if (!hashLookup(uniqHash, s))
	    {
	    hashAdd(uniqHash, s, NULL);
	    name = slNameNew(s);
	    slAddHead(&nameList, name);
	    }
	}
    slNameSort(&nameList);
    }
/* suppress for parent tracks -- only the subtracks have tables */
if (track->subtracks == NULL)
    {
    name = slNameNew(trackTable);
    slAddHead(&nameList, name);
    }
addTablesAccordingToTrackType(db, &nameList, uniqHash, track);
hashFree(&uniqHash);
return nameList;
}
