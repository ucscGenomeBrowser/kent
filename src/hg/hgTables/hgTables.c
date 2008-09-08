/* hgTables - Main and utility functions for table browser. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "obscure.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "textOut.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "hCommon.h"
#include "hgColors.h"
#include "trackDb.h"
#include "botDelay.h"
#include "grp.h"
#include "customTrack.h"
#include "pipeline.h"
#include "hgFind.h"
#include "hgTables.h"
#include "joiner.h"
#include "bedCart.h"
#include "hgMaf.h"
#include "gvUi.h"
#include "wikiTrack.h"
#include "hgConfig.h"

static char const rcsid[] = "$Id: hgTables.c,v 1.166 2008/09/08 18:20:38 markd Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTables - Get table data associated with tracks and intersect tracks\n"
  "usage:\n"
  "   hgTables XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Global variables. */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldVars;	/* The cart before new cgi stuff added. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *database;		/* Current genome database - hg17, mm5, etc. */
char *freezeName;	/* Date of assembly. */
struct grp *fullGroupList;	/* List of all groups. */
struct grp *curGroup;	/* Currently selected group. */
struct trackDb *fullTrackList;	/* List of all tracks in database. */
struct trackDb *curTrack;	/* Currently selected track. */
char *curTable;		/* Currently selected table. */
struct joiner *allJoiner;	/* Info on how to join tables. */

static struct pipeline *compressPipeline = (struct pipeline *)NULL;

boolean allowAllTables(void)
/* determine if all tables should is allowed by configuration */
{
return !cfgOptionBooleanDefault("hgta.disableAllTables", FALSE);
}

/* --------------- HTML Helpers ----------------- */

void hPrintSpaces(int count)
/* Print a number of non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    hPrintf("&nbsp;");
}

static void vaHtmlOpen(char *format, va_list args)
/* Start up a page that will be in html format. */
{
puts("Content-Type:text/html\n");
cartVaWebStart(cart, database, format, args);
}

void htmlOpen(char *format, ...)
/* Start up a page that will be in html format. */
{
va_list args;
va_start(args, format);
vaHtmlOpen(format, args);
}

void htmlClose()
/* Close down html format page. */
{
cartWebEnd();
}

void explainWhyNoResults(FILE *f)
/* Put up a little explanation to user of why they got nothing. */
{
if (f == NULL)
    {
    hPrintf("# No results");
    if (identifierFileName() != NULL)
        hPrintf(" matching identifier list");
    if (anyFilter())
        hPrintf(" passing filter");
    if (!fullGenomeRegion())
        hPrintf(" in given region");
    if (anyIntersection())
        hPrintf(" after intersection");
    hPrintf(".");
    }
else
    {
    fprintf(f, "# No results");
    if (identifierFileName() != NULL)
        fprintf(f, " matching identifier list");
    if (anyFilter())
        fprintf(f, " passing filter");
    if (!fullGenomeRegion())
        fprintf(f, " in given region");
    if (anyIntersection())
        fprintf(f, " after intersection");
    fprintf(f, ".");
    }
}

char *curTableLabel()
/* Return label for current table - track short label if it's a track */
{
if (curTrack != NULL && sameString(curTrack->tableName, curTable))
    return curTrack->shortLabel;
else
    return curTable;
}

char *getScriptName()
/* returns script name from environment or hardcoded for command line */
{
char *script = cgiScriptName();
if (script != NULL)
    return script;
else
    return hgTablesName();
}

void textOpen()
/* Start up page in text format. (No need to close this).
 *	In case of pipeline output to a compressor, it is closed
 *	at main() exit.
 */
{
char *fileName = cartUsualString(cart, hgtaOutFileName, "");
char *compressType = cartUsualString(cart, hgtaCompressType,
				     textOutCompressNone);

compressPipeline = textOutInit(fileName, compressType);
}


void dbOverrideFromTable(char buf[256], char **pDb, char **pTable)
/* If *pTable includes database, overrider *pDb with it, using
 * buf to hold string. */
{
char *s;
safef(buf, 256, "%s", *pTable);
s = strchr(buf, '.');
if (s != NULL)
    {
    *pDb = buf;
    *s++ = 0;
    *pTable = s;
    }
}

struct trackDb *findCompositeTdb(struct trackDb *track, char *table)
/*	find the tdb for the table, if it is custom or composite or ordinary  */
{
struct trackDb *tdb = track;

if (isCustomTrack(table))
    {
    struct customTrack *ct = lookupCt(table);
    tdb = ct->tdb;
    }
else if (track && tdbIsComposite(track))
    {
    struct trackDb *subTdb;
    for (subTdb=track->subtracks; subTdb != NULL; subTdb = subTdb->next)
	{
	if (sameWord(subTdb->tableName, table))
	    {
	    tdb = subTdb;
	    break;
	    }
	}
    }
return(tdb);
}

static struct trackDb *getFullTrackList()
/* Get all tracks including custom tracks if any. */
{
struct trackDb *list = hTrackDb(database, NULL), *tdb;
struct customTrack *ctList, *ct;

/* Change the mrna track to all_mrna to avoid confusion elsewhere. */
for (tdb = list; tdb != NULL; tdb = tdb->next)
    {
    if (sameString(tdb->tableName, "mrna"))
        {
	tdb->tableName = cloneString("all_mrna");
	}
    }
/* add wikiTrack if enabled */
if (wikiTrackEnabled(database, NULL))
    wikiTrackDb(&list);

/* Create dummy group for custom tracks if any */
ctList = getCustomTracks();
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    slAddHead(&list, ct->tdb);
    }

return list;
}

boolean fullGenomeRegion()
/* Return TRUE if region is full genome. */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
return sameString(regionType, "genome");
}

static int regionCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,start */
{
const struct region *a = *((struct region **)va);
const struct region *b = *((struct region **)vb);
int dif;
dif = chrStrippedCmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

struct region *getRegionsFullGenome()
/* Get a region list that covers all of each chromosome. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
struct region *region, *regionList = NULL;

sr = sqlGetResult(conn, "select chrom,size from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->chrom = cloneString(row[0]);
    region->end = sqlUnsigned(row[1]);
    region->fullChrom = TRUE;
    region->name = NULL;		/* unused for full chrom */
    slAddHead(&regionList, region);
    }
