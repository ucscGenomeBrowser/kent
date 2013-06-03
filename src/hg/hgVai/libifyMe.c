/* Stuff lifted from hgTables that should be libified. */
#include "common.h"
#include "cheapcgi.h"
#include "customTrack.h"
#include "grp.h"
#include "hdb.h"
#include "hgFind.h"
#include "hubConnect.h"
#include "hui.h"
#include "trackHub.h"
#include "wikiTrack.h"
#include "annoGratorQuery.h"
#include "annoGratorGpVar.h"
#include "annoStreamBigBed.h"
#include "annoStreamDb.h"
#include "annoStreamTab.h"
#include "annoStreamVcf.h"
#include "annoStreamWig.h"
#include "annoGrateWigDb.h"
#include "annoFormatTab.h"
#include "annoFormatVep.h"
#include "pgSnp.h"
#include "vcf.h"

#include "libifyMe.h"

static boolean searchPosition(char *range, struct cart *cart, char *cartVar)
/* Try and fill in region via call to hgFind. Return FALSE
 * if it can't find a single position. */
{
struct hgPositions *hgp = NULL;
char retAddr[512];
char position[512];
char *chrom = NULL;
int start=0, end=0;
char *db = cartString(cart, "db");
safef(retAddr, sizeof(retAddr), "%s", cgiScriptName());
hgp = findGenomePosWeb(db, range, &chrom, &start, &end,
	cart, TRUE, retAddr);
if (hgp != NULL && hgp->singlePos != NULL)
    {
    safef(position, sizeof(position),
	    "%s:%d-%d", chrom, start+1, end);
    cartSetString(cart, cartVar, position);
    return TRUE;
    }
else if (start == 0)	/* Confusing way findGenomePosWeb says pos not found. */
    {
    cartSetString(cart, cartVar, hDefaultPos(db));
    return FALSE;
    }
else
    return FALSE;
}

boolean lookupPosition(struct cart *cart, char *cartVar)
/* Look up position if it is not already seq:start-end.  Return FALSE if it puts
 * up multiple positions. */
{
char *db = cartString(cart, "db");
char *range = cartUsualString(cart, cartVar, "");
boolean isSingle = TRUE;
range = trimSpaces(range);
if (range[0] != 0)
    isSingle = searchPosition(range, cart, cartVar);
else
    cartSetString(cart, cartVar, hDefaultPos(db));
return isSingle;
}

void nbSpaces(int count)
/* Print some non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    printf("&nbsp;");
}

//#*** --------------------- almost verbatim from hgTables/custom.c (+ globals) -------------------------
static struct customTrack *getCustomTracks(char *database, struct cart *cart)
/* Get custom track list. */
{
static struct slName *browserLines = NULL;
static struct customTrack *theCtList = NULL;
if (theCtList == NULL)
    theCtList = customTracksParseCart(database, cart, &browserLines, NULL);
return theCtList;
}
//#*** --------------------- end verbatim from hgTables/custom.c -------------------------

boolean hasCustomTracks(struct cart *cart)
/* Return TRUE if cart has custom tracks for the current db. */
{
char *db = cartString(cart, "db");
struct customTrack *ctList = getCustomTracks(db, cart);
return (ctList != NULL);
}

//#*** libify to hg/lib/wikiTrack.c
//#*** should it sort trackDb, or leave that to the caller?
void wikiTrackDb(struct trackDb **list)
/* create a trackDb entry for the wiki track */
{
struct trackDb *tdb;

AllocVar(tdb);
tdb->track = WIKI_TRACK_TABLE;
tdb->table = WIKI_TRACK_TABLE;
tdb->shortLabel = WIKI_TRACK_LABEL;
tdb->longLabel = WIKI_TRACK_LONGLABEL;
tdb->visibility = tvFull;
tdb->priority = WIKI_TRACK_PRIORITY;

tdb->html = hFileContentsOrWarning(hHelpFile(tdb->track));
tdb->type = "none";
tdb->grp = "map";
tdb->canPack = FALSE;

slAddHead(list, tdb);
slSort(list, trackDbCmp);
}

static struct trackDb *getFullTrackList(struct cart *cart, struct hubConnectStatus *hubList,
					struct grp **pHubGroups)
/* Get all tracks including custom tracks if any. */
{
char *db = cartString(cart, "db");
struct trackDb *list = hTrackDb(db);
struct customTrack *ctList, *ct;

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
    if (tbOff == NULL || !startsWithWord("off", tbOff))
	slAddHead(&newList, tdb);
    }
slReverse(&newList);
list = newList;

/* add wikiTrack if enabled */
if (wikiTrackEnabled(db, NULL))
    wikiTrackDb(&list);

/* Add hub tracks. */
struct trackDb *hubTrackList = hubCollectTracks(db, pHubGroups);
if (hubTrackList != NULL)
    list = slCat(list, hubTrackList);