slSort(&regionList, regionCmp);
sqlFreeResult(&sr);
hFreeConn(&conn);
return regionList;
}

struct region *getEncodeRegions()
/* Get encode regions from encodeRegions table. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char **row;
struct region *list = NULL, *region;

sr = sqlGetResult(conn, "select chrom,chromStart,chromEnd,name from encodeRegions order by name desc");
while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(region);
    region->chrom = cloneString(row[0]);
    region->start = atoi(row[1]);
    region->end = atoi(row[2]);
    region->name = cloneString(row[3]);	/* encode region name	*/
    slAddHead(&list, region);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return list;
}

boolean searchPosition(char *range, struct region *region)
/* Try and fill in region via call to hgFind. Return FALSE
 * if it can't find a single position. */
{
struct hgPositions *hgp = NULL;
char retAddr[512];
char position[512];
safef(retAddr, sizeof(retAddr), "%s", getScriptName());
hgp = findGenomePosWeb(database, range, &region->chrom, &region->start, &region->end,
	cart, TRUE, retAddr);
if (hgp != NULL && hgp->singlePos != NULL)
    {
    safef(position, sizeof(position),
	    "%s:%d-%d", region->chrom, region->start+1, region->end);
    cartSetString(cart, hgtaRange, position);
    return TRUE;
    }
else if (region->start == 0)	/* Confusing way findGenomePosWeb says pos not found. */
    {
    cartSetString(cart, hgtaRange, hDefaultPos(database));
    return FALSE;
    }
else
    return FALSE;
}

boolean lookupPosition()
/* Look up position (aka range) if need be.  Return FALSE if it puts
 * up multiple positions. */
{
char *range = cartUsualString(cart, hgtaRange, "");
boolean isSingle = TRUE;
range = trimSpaces(range);
if (range[0] != 0)
    {
    struct region r;
    isSingle = searchPosition(range, &r);
    }
else
    {
    cartSetString(cart, hgtaRange, hDefaultPos(database));
    }
return isSingle;
}

struct region *getRegions()
/* Consult cart to get list of regions to work on. */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
struct region *regionList = NULL, *region;
if (sameString(regionType, hgtaRegionTypeGenome))
    {
    regionList = getRegionsFullGenome();
    }
else if (sameString(regionType, hgtaRegionTypeRange))
    {
    char *range = cartString(cart, hgtaRange);
    boolean parseOk = FALSE;
    regionList = AllocVar(region);
    if (! strchr(range, ':'))
	parseOk = ((region->chrom = hgOfficialChromName(database, range)) != NULL);
    else
	parseOk = hgParseChromRange(database, range, &region->chrom, &region->start,
				    &region->end);
    if (! parseOk)
	{
	errAbort("Bad position %s, please enter something else in position box",
		 range);
	}
    region->name = NULL;	/*	unused for range	*/
    }
else if (sameString(regionType, hgtaRegionTypeEncode))
    {
    regionList = getEncodeRegions();
    }
else if (sameString(regionType, hgtaRegionTypeUserRegions) &&
	(NULL != userRegionsFileName()))
    {
    regionList = getUserRegions(userRegionsFileName());
    }
else
    {
    regionList = getRegionsFullGenome();
    }
return regionList;
}

char *getRegionName()
/* Get a name for selected region.  Don't free this. */
{
char *region = cartUsualString(cart, hgtaRegionType, "genome");
if (sameString(region, "range"))
    region = cartUsualString(cart, hgtaRange, "n/a");
return region;
}

struct sqlResult *regionQuery(struct sqlConnection *conn, char *table,
	char *fields, struct region *region, boolean isPositional,
	char *extraWhere)
/* Construct and execute query for table on region. Returns NULL if 
 * table doesn't exist (e.g. missing split table for region->chrom). */
{
struct sqlResult *sr;
if (isPositional)
    {
    /* Check for missing split tables before querying: */
    char *db = sqlGetDatabase(conn);
    struct hTableInfo *hti = hFindTableInfo(db, region->chrom, table);
    if (hti == NULL)
	return NULL;
    else if (hti->isSplit)
	{
	char fullTableName[256];
	safef(fullTableName, sizeof(fullTableName),
	      "%s_%s", region->chrom, table);
	if (!sqlTableExists(conn, fullTableName))
	    return NULL;
	}
    if (region->fullChrom) /* Full chromosome. */
	{
	sr = hExtendedChromQuery(conn, table, region->chrom, 
		extraWhere, FALSE, fields, NULL);
	}
    else
	{
	sr = hExtendedRangeQuery(conn, table, region->chrom, 
		region->start, region->end, 
		extraWhere, TRUE, fields, NULL);
	}
    }
else
    {
    struct dyString *query = dyStringNew(0);
    dyStringPrintf(query, "select %s from %s", fields, table);
    if (extraWhere)
         {
	 dyStringAppend(query, " where ");
	 dyStringAppend(query, extraWhere);
	 }
    sr = sqlGetResult(conn, query->string);
    dyStringFree(&query);
    }
return sr;
}

char *getDbTable(char *db, char *table)
/* If table already contains its real database as a dot-prefix, then 
 * return a clone of table; otherwise alloc and return db.table . */
{
if (sameString(table, WIKI_TRACK_TABLE))
    {
    char dbTable[256];
    safef(dbTable, sizeof(dbTable), "%s.%s", wikiDbName(), WIKI_TRACK_TABLE);
    return cloneString(dbTable);
    }
else if (strchr(table, '.') != NULL)
    return cloneString(table);
else
    {
    char dbTable[256];
    safef(dbTable, sizeof(dbTable), "%s.%s", db, table);
    return cloneString(dbTable);
    }
}

char *trackTable(char *rawTable)
/* Return table name for track, substituting all_mrna
 * for mRNA if need be. */
{
char *table = rawTable;
return table;
}

char *connectingTableForTrack(char *rawTable)
/* Return table name to use with all.joiner for track. 
 * You can freeMem this when done. */
{
if (sameString(rawTable, "mrna"))
    return cloneString("all_mrna");
else if (sameString(rawTable, "est"))
    return cloneString("all_est");
else 
    return cloneString(rawTable);
}

char *chromTable(struct sqlConnection *conn, char *table)
/* Get chr1_table if it exists, otherwise table. 
 * You can freeMem this when done. */
{
char *chrom = hDefaultChrom(database);
if (sqlTableExists(conn, table))
    return cloneString(table);
else
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s_%s", chrom, table);
    return cloneString(buf);
    }
}

char *chrnTable(struct sqlConnection *conn, char *table)
/* Return chrN_table if table is split, otherwise table. 
 * You can freeMem this when done. */
{
char buf[256];
char *splitTable = chromTable(conn, table);
if (!sameString(splitTable, table))
     {
     safef(buf, sizeof(buf), "chrN_%s", table);
     freez(&splitTable);
     return cloneString(buf);
     }
else
     return splitTable;
}

void checkTableExists(struct sqlConnection *conn, char *table)
/* Check that table exists, or put up an error message. */
{
char *splitTable = chromTable(conn, table);
if (!sqlTableExists(conn, table))
    errAbort("Table %s doesn't exist", table);
freeMem(splitTable);
}

struct hTableInfo *maybeGetHti(char *db, char *table)
/* Return primary table info. */
{
struct hTableInfo *hti = NULL;

if (isCustomTrack(table))
    {
    struct customTrack *ct = lookupCt(table);
    hti = ctToHti(ct);
    }
else if (sameWord(table, WIKI_TRACK_TABLE))
    {
    hti = wikiHti();
    }
else
    {
    char *track;
    if (startsWith("chrN_", table))
	track = table + strlen("chrN_");
    else
	track = table;
    hti = hFindTableInfo(db, NULL, track);
    }
return(hti);
}

struct hTableInfo *getHti(char *db, char *table)
/* Return primary table info. */
{
struct hTableInfo *hti = maybeGetHti(db, table);

if (hti == NULL)
    {
    errAbort("Could not find table info for table %s in db %s",
	     table, db);
    }
return(hti);
}

boolean isPositional(char *db, char *table)
/* Return TRUE if it looks to be a positional table. */
{
boolean result = FALSE;
struct sqlConnection *conn = sqlConnect(db);
if (sqlTableExists(conn, "chromInfo"))
    {
    char chromName[64];
    struct hTableInfo *hti;
    sqlQuickQuery(conn, "select chrom from chromInfo limit 1", 
	chromName, sizeof(chromName));
    hti = hFindTableInfo(db, chromName, table);
    if (hti != NULL)
	{
	result = htiIsPositional(hti);
	}
    }
sqlDisconnect(&conn);
return result;
}

boolean isSqlStringType(char *type)
/* Return TRUE if type is a stringish SQL type. */
{
return strstr(type, "char") || strstr(type, "text") 
       || strstr(type, "blob");
}

boolean isSqlEnumType(char *type)
/* Return TRUE if type is an enum. */
{
return startsWith("enum", type);
}

boolean isSqlSetType(char *type)
/* Return TRUE if type is a set. */
{
return startsWith("set", type);
}

boolean isSqlNumType(char *type)
/* Return TRUE if it is a numerical SQL type. */
{
return strstr(type, "int") || strstr(type, "float") || strstr(type, "double");
}

struct trackDb *findTrackInGroup(char *name, struct trackDb *trackList,
	struct grp *group)
/* Find named track that is in group (NULL for any group).
 * Return NULL if can't find it. */
{
struct trackDb *track;
if (group != NULL && sameString(group->name, "all"))
    group = NULL;
for (track = trackList; track != NULL; track = track->next)
    {
    if (sameString(name, track->tableName) &&
       (group == NULL || sameString(group->name, track->grp)))
       return track;
    }
return NULL;
}

struct trackDb *findTrack(char *name, struct trackDb *trackList)
/* Find track, or return NULL if can't find it. */
{
return findTrackInGroup(name, trackList, NULL);
}

struct trackDb *mustFindTrack(char *name, struct trackDb *trackList)
/* Find track or squawk and die. */
{
struct trackDb *track = findTrack(name, trackList);
if (track == NULL)
    {
    if (isCustomTrack(name))
        errAbort("Can't find custom track %s. "
	         "If it's been 8 hours since you accessed this track you "
		 "may just need to upload it again.", name);
    else
	errAbort("Track %s doesn't exist in database %s.", name, database);
    }
return track;
}

struct trackDb *findSelectedTrack(struct trackDb *trackList, 
	struct grp *group, char *varName)
/* Find selected track - from CGI variable if possible, else
 * via various defaults. */
{
char *name = cartOptionalString(cart, varName);
struct trackDb *track = NULL;

if (name != NULL)
    {
    /* getFullTrackList tweaks tdb->tableName mrna to all_mrna, so in
     * case mrna is passed in (e.g. from hgc link to schema page)
     * tweak it here too: */
    if (sameString(name, "mrna"))
	name = "all_mrna";
    track = findTrackInGroup(name, trackList, group);
    }
if (track == NULL)
    {
    if (group == NULL || sameString(group->name, "all"))
        track = trackList;
    else
	{
	for (track = trackList; track != NULL; track = track->next)
	    if (sameString(track->grp, group->name))
	         break;
	if (track == NULL)
	    internalErr();
	}
    }
return track;
}

struct grp *makeGroupList(struct sqlConnection *conn, 
	struct trackDb *trackList, boolean allTablesOk)