/* Add custom tracks to list */
ctList = getCustomTracks(db, cart);
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    slAddHead(&list, ct->tdb);
    }

return list;
}

static struct grp *makeGroupList(char *db, struct trackDb *trackList,
				 struct grp **pHubGrpList, boolean allTablesOk)
/* Get list of groups that actually have something in them. */
{
struct grp *groupsAll, *groupList = NULL, *group;
struct hash *groupsInTrackList = newHash(0);
struct hash *groupsInDb = newHash(0);
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
	hashAdd(groupsInDb, group->name, group);
	}
    else
        grpFree(&group);
    }

/* if we have custom tracks, we want to add the track hubs
 * after that group */
struct grp *addAfter = NULL;
if (sameString(groupList->name, "user"))
    addAfter = groupList;

/* Add in groups from hubs. */
for (group = slPopHead(pHubGrpList); group != NULL; group = slPopHead(pHubGrpList))
    {
    /* check to see if we're inserting hubs rather than
     * adding them to the front of the list */
    if (addAfter != NULL)
	{
	group->next = addAfter->next;
	addAfter->next = group;
	}
    else
	slAddHead(&groupList, group);
    hashAdd(groupsInDb, group->name, group);
    }

/* Do some error checking for tracks with group names that are
 * not in db.  Just warn about them. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInDb, track->grp))
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
hashFree(&groupsInDb);
return groupList;
}

void initGroupsTracksTables(struct cart *cart,
			    struct trackDb **retFullTrackList, struct grp **retFullGroupList)
/* Get lists of all tracks and of groups that actually have tracks in them. */
{
static boolean inited = FALSE;
static struct trackDb *fullTrackList = NULL;
static struct grp *fullGroupList = NULL;
if (! inited)
    {
    struct hubConnectStatus *hubList = hubConnectStatusListFromCart(cart);
    struct grp *hubGrpList = NULL;
    fullTrackList = getFullTrackList(cart, hubList, &hubGrpList);
    char *db= cartString(cart, "db");
    fullGroupList = makeGroupList(db, fullTrackList, &hubGrpList, FALSE);
    inited = TRUE;
    }
if (retFullTrackList != NULL)
    *retFullTrackList = fullTrackList;
if (retFullGroupList != NULL)
    *retFullGroupList = fullGroupList;
}

//#*** duplicated in hgVarAnnoGrator and annoGratorTester
struct annoAssembly *getAnnoAssembly(char *db)
/* Make annoAssembly for db. */
{
char *nibOrTwoBitDir = hDbDbNibPath(db);
if (nibOrTwoBitDir == NULL)
    errAbort("Can't find .2bit for db '%s'", db);
char twoBitPath[HDB_MAX_PATH_STRING];
safef(twoBitPath, sizeof(twoBitPath), "%s/%s.2bit", nibOrTwoBitDir, db);
return annoAssemblyNew(db, twoBitPath);
}

static boolean columnsMatch(struct asObject *asObj, struct sqlFieldInfo *fieldList)
/* Return TRUE if asObj's column names match the given SQL fields. */
{
if (asObj == NULL)
    return FALSE;
struct sqlFieldInfo *firstRealField = fieldList;
if (sameString("bin", fieldList->field) && differentString("bin", asObj->columnList->name))
    firstRealField = fieldList->next;
boolean columnsMatch = TRUE;
struct sqlFieldInfo *field = firstRealField;
struct asColumn *asCol = asObj->columnList;
for (;  field != NULL && asCol != NULL;  field = field->next, asCol = asCol->next)
    {
    if (!sameString(field->field, asCol->name))
	{
	columnsMatch = FALSE;
	break;
	}
    }
if (field != NULL || asCol != NULL)
    columnsMatch = FALSE;
return columnsMatch;
}

static struct asObject *asObjectFromFields(char *name, struct sqlFieldInfo *fieldList)
/* Make autoSql text from SQL fields and pass it to asParse. */
{
struct dyString *dy = dyStringCreate("table %s\n"
				     "\"Column names grabbed from mysql\"\n"
				     "    (\n", name);
struct sqlFieldInfo *field;
for (field = fieldList;  field != NULL;  field = field->next)
    {
    char *sqlType = field->type;
    // hg19.wgEncodeOpenChromSynthGm12878Pk.pValue has sql type "float unsigned",
    // and I'd rather pretend it's just a float than work unsigned floats into autoSql.
    if (sameString(sqlType, "float unsigned"))
	sqlType = "float";
    char *asType = asTypeNameFromSqlType(sqlType);
    if (asType == NULL)
	errAbort("No asTypeInfo for sql type '%s'!", field->type);
    dyStringPrintf(dy, "    %s %s;\t\"\"\n", asType, field->field);
    }
dyStringAppend(dy, "    )\n");
return asParseText(dy->string);
}