/* Get list of groups that actually have something in them. */
{
struct grp *groupsAll, *groupList = NULL, *group;
struct hash *groupsInTrackList = newHash(0);
struct hash *groupsInDatabase = newHash(0);
struct trackDb *track;

/* Stream throught track list building up hash of active groups. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInTrackList,track->grp))
        hashAdd(groupsInTrackList, track->grp, NULL);
    }

/* Scan through group table, putting in ones where we have data. */
groupsAll = hLoadGrps(database);
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

/* Do some error checking for tracks with group names that are
 * not in database.  Just warn about them. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInDatabase, track->grp))
         warn("Track %s has group %s, which isn't in grp table",
	 	track->tableName, track->grp);
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

static struct grp *findGroup(struct grp *groupList, char *name)
/* Return named group in list, or NULL if not found. */
{
struct grp *group;
for (group = groupList; group != NULL; group = group->next)
    if (sameString(name, group->name))
        return group;
return NULL;
}

struct grp *findSelectedGroup(struct grp *groupList, char *cgiVar)
/* Find user-selected group if possible.  If not then
 * go to various levels of defaults. */
{
char *defaultGroup = "genes";
char *name = cartUsualString(cart, cgiVar, defaultGroup);
struct grp *group = findGroup(groupList, name);
if (group == NULL)
    group = findGroup(groupList, defaultGroup);
if (group == NULL)
    group = groupList;
return group;
}


static void addTablesAccordingToTrackType(struct slName **pList, 
	struct hash *uniqHash, struct trackDb *track)
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
        struct consWiggle *wig, *wiggles = wigMafWiggles(database, track);
        slReverse(&wiggles);
        for (wig = wiggles; wig != NULL; wig = wig->next)
            {
            name = slNameNew(wig->table);
            slAddHead(pList, name);
            hashAdd(uniqHash, wig->table, NULL);
            }
	}
    if (tdbIsComposite(track))
        {
        struct trackDb *subTdb;
        struct slName *subList = NULL;
	slSort(&(track->subtracks), trackDbCmp);
        for (subTdb = track->subtracks; subTdb != NULL; subTdb = subTdb->next)
            {
	    name = slNameNew(subTdb->tableName);
	    slAddTail(&subList, name);
	    hashAdd(uniqHash, subTdb->tableName, NULL);
            }
        pList = slCat(pList, subList);
        }
    }
freez(&trackDupe);
}

struct slName *tablesForTrack(struct trackDb *track, boolean useJoiner)
/* Return list of all tables associated with track. */
{
struct hash *uniqHash = newHash(8);
struct slName *name, *nameList = NULL;
char *trackTable = track->tableName;

hashAdd(uniqHash, trackTable, NULL);
if (useJoiner)
    {
    struct joinerPair *jpList, *jp;
    jpList = joinerRelate(allJoiner, database, trackTable);
    for (jp = jpList; jp != NULL; jp = jp->next)
	{
	struct joinerDtf *dtf = jp->b;
	char buf[256];
	char *s;
	if (sameString(dtf->database, database))
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
name = slNameNew(trackTable);
if (!tdbIsComposite(track))
    /* suppress for composite tracks -- only the subtracks have tables */
    slAddHead(&nameList, name);
addTablesAccordingToTrackType(&nameList, uniqHash, track);
hashFree(&uniqHash);
return nameList;
}

static char *findSelectedTable(struct sqlConnection *conn, struct trackDb *track, char *var)
/* Find selected table.  Default to main track table if none
 * found. */
{
if (track == NULL)
    return cartString(cart, var);
else if (isCustomTrack(track->tableName))
    return track->tableName;
else
    {
    struct slName *tableList = tablesForTrack(track, TRUE);
    char *table = cartUsualString(cart, var, tableList->name);
    if (slNameInList(tableList, table))
	return table;
    return tableList->name;
    }
}



void addWhereClause(struct dyString *query, boolean *gotWhere)
/* Add where clause to query.  If already have a where clause
 * add 'and' to it. */
{
if (*gotWhere)
    {
    dyStringAppend(query, " and ");
    }
else
    {
    dyStringAppend(query, " where ");
    *gotWhere = TRUE;
    }
}

boolean htiIsPositional(struct hTableInfo *hti)
/* Return TRUE if hti looks like it's from a positional table. */
{
return isCustomTrack(hti->rootName) ||
    ((hti->startField[0] && hti->endField[0]) &&
	(hti->chromField[0] || sameString(hti->rootName, "gl")));
}

char *getIdField(char *db, struct trackDb *track, char *table, 
	struct hTableInfo *hti)
/* Get ID field for table, or NULL if none.  FreeMem result when done */
{
char *idField = NULL;
if (hti != NULL && hti->nameField[0] != 0)
    idField = cloneString(hti->nameField);
else if (track != NULL)
    {
    struct hTableInfo *trackHti = maybeGetHti(db, track->tableName);
    if (trackHti != NULL && isCustomTrack(table))
	idField = cloneString(trackHti->nameField);
    else if (hti != NULL && trackHti != NULL && trackHti->nameField[0] != 0)
        {
	struct joinerPair *jp, *jpList;
	jpList = joinerRelate(allJoiner, db, track->tableName);
	for (jp = jpList; jp != NULL; jp = jp->next)
	    {
	    if (sameString(jp->a->field, trackHti->nameField))
	        {
		if ( sameString(jp->b->database, db) 
		  && sameString(jp->b->table, table) )
		    {
		    idField = cloneString(jp->b->field);
		    break;
		    }
		}
	    }
	joinerPairFreeList(&jpList);
	}
    }
/* If we haven't found the answer but this looks like a non-positional table,
 * use the first field. */
if (idField == NULL && !isCustomTrack(table) && (hti == NULL || !hti->isPos))
    {
    struct sqlConnection *conn = hAllocConn(db);
    struct slName *fieldList = sqlListFields(conn, table);
    if (fieldList == NULL)
	errAbort("getIdField: Can't find fields of table %s", table);
    idField = cloneString(fieldList->name);
    slFreeList(&fieldList);
    hFreeConn(&conn);
    }
return idField;
}

int countTableColumns(struct sqlConnection *conn, char *table)
/* Count columns in table. */
{
char *splitTable = chromTable(conn, table);
int count = sqlCountColumnsInTable(conn, splitTable);
freez(&splitTable);
return count;
}

static void itemRgbDataOut(char **row, int lastCol, int itemRgbCol)
/*	display bed data, show "reserved" column as r,g,b	*/
{
int colIx;
int rgb;

/*	Print up to itemRgbCol	*/
for (colIx = 0; (colIx < itemRgbCol) && (colIx < lastCol); ++colIx)
    hPrintf("%s\t", row[colIx]);

/*	Print out the itemRgbCol column	*/
rgb = atoi(row[itemRgbCol]);
hPrintf("%d,%d,%d", (rgb & 0xff0000) >> 16,
	(rgb & 0xff00) >> 8, (rgb & 0xff));

/*	Print the rest if there are any	*/
if (itemRgbCol < lastCol)
    {
    hPrintf("\t");
    for (colIx = itemRgbCol+1; colIx < lastCol; ++colIx)
	hPrintf("%s\t", row[colIx]);
    hPrintf("%s\n", row[lastCol]);
    }
else
    hPrintf("\n");	/*	itemRgbCol was the last column	*/
}

static int itemRgbHeader(struct sqlResult *sr, int lastCol)
/*	print out bed header, recognize "reserved" column, return which
 *	column it is, or -1 if not found
 */
{
int colIx;
int ret = -1;
char *field = sqlFieldName(sr);
for (colIx = 0; colIx < lastCol; ++colIx)
    {
    if (sameWord("reserved",field))
	{
	hPrintf("itemRgb\t");
	ret = colIx;
	}
    else
	hPrintf("%s\t", field);
    field = sqlFieldName(sr);
    }
if (sameWord("reserved",field))
    {
    hPrintf("itemRgb\n");
    ret = lastCol;
    }
else
    hPrintf("%s\n", field);

return(ret);
}

static void doTabOutDb( char *db, char *table, 
	struct sqlConnection *conn, char *fields)
/* Do tab-separated output on fields of a single table. */
{
struct region *regionList = getRegions();
struct region *region;
struct hTableInfo *hti = NULL;
struct dyString *fieldSpec = newDyString(256);
struct hash *idHash = NULL;
int outCount = 0;
boolean isPositional;
int fieldCount;
char *idField;
boolean showItemRgb = FALSE;
int itemRgbCol = -1;	/*	-1 means not found	*/
boolean printedColumns = FALSE;

hti = getHti(db, table);
idField = getIdField(db, curTrack, table, hti);
showItemRgb=bedItemRgb(curTrack);	/* should we expect itemRgb */
					/*	instead of "reserved" */

/* If they didn't pass in a field list assume they want all fields. */
if (fields != NULL)
    {
    dyStringAppend(fieldSpec, fields);
    fieldCount = countChars(fields, ',') + 1;
    }
else
    {
    dyStringAppend(fieldSpec, "*");
    fieldCount = countTableColumns(conn, table);
    }

/* If can find id field for table then get
 * uploaded list of identifiers, create identifier hash
 * and add identifier column to end of result set. */
if (idField != NULL)
    {
    idHash = identifierHash(db, table);
    if (idHash != NULL)
	{
	dyStringAppendC(fieldSpec, ',');
	dyStringAppend(fieldSpec, idField);
	}
    }
isPositional = htiIsPositional(hti);

/* Loop through each region. */
for (region = regionList; region != NULL; region = region->next)
    {
    struct sqlResult *sr;
    char **row;
    int colIx, lastCol = fieldCount-1;
    char *filter = filterClause(db, table, region->chrom);

    sr = regionQuery(conn, table, fieldSpec->string, 
    	region, isPositional, filter);
    if (sr == NULL)
	continue;

    /* First time through print column names. */
    if (! printedColumns)
        {
	if (filter != NULL)
	    hPrintf("#filter: %s\n", filter);
	hPrintf("#");
	if (showItemRgb)
	    {
	    itemRgbCol = itemRgbHeader(sr, lastCol);
	    if (itemRgbCol == -1)
		showItemRgb = FALSE;	/*  did not find "reserved" */
	    }
	else
	    {
	    for (colIx = 0; colIx < lastCol; ++colIx)
		hPrintf("%s\t", sqlFieldName(sr));
	    hPrintf("%s\n", sqlFieldName(sr));
	    }
	printedColumns = TRUE;
	}
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (idHash == NULL || hashLookup(idHash, row[fieldCount]))
	    {
	    if (showItemRgb)
		itemRgbDataOut(row, lastCol, itemRgbCol);
	    else
		{
		for (colIx = 0; colIx < lastCol; ++colIx)
		    hPrintf("%s\t", row[colIx]);
		hPrintf("%s\n", row[lastCol]);
		}
	    ++outCount;
	    }
	}
    sqlFreeResult(&sr);
    if (!isPositional)
        break;	/* No need to iterate across regions in this case. */
    freez(&filter);
    }

/* Do some error diagnostics for user. */
if (outCount == 0)
    explainWhyNoResults(NULL);
hashFree(&idHash);
}

static void doTabOutDbFile( char *db, char *table, FILE *f,
        struct sqlConnection *conn, char *fields)
/* Do tab-separated output on fields of a single table, to a file. */
{
struct region *regionList = getRegions();
struct region *region;
struct hTableInfo *hti = NULL;
struct dyString *fieldSpec = newDyString(256);
struct hash *idHash = NULL;
int outCount = 0;
boolean isPositional;
int fieldCount;
char *idField;
boolean showItemRgb = FALSE;
int itemRgbCol = -1;	/*	-1 means not found	*/
boolean printedColumns = FALSE;

hti = getHti(db, table);
idField = getIdField(db, curTrack, table, hti);
showItemRgb=bedItemRgb(curTrack);	/* should we expect itemRgb */
					/*	instead of "reserved" */

/* If they didn't pass in a field list assume they want all fields. */
if (fields != NULL)
    {
    dyStringAppend(fieldSpec, fields);
    fieldCount = countChars(fields, ',') + 1;
    }
else
    {
    dyStringAppend(fieldSpec, "*");
    fieldCount = countTableColumns(conn, table);
    }

/* If can find id field for table then get
 * uploaded list of identifiers, create identifier hash
 * and add identifier column to end of result set. */
if (idField != NULL)
    {
    idHash = identifierHash(db, table);
    if (idHash != NULL)
	{
	dyStringAppendC(fieldSpec, ',');
	dyStringAppend(fieldSpec, idField);
	}
    }
isPositional = htiIsPositional(hti);

/* Loop through each region. */
for (region = regionList; region != NULL; region = region->next)
    {
    struct sqlResult *sr;
    char **row;
    int colIx, lastCol = fieldCount-1;
    char *filter = filterClause(db, table, region->chrom);

    sr = regionQuery(conn, table, fieldSpec->string, 
    	region, isPositional, filter);
    if (sr == NULL)
	continue;

    /* First time through print column names. */
    if (! printedColumns)
        {
	if (filter != NULL)
	    fprintf(f, "#filter: %s\n", filter);
	fprintf(f, "#");
	if (showItemRgb)
	    {
	    itemRgbCol = itemRgbHeader(sr, lastCol);
	    if (itemRgbCol == -1)
		showItemRgb = FALSE;	/*  did not find "reserved" */
	    }
	else
	    {
	    for (colIx = 0; colIx < lastCol; ++colIx)
		fprintf(f, "%s\t", sqlFieldName(sr));
	    fprintf(f, "%s\n", sqlFieldName(sr));
	    }
	printedColumns = TRUE;
	}
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (idHash == NULL || hashLookup(idHash, row[fieldCount]))
	    {
	    if (showItemRgb)
		itemRgbDataOut(row, lastCol, itemRgbCol);
	    else
		{
		for (colIx = 0; colIx < lastCol; ++colIx)
		    fprintf(f, "%s\t", row[colIx]);
		fprintf(f, "%s\n", row[lastCol]);
		}
	    ++outCount;
	    }
	}
    sqlFreeResult(&sr);
    if (!isPositional)
        break;	/* No need to iterate across regions in this case. */
    freez(&filter);
    }

/* Do some error diagnostics for user. */
if (outCount == 0)
    explainWhyNoResults(f);
hashFree(&idHash);
}