static struct asObject *getAutoSqlForTable(char *db, char *dataDb, char *dbTable,
					   struct trackDb *tdb)
/* Get autoSql for dataDb.dbTable from tdb and/or db.tableDescriptions;
 * if it doesn't match columns, make one up from dataDb.table sql fields. */
//#*** should we just always use sql fields?
{
struct sqlConnection *connDataDb = hAllocConn(dataDb);
struct sqlFieldInfo *fieldList = sqlFieldInfoGet(connDataDb, dbTable);
hFreeConn(&connDataDb);
struct asObject *asObj = NULL;
if (tdb != NULL)
    {
    struct sqlConnection *connDb = hAllocConn(db);
    asObj = asForTdb(connDb, tdb);
    hFreeConn(&connDb);
    }
if (columnsMatch(asObj, fieldList))
    return asObj;
else
    return asObjectFromFields(dbTable, fieldList);
}

static char *getBigDataFileName(char *db, struct trackDb *tdb, char *selTable, char *chrom)
/* Get fileName from bigBed/bigWig/BAM/VCF database table, or bigDataUrl from custom track. */
{
struct sqlConnection *conn = hAllocConn(db);
char *fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, selTable, chrom);
hFreeConn(&conn);
return fileOrUrl;
}

struct annoStreamer *streamerFromTrack(struct annoAssembly *assembly, char *selTable,
				       struct trackDb *tdb, char *chrom, int maxOutRows)
/* Figure out the source and type of data and make an annoStreamer. */
{
struct annoStreamer *streamer = NULL;
char *db = assembly->name, *dataDb = db, *dbTable = selTable;
if (chrom == NULL)
    chrom = hDefaultChrom(db);
if (isCustomTrack(selTable))
    {
    dbTable = trackDbSetting(tdb, "dbTableName");
    if (dbTable != NULL)
	// This is really a database table, not a bigDataUrl CT.
	dataDb = CUSTOM_TRASH;
    }
if (startsWith("wig", tdb->type))
    streamer = annoStreamWigDbNew(dataDb, dbTable, assembly, maxOutRows);
else if (sameString("vcfTabix", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(db, tdb, selTable, chrom);
    streamer = annoStreamVcfNew(fileOrUrl, TRUE, assembly, maxOutRows);
    }
else if (sameString("bam", tdb->type))
    {
    warn("Sorry, BAM is not yet supported");
    }
else if (sameString("bigBed", tdb->type))
    {
    char *fileOrUrl = getBigDataFileName(db, tdb, selTable, chrom);
    streamer = annoStreamBigBedNew(fileOrUrl, assembly, maxOutRows);
    }
else
    {
    struct sqlConnection *conn = hAllocConn(dataDb);
    char maybeSplitTable[1024];
    if (sqlTableExists(conn, dbTable))
	safecpy(maybeSplitTable, sizeof(maybeSplitTable), dbTable);
    else
	safef(maybeSplitTable, sizeof(maybeSplitTable), "%s_%s", chrom, dbTable);
    hFreeConn(&conn);
    struct asObject *asObj = getAutoSqlForTable(db, dataDb, maybeSplitTable, tdb);
    streamer = annoStreamDbNew(dataDb, maybeSplitTable, assembly, asObj, maxOutRows);
    }
return streamer;
}

struct annoGrator *gratorFromTrack(struct annoAssembly *assembly, char *selTable,
				   struct trackDb *tdb, char *chrom, int maxOutRows,
				   struct asObject *primaryAsObj,
				   enum annoGratorOverlap overlapRule)
/* Figure out the source and type of data, make an annoStreamer & wrap in annoGrator.
 * If not NULL, primaryAsObj is used to determine whether we can make an annoGratorGpVar. */
{
struct annoGrator *grator = NULL;
char *dataDb = assembly->name, *dbTable = selTable;
if (isCustomTrack(selTable))
    {
    dataDb = CUSTOM_TRASH;
    dbTable = trackDbSetting(tdb, "dbTableName");
    if (dbTable == NULL)
	errAbort("Can't find dbTableName for custom track %s", selTable);
    }
if (startsWith("wig", tdb->type))
    {
    grator = annoGrateWigDbNew(dataDb, dbTable, assembly, maxOutRows);
    }
else
    {
    struct annoStreamer *streamer = streamerFromTrack(assembly, dbTable, tdb, chrom, maxOutRows);
    if (primaryAsObj != NULL &&
	(asObjectsMatch(primaryAsObj, pgSnpAsObj()) || asObjectsMatch(primaryAsObj, vcfAsObj()))
	&& asObjectsMatchFirstN(streamer->asObj, genePredAsObj(), 10))
	grator = annoGratorGpVarNew(streamer);
    else
	grator = annoGratorNew(streamer);
    }
grator->setOverlapRule(grator, overlapRule);
return grator;
}