void doTabOutTable( char *db, char *table, FILE *f, struct sqlConnection *conn, char *fields)
/* Do tab-separated output on fields of a single table. */
{
if (isCustomTrack(table))
    {
    struct trackDb *track = findTrack(table, fullTrackList);
    doTabOutCustomTracks(track, conn, fields, f);
    }
else
    {
    if (f == NULL)
        doTabOutDb(db, table, conn, fields);
    else
        doTabOutDbFile(db, table, f, conn, fields);
    }
}

struct slName *fullTableFields(char *db, char *table)
/* Return list of fields in db.table.field format. */
{
char dtBuf[256];
struct sqlConnection *conn;
struct slName *fieldList = NULL, *dtfList = NULL, *field, *dtf;
if (isCustomTrack(table))
    {
    struct customTrack *ct = lookupCt(table);
    if ((ct!= NULL) && (ct->dbTrackType != NULL) &&
	    sameString(ct->dbTrackType, "maf"))
	{
	conn = hAllocConn(CUSTOM_TRASH);
	fieldList = sqlListFields(conn, ct->dbTableName);
	hFreeConn(&conn);
	}
    else
	{
	fieldList = getBedFields(ct->fieldCount);
	}
    }
else
    {
    char *splitTable;
    dbOverrideFromTable(dtBuf, &db, &table);
    conn = sqlConnect(db);
    splitTable = chromTable(conn, table);
    fieldList = sqlListFields(conn, splitTable);
    freez(&splitTable);
    sqlDisconnect(&conn);
    }
for (field = fieldList; field != NULL; field = field->next)
    {
    char buf[256];
    safef(buf, sizeof(buf), "%s.%s.%s", db, table, field->name);
    dtf = slNameNew(buf);
    slAddHead(&dtfList, dtf);
    }
slReverse(&dtfList);
slFreeList(&fieldList);
return dtfList;
}

void doOutPrimaryTable(char *table, struct sqlConnection *conn)
/* Dump out primary table. */
{
if (anySubtrackMerge(database, table))
    errAbort("Can't do all fields output when subtrack merge is on. "
    "Please go back and select another output type (BED or custom track is good), or clear the subtrack merge.");
if (anyIntersection())
    errAbort("Can't do all fields output when intersection is on. "
    "Please go back and select another output type (BED or custom track is good), or clear the intersection.");
textOpen();
if (sameWord(table, WIKI_TRACK_TABLE))
    tabOutSelectedFields(wikiDbName(), table, NULL, fullTableFields(wikiDbName(), table));
else
    tabOutSelectedFields(database, table, NULL, fullTableFields(database, table));
}

void doOutHyperlinks(char *table, struct sqlConnection *conn)
/* Output as genome browser hyperlinks. */
{
char *table2 = cartOptionalString(cart, hgtaIntersectTrack);
int outputPad = cartUsualInt(cart, hgtaOutputPad,0);
struct region *region, *regionList = getRegions();
char posBuf[64];
int count = 0;

htmlOpen("Hyperlinks to Genome Browser");
for (region = regionList; region != NULL; region = region->next)
    {
    struct lm *lm = lmInit(64*1024);
    struct bed *bedList, *bed;
    bedList = cookedBedList(conn, table, region, lm, NULL);
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	char *name;
        int start = max(0,bed->chromStart+1-outputPad);
        int end = min(hChromSize(database, bed->chrom),bed->chromEnd+outputPad);
	safef(posBuf, sizeof(posBuf), "%s:%d-%d",
		    bed->chrom, start, end);
	/* Construct browser anchor URL with tracks we're looking at open. */
        if (doGalaxy())
            {
            char *s, *script = hgTracksName();
            s = strstr(script, "cgi-bin");
            hPrintf("<A HREF=\"http://%s/%s?db=%s", cgiServerName(), s, database);
            }
        else
	    hPrintf("<A HREF=\"%s?db=%s", hgTracksName(), database);
	hPrintf("&position=%s", posBuf);
	hPrintf("&%s=%s", table, hTrackOpenVis(database, table));
	if (table2 != NULL)
	    hPrintf("&%s=%s", table2, hTrackOpenVis(database, table2));
	hPrintf("\" TARGET=_blank>");
	name = bed->name;
	if (bed->name == NULL)
	    name = posBuf;
	if (sameString(name, posBuf))
	    hPrintf("%s", posBuf);
	else
	    {
	    char *tmp = htmlEncode(name);
	    hPrintf("%s at %s", tmp, posBuf);
	    freeMem(tmp);
	    }
	hPrintf("</A><BR>\n");
	++count;
	}
    lmCleanup(&lm);
    }
if (count == 0)
    hPrintf("\n# No results returned from query.\n\n");
htmlClose();
}

void doLookupPosition(struct sqlConnection *conn)
/* Handle lookup button press.   The work has actually
 * already been done, so just call main page. */
{
doMainPage(conn);
}

/* Remove any meta data variables from the cart. (Copied from above!) */
void removeMetaData()
{
cartRemove(cart, "hgta_metaStatus");
cartRemove(cart, "hgta_metaVersion");
cartRemove(cart, "hgta_metaDatabases");
cartRemove(cart, "hgta_metaTables");
}

void doMetaData(struct sqlConnection *conn)
/* Get meta data for a database. */
{
puts("Content-Type:text/plain\n");
char *query = "";
if (cartVarExists(cart, hgtaMetaStatus))
    {
    printf("Table status for database %s\n", database);
    query = "SHOW TABLE STATUS";
    }
else if (cartVarExists(cart, hgtaMetaVersion))
    {
    query = "SELECT @@VERSION";
    }
else if (cartVarExists(cart, hgtaMetaDatabases))
    {
    query = "SHOW DATABASES";
    }
else if (cartVarExists(cart, hgtaMetaTables))
    {
    query = "SHOW TABLES";
    }
struct sqlResult *sr;
char **row;
char *sep="";
int c = 0;
int numCols = 0;
sr = sqlGetResult(conn, query);
numCols = sqlCountColumns(sr);
char *field;
while ((field = sqlFieldName(sr)) != NULL)
    printf("%s \t", field);
printf("\n");
while ((row = sqlNextRow(sr)) != NULL)
    {
    sep="";
    for (c=0;c<numCols;++c)
	{
	printf("%s%s",sep,row[c]);
	sep = "\t";
	}
    fprintf(stdout, "\n");	    
    }
sqlFreeResult(&sr);
removeMetaData();
}

void dispatch(struct sqlConnection *conn);

void doTopSubmit(struct sqlConnection *conn)
/* Respond to submit button on top level page.
 * This basically just dispatches based on output type. */
{
char *output = cartString(cart, hgtaOutputType);
char *trackName = NULL;
char *table = cartString(cart, hgtaTable);
struct trackDb *track = NULL;

hgBotDelay();
if (!sameString(curGroup->name, "allTables"))
    {
    trackName = cartString(cart, hgtaTrack);
    track = mustFindTrack(trackName, fullTrackList);
    }
else
    {
    struct trackDb *cTdb = NULL;
    track = hTrackDbForTrack(database, table);
    cTdb = hCompositeTrackDbForSubtrack(database, track);
    if (cTdb)
	track = cTdb;
    }
if (track != NULL)
    {
    if (sameString(track->tableName, "gvPos") &&
	!cartVarExists(cart, "gvDisclaimer"))
	{
	/* display disclaimer and add flag to cart, program exits from here */
	htmlSetBackground(hBackgroundImage());
	htmlStart("Table Browser");
	gvDisclaimer();
	}
    else if (sameString(track->tableName, "gvPos") &&
	     sameString(cartString(cart, "gvDisclaimer"), "Disagree"))
	{
	cartRemove(cart, "gvDisclaimer");
	cartRemove(cart, hgtaDoTopSubmit);
	cartSetString(cart, hgtaDoMainPage, "return to table browser");
	dispatch(conn);
	return;
	}
    }
if (sameString(output, outPrimaryTable))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutPrimaryTable(table, conn);
    }
else if (sameString(output, outSelectedFields))
    doOutSelectedFields(table, conn);
else if (sameString(output, outSequence))
    doOutSequence(conn);
else if (sameString(output, outBed))
    doOutBed(table, conn);
else if (sameString(output, outCustomTrack))
    doOutCustomTrack(table, conn);
else if (sameString(output, outGff))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutGff(table, conn); 
    }
else if (sameString(output, outHyperlinks))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutHyperlinks(table, conn); 
    }
else if (sameString(output, outWigData))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutWigData(track, table, conn);
    }
else if (sameString(output, outWigBed))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutWigBed(track, table, conn);
    }
else if (sameString(output, outMaf))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutMaf(track, table, conn); 
    }
else if (sameString(output, outChromGraphData))
    {
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoTopSubmit, "get output");
    else
        doOutChromGraphDataCt(track, table); 
    }
else
    errAbort("Don't know how to handle %s output yet", output);
}

void dispatch(struct sqlConnection *conn)
/* Scan for 'do' variables and dispatch to appropriate page-generator.
 * By default head to the main page. */
{
struct hashEl *varList;
if (cartVarExists(cart, hgtaDoTest))
    doTest();
else if (cartVarExists(cart, hgtaDoMainPage))
    doMainPage(conn);
else if (cartVarExists(cart, hgtaDoSchema))
    doSchema(conn);
else if (cartVarExists(cart, hgtaDoTopSubmit))
    doTopSubmit(conn);
else if (cartVarExists(cart, hgtaDoSummaryStats))
    doSummaryStats(conn);
else if (cartVarExists(cart, hgtaDoIntersectPage))
    doIntersectPage(conn);
else if (cartVarExists(cart, hgtaDoClearIntersect))
    doClearIntersect(conn);
else if (cartVarExists(cart, hgtaDoIntersectMore))
    doIntersectMore(conn);
else if (cartVarExists(cart, hgtaDoIntersectSubmit))
    doIntersectSubmit(conn);
else if (cartVarExists(cart, hgtaDoCorrelatePage))
    doCorrelatePage(conn);
else if (cartVarExists(cart, hgtaDoClearCorrelate))
    doClearCorrelate(conn);
else if (cartVarExists(cart, hgtaDoClearContinueCorrelate))
    doClearContinueCorrelate(conn);
else if (cartVarExists(cart, hgtaDoCorrelateMore))
    doCorrelateMore(conn);
else if (cartVarExists(cart, hgtaDoCorrelateSubmit))
    doCorrelateSubmit(conn);
else if (cartVarExists(cart, hgtaDoPasteIdentifiers))
    doPasteIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoClearPasteIdentifierText))
    doClearPasteIdentifierText(conn);
/* Respond to clear within paste identifier page. */
else if (cartVarExists(cart, hgtaDoPastedIdentifiers))
    doPastedIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoUploadIdentifiers))
    doUploadIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoClearIdentifiers))
    doClearIdentifiers(conn);
else if (cartVarExists(cart, hgtaDoFilterPage))
    doFilterPage(conn);
else if (cartVarExists(cart, hgtaDoFilterMore))
    doFilterMore(conn);
else if (cartVarExists(cart, hgtaDoFilterSubmit))
    doFilterSubmit(conn);
else if (cartVarExists(cart, hgtaDoClearFilter))
     doClearFilter(conn);
else if (cartVarExists(cart, hgtaDoSchemaTable))
    {
    doTableSchema( cartString(cart, hgtaDoSchemaDb), 
    	cartString(cart, hgtaDoSchemaTable), conn);
    }
else if (cartVarExists(cart, hgtaDoValueHistogram))
    doValueHistogram(cartString(cart, hgtaDoValueHistogram));
else if (cartVarExists(cart, hgtaDoValueRange))
    doValueRange(cartString(cart, hgtaDoValueRange));
else if (cartVarExists(cart, hgtaDoSelectFieldsMore))
    doSelectFieldsMore();
else if (cartVarExists(cart, hgtaDoPrintSelectedFields))
    doPrintSelectedFields();  
else if (cartVarExists(cart, hgtaDoGalaxySelectedFields))
    sendParamsToGalaxy(hgtaDoPrintSelectedFields, "get output");
else if ((varList = cartFindPrefix(cart, hgtaDoClearAllFieldPrefix)) != NULL)
    doClearAllField(varList->name + strlen(hgtaDoClearAllFieldPrefix));
else if ((varList = cartFindPrefix(cart, hgtaDoSetAllFieldPrefix)) != NULL)
    doSetAllField(varList->name + strlen(hgtaDoSetAllFieldPrefix));
else if (cartVarExists(cart, hgtaDoGenePredSequence))
    doGenePredSequence(conn);  
else if (cartVarExists(cart, hgtaDoGenomicDna))
    if (doGalaxy() && !cgiOptionalString(hgtaDoGalaxyQuery))
        sendParamsToGalaxy(hgtaDoGenomicDna, "submit");
    else
        doGenomicDna(conn);  
else if (cartVarExists(cart, hgtaDoGetBed))
    doGetBed(conn);  
else if (cartVarExists(cart, hgtaDoGetCustomTrackTb))
    doGetCustomTrackTb(conn);  
else if (cartVarExists(cart, hgtaDoGetCustomTrackGb))
    doGetCustomTrackGb(conn);
else if (cartVarExists(cart, hgtaDoGetCustomTrackFile))
    doGetCustomTrackFile(conn);  
else if (cartVarExists(cart, hgtaDoRemoveCustomTrack))
    doRemoveCustomTrack(conn);
else if (cartVarExists(cart, hgtaDoClearSubtrackMerge))
    doClearSubtrackMerge(conn);
/* Hack but I don't know what else to do now: hCompositeUi makes a hidden 
 * var for hgtaDoSubtrackMergePage, so that javascript can submit and it will 
 * look like that button was pressed.  However when the real submit button is 
 * pressed, it will look like both were pressed... so check the real submit 
 * button first (and check doMainPage before this too, for "cancel"!): */
else if (cartVarExists(cart, hgtaDoSubtrackMergeSubmit))
    doSubtrackMergeSubmit(conn);
else if (cartVarExists(cart, hgtaDoSubtrackMergePage))
    doSubtrackMergePage(conn);
else if (cartVarExists(cart, hgtaDoLookupPosition))
    doLookupPosition(conn);
else if (cartVarExists(cart, hgtaDoSetUserRegions))
    doSetUserRegions(conn);
else if (cartVarExists(cart, hgtaDoSubmitUserRegions))
    doSubmitUserRegions(conn);
else if (cartVarExists(cart, hgtaDoClearSetUserRegionsText))
    doClearSetUserRegionsText(conn);
else if (cartVarExists(cart, hgtaDoClearUserRegions))
    doClearUserRegions(conn);
else if (cartVarExists(cart, hgtaDoMetaData))
    doMetaData(conn);
else	/* Default - put up initial page. */
    doMainPage(conn);
cartRemovePrefix(cart, hgtaDo);
}

char *excludeVars[] = {"Submit", "submit", NULL};

void initGroupsTracksTables(struct sqlConnection *conn)
/* Get list of groups that actually have something in them. */
{
fullTrackList = getFullTrackList();
curTrack = findSelectedTrack(fullTrackList, NULL, hgtaTrack);
fullGroupList = makeGroupList(conn, fullTrackList, allowAllTables());
curGroup = findSelectedGroup(fullGroupList, hgtaGroup);
if (sameString(curGroup->name, "allTables"))
    curTrack = NULL;
curTable = findSelectedTable(conn, curTrack, hgtaTable);
if (curTrack == NULL)
    {
    struct trackDb *tdb = hTrackDbForTrack(database, curTable);
    struct trackDb *cTdb = hCompositeTrackDbForSubtrack(database, tdb);
    if (cTdb)
	curTrack = cTdb;
    else
	curTrack = tdb;
    }
}


void hgTables()
/* hgTables - Get table data associated with tracks and intersect tracks. 
 * Here we set up cart and some global variables, dispatch the command,
 * and put away the cart when it is done. */
{
struct sqlConnection *conn = NULL;
char *clade = NULL;

oldVars = hashNew(10);

/* Sometimes we output HTML and sometimes plain text; let each outputter 
 * take care of headers instead of using a fixed cart*Shell(). */
cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);

/* Set up global variables. */
allJoiner = joinerRead("all.joiner");
getDbGenomeClade(cart, &database, &genome, &clade, oldVars);
freezeName = hFreezeFromDb(database);
conn = hAllocConn(database);

if (lookupPosition())
    {
    /* Init track and group lists and figure out what page to put up. */
    initGroupsTracksTables(conn);
    dispatch(conn);
    }

/* Save variables. */
cartCheckout(&cart);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(LIMIT_2or6GB);
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
cgiSpoof(&argc, argv);

hgTables();

textOutClose(&compressPipeline);

return 0;
}
