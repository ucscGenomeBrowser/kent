/* Import - put up import pages and sub-pages. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "ra.h"
#include "portable.h"
#include "cheapcgi.h"
#include "localmem.h"
#include "cart.h"
#include "web.h"
#include "chromInfo.h"
#include "chromGraph.h"
#include "chromGraphFactory.h"
#include "errCatch.h"
#include "hPrint.h"
#include "customTrack.h"
#include "trashDir.h"
#include "hgGenome.h"

#include "jsHelper.h"
#include "grp.h"
#include "hdb.h"
#include "joiner.h"
#include "hgMaf.h"

#include "wiggle.h"


/* from hgTables.c */

struct grp *fullGroupList;      /* List of all groups. */
struct grp *curGroup;   /* Currently selected group. */
struct trackDb *fullTrackList;  /* List of all tracks in database. */
struct trackDb *curTrack;       /* Currently selected track. */
char *curTable;         /* Currently selected table. */
struct joiner *allJoiner;       /* Info on how to join tables. */


char *getScriptName()
/* returns script name from environment or hardcoded for command line */
{
char *script = cgiScriptName();
if (script != NULL)
    return script;
else
    return "hgGenome";
}

struct slName *getChroms()
/* Get a chrom list. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char **row;
struct slName *chromList = NULL;
sr = sqlGetResult(conn, "select chrom from chromInfo");
while ((row = sqlNextRow(sr)) != NULL)
    {
    slNameAddHead(&chromList, cloneString(row[0]));
    }
slReverse(&chromList);
sqlFreeResult(&sr);
hFreeConn(&conn);
return chromList;
}


struct sqlResult *chromQuery(struct sqlConnection *conn, char *table,
        char *fields, char *chrom, boolean isPositional,
        char *extraWhere)
/* Construct and execute query for table on chrom. Returns NULL if
 * table doesn't exist (e.g. missing split table for chrom). */
{
struct sqlResult *sr;
if (isPositional)
    {
    /* Check for missing split tables before querying: */
    char *db = sqlGetDatabase(conn);
    struct hTableInfo *hti = hFindTableInfoDb(db, chrom, table);
    if (hti == NULL)
        return NULL;
    else if (hti->isSplit)
        {
        char fullTableName[256];
        safef(fullTableName, sizeof(fullTableName),
              "%s_%s", chrom, table);
        if (!sqlTableExists(conn, fullTableName))
            return NULL;
        }
    sr = hExtendedChromQuery(conn, table, chrom,
	    extraWhere, FALSE, fields, NULL);
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

struct hTableInfo *maybeGetHti(char *db, char *table)
/* Return primary table info. */
{
struct hTableInfo *hti = NULL;

if (isCustomTrack(table))
    {
    struct customTrack *ct = lookupCt(table);
    hti = ctToHti(ct);
    }
else
    {
    char *track;
    if (startsWith("chrN_", table))
        track = table + strlen("chrN_");
    else
        track = table;
    hti = hFindTableInfoDb(db, NULL, track);
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
groupsAll = hLoadGrps();
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



struct trackDb *getFullTrackList()
/* Get all tracks including custom tracks if any. */
{
struct trackDb *list = hTrackDb(NULL), *tdb, *next;
struct customTrack *ctList, *ct;


for (tdb = list; tdb != NULL; tdb = next)
    {
    next = tdb->next;
    /* Change the mrna track to all_mrna to avoid confusion elsewhere. */
    if (sameString(tdb->tableName, "mrna"))
        {
        tdb->tableName = cloneString("all_mrna");
        }
    }

/* Create dummy group for custom tracks if any */
ctList = getCustomTracks();
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    if (!isChromGraph(ct->tdb))  /* filter out chromGraph custom tracks */
    	slAddHead(&list, ct->tdb);
    }
return list;
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
        struct consWiggle *wig, *wiggles = wigMafWiggles(track);
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




void initGroupsTracksTables(struct sqlConnection *conn)
/* Get list of groups that actually have something in them. */
{

fullTrackList = getFullTrackList();

curTrack = findSelectedTrack(fullTrackList, NULL, hggTrack);

fullGroupList = makeGroupList(conn, fullTrackList, TRUE);

curGroup = findSelectedGroup(fullGroupList, hggGroup);

if (sameString(curGroup->name, "allTables"))
    curTrack = NULL;

curTable = findSelectedTable(conn, curTrack, hggTable);

if (curTrack == NULL)
    {
    struct trackDb *tdb = hTrackDbForTrack(curTable);
    struct trackDb *cTdb = hCompositeTrackDbForSubtrack(tdb);
    if (cTdb)
        curTrack = cTdb;
    else
        curTrack = tdb;
    }

}




/* --------------------- */

void nbSpaces(int count)
/* Print some non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    hPrintf("&nbsp;");
}



static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = jsOnChangeStart();
jsDropDownCarryOver(dy, hggGroup);
jsDropDownCarryOver(dy, hggTrack);
dyStringPrintf(dy, "document.hiddenForm.%s.value=%s;",
	hggBedConvertType,
	hggBedConvertTypeJs);
return dy;
}



static char *onChangeGroupOrTrack()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
//jsDropDownCarryOver(dy, "clade");
//jsDropDownCarryOver(dy, "db");
//jsDropDownCarryOver(dy, "org");
dyStringPrintf(dy, " document.hiddenForm.%s.value=0;", hggTable);
return jsOnChangeEnd(&dy);
}


static char *onChangeTable()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
//jsDropDownCarryOver(dy, "clade");
//jsDropDownCarryOver(dy, "db");
//jsDropDownCarryOver(dy, "org");
jsDropDownCarryOver(dy, hggTable);
return jsOnChangeEnd(&dy);
}


struct grp *showGroupField(char *groupVar, char *groupScript,
    struct sqlConnection *conn, boolean allTablesOk)
/* Show group control. Returns selected group. */
{
struct grp *group, *groupList = makeGroupList(conn, fullTrackList, allTablesOk);
struct grp *selGroup = findSelectedGroup(groupList, groupVar);
hPrintf("<B>group:</B>\n");
hPrintf("<SELECT NAME=%s %s>\n", groupVar, groupScript);
for (group = groupList; group != NULL; group = group->next)
    {
    hPrintf(" <OPTION VALUE=%s%s>%s\n", group->name,
        (group == selGroup ? " SELECTED" : ""),
        group->label);
    }
hPrintf("</SELECT>\n");
return selGroup;
}


static void addIfExists(struct hash *hash, struct slName **pList, char *name)
/* Add name to tail of list if it exists in hash. */
{
if (hashLookup(hash, name))
    slNameAddTail(pList, name);
}



struct slName *getDbListForGenome()
/* Get list of selectable databases. */
{
struct hash *hash = sqlHashOfDatabases();
struct slName *dbList = NULL;
addIfExists(hash, &dbList, database);
/* currently filtering these out for hgGenome 
addIfExists(hash, &dbList, "swissProt");
addIfExists(hash, &dbList, "proteins");
addIfExists(hash, &dbList, "uniProt");
addIfExists(hash, &dbList, "proteome");
addIfExists(hash, &dbList, "go");
addIfExists(hash, &dbList, "hgFixed");
addIfExists(hash, &dbList, "visiGene");
addIfExists(hash, &dbList, "ultra");
*/
return dbList;
}




char *findSelDb()
/* Find user selected database (as opposed to genome database). */
{
struct slName *dbList = getDbListForGenome();
char *selDb = cartUsualString(cart, hggTrack, NULL);
if (!slNameInList(dbList, selDb))
    selDb = cloneString(dbList->name);
slFreeList(&dbList);
return selDb;
}

int trackDbCmpShortLabel(const void *va, const void *vb)
/* Sort track by shortLabel. */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
return strcmp(a->shortLabel, b->shortLabel);
}



struct trackDb *showTrackField(struct grp *selGroup,
        char *trackVar, char *trackScript)
/* Show track control. Returns selected track. */
{
struct trackDb *track, *selTrack = NULL;
if (trackScript == NULL)
    trackScript = "";
if (sameString(selGroup->name, "allTables"))
    {
    char *selDb = findSelDb();
    struct slName *dbList = getDbListForGenome(), *db;
    hPrintf("<B>database:</B>\n");
    hPrintf("<SELECT NAME=%s %s>\n", trackVar, trackScript);
    for (db = dbList; db != NULL; db = db->next)
        {
        hPrintf(" <OPTION VALUE=%s%s>%s\n", db->name,
                (sameString(db->name, selDb) ? " SELECTED" : ""),
                db->name);
        }
    hPrintf("</SELECT>\n");
    }
else
    {
    boolean allTracks = sameString(selGroup->name, "allTracks");
    hPrintf("<B>track:</B>\n");
    hPrintf("<SELECT NAME=\"%s\" %s>\n", trackVar, trackScript);
    if (allTracks)
        {
        selTrack = findSelectedTrack(fullTrackList, NULL, trackVar);
        slSort(&fullTrackList, trackDbCmpShortLabel);
        }
    else
        {
        selTrack = findSelectedTrack(fullTrackList, selGroup, trackVar);
        }
    for (track = fullTrackList; track != NULL; track = track->next)
        {
        if (allTracks || sameString(selGroup->name, track->grp))
            {
            hPrintf(" <OPTION VALUE=\"%s\"%s>%s\n", track->tableName,
                (track == selTrack ? " SELECTED" : ""),
                track->shortLabel);
            }
        }
    hPrintf("</SELECT>\n");
    }
hPrintf("\n");
return selTrack;
}


struct trackDb *findCompositeTdb(struct trackDb *track, char *table)
/*      find the tdb for the table, if it is custom or composite or ordinary  */
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



char *unsplitTableName(char *table)
/* Convert chr*_name to name */
{
if (startsWith("chr", table))
    {
    char *s = strrchr(table, '_');
    if (s != NULL)
        {
        table = s + 1;
        }
    }
return table;
}


static char *chopAtFirstDot(char *string)
/* Terminate string at first '.' if found.  Return string for convenience. */
{
char *ptr = strchr(string, '.');
if (ptr != NULL)
    *ptr = '\0';
return string;
}



static struct hash *accessControlInit(char *db, struct sqlConnection *conn)
/* Return a hash associating restricted table/track names in the given db/conn
 * with virtual hosts, or NULL if there is no tableAccessControl table. */
{
struct hash *acHash = NULL;
if (sqlTableExists(conn, "tableAccessControl"))
    {
    struct sqlResult *sr = NULL;
    char **row = NULL;
    acHash = newHash(8);
    sr = sqlGetResult(conn, "select name,host from tableAccessControl");
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct slName *sln = slNameNew(chopAtFirstDot(row[1]));
	struct hashEl *hel = hashLookup(acHash, row[0]);
	if (hel == NULL)
	    hashAdd(acHash, row[0], sln);
	else
	    slAddHead(&(hel->val), sln);
	}
    sqlFreeResult(&sr);
    }
return acHash;
}

static boolean accessControlDenied(struct hash *acHash, char *table)
/* Return TRUE if table access is restricted to some host(s) other than 
 * the one we're running on. */
{
static char *currentHost = NULL;
struct slName *enabledHosts = NULL;
struct slName *sln = NULL;

if (acHash == NULL)
    return FALSE;
enabledHosts = (struct slName *)hashFindVal(acHash, table);
if (enabledHosts == NULL)
    return FALSE;
if (currentHost == NULL)
    {
    currentHost = cloneString(cgiServerName());
    if (currentHost == NULL)
	{
	warn("accessControl: unable to determine current host");
	return FALSE;
	}
    else
	chopAtFirstDot(currentHost);
    }
for (sln = enabledHosts;  sln != NULL;  sln = sln->next)
    {
    if (sameString(currentHost, sln->name))
	return FALSE;
    }
return TRUE;
}

static void freeHelSlNameList(struct hashEl *hel)
/* Helper function for hashTraverseEls, to free slNameList vals. */
{
slNameFreeList(&(hel->val));
}

static void accessControlFree(struct hash **pAcHash)
/* Free up access control hash. */
{
if (*pAcHash != NULL)
    {
    hashTraverseEls(*pAcHash, freeHelSlNameList);
    freeHash(pAcHash);
    }
}


struct slName *tablesForDb(char *db)
/* Find tables associated with database. */
{
boolean isGenomeDb = sameString(db, database);
struct sqlConnection *conn = sqlConnect(db);
struct slName *raw, *rawList = sqlListTables(conn);
struct slName *cooked, *cookedList = NULL;
struct hash *uniqHash = newHash(0);
struct hash *accessCtlHash = accessControlInit(db, conn);

sqlDisconnect(&conn);
for (raw = rawList; raw != NULL; raw = raw->next)
    {
    if (isGenomeDb)
	{
	/* Deal with tables split across chromosomes. */
	char *root = unsplitTableName(raw->name);
	if (accessControlDenied(accessCtlHash, root) ||
	    accessControlDenied(accessCtlHash, raw->name))
	    continue;
	if (!hashLookup(uniqHash, root))
	    {
	    hashAdd(uniqHash, root, NULL);
	    cooked = slNameNew(root);
	    slAddHead(&cookedList, cooked);
	    }
	}
    else
        {
	char dbTable[256];
	if (accessControlDenied(accessCtlHash, raw->name))
	    continue;
	safef(dbTable, sizeof(dbTable), "%s.%s", db, raw->name);
	cooked = slNameNew(dbTable);
	slAddHead(&cookedList, cooked);
	}
    }
hashFree(&uniqHash);
accessControlFree(&accessCtlHash);
slFreeList(&rawList);
slSort(&cookedList, slNameCmp);
return cookedList;
}

boolean htiIsPositional(struct hTableInfo *hti)
/* Return TRUE if hti looks like it's from a positional table. */
{
return isCustomTrack(hti->rootName) ||
    ((hti->startField[0] && hti->endField[0]) &&
        (hti->chromField[0] || sameString(hti->rootName, "gl")));
}



char *showTableField(struct trackDb *track, char *varName, boolean useJoiner)
/* Show table control and label. */
{
struct slName *name, *nameList = NULL, *newList = NULL, *next = NULL;
char *selTable;

if (track == NULL)
    nameList = tablesForDb(findSelDb());
else
    nameList = tablesForTrack(track, useJoiner);


/* filter out non-positional tables, chains and nets, and other untouchables */
for (name = nameList; name != NULL; name = next)
    {

    next = name->next;
    boolean isCt = isCustomTrack(name->name);

    if (strchr(name->name, '.'))
	    continue;  /* filter out anything starting with database, e.g. db.name (like Locus Variants) */
    if (!isCt)  /* all custom tracks are positional */
	{
    	struct hTableInfo *hti = hFindTableInfoDb(database, NULL, name->name);
	if (!hti)
	    continue;  /* filter out tables that don't exist anymore */
	if (!htiIsPositional(hti))
	    continue;  /* filter out non-positional, do not add it to new list */
	}
    slAddHead(&newList,name);
    }
slReverse(&newList);
nameList = newList;

/* Get currently selected table.  If it isn't in our list
 * then revert to first in list. */
selTable = cartUsualString(cart, varName, nameList->name);
if (!slNameInList(nameList, selTable))
    selTable = nameList->name;

/* Print out label and drop-down list. */
hPrintf("<B>table: </B>");
hPrintf("<SELECT NAME=\"%s\" %s>\n", varName, onChangeTable());
for (name = nameList; name != NULL; name = name->next)
    {
    struct trackDb *tdb = NULL;
    if (track != NULL)
        tdb = findCompositeTdb(track, name->name);
    hPrintf("<OPTION VALUE=\"%s\"", name->name);
    if (sameString(selTable, name->name))
        hPrintf(" SELECTED");
    if (tdb != NULL)
        if ((curTrack == NULL) || differentWord(tdb->shortLabel, curTrack->shortLabel))
            hPrintf(">%s (%s)\n", tdb->shortLabel, name->name);
        else
            hPrintf(">%s\n", name->name);
    else
        hPrintf(">%s\n", name->name);
    }
hPrintf("</SELECT>\n");
return selTable;
}
                  


void importPage(struct sqlConnection *conn)
/* Put up initial import page. */
{
struct grp *selGroup;
boolean isWig = FALSE, isPositional = FALSE, isMaf = FALSE, isBedGr = FALSE,
        isChromGraphCt = FALSE;
struct hTableInfo *hti = NULL;


cartWebStart(cart, "Import Table to Genome Graphs");
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\" NAME=\"mainForm\" METHOD=\"POST\">");
cartSaveSession(cart);

jsWriteFunctions();


allJoiner = joinerRead("all.joiner");
initGroupsTracksTables(conn);

hPrintf("<TABLE BORDER=0>\n");


/* Print group and track line. */

hPrintf("<TR><TD>");

selGroup = showGroupField(hggGroup, onChangeGroupOrTrack(), conn, TRUE);

nbSpaces(3);
curTrack = showTrackField(selGroup, hggTrack, onChangeGroupOrTrack());
hPrintf("</TD></TR>\n");



/* Print table line. */
hPrintf("<TR><TD>");
curTable = showTableField(curTrack, hggTable, TRUE);

if (strchr(curTable, '.') == NULL)  /* In same database */
    {
    hti = getHti(database, curTable);
    isPositional = htiIsPositional(hti);
    }
isWig = isWiggle(database, curTable);
isMaf = isMafTable(database, curTrack, curTable);
isBedGr = isBedGraph(curTable);
nbSpaces(1);
if (isCustomTrack(curTable))
    {
    isChromGraphCt = isChromGraph(curTrack);
    }

hPrintf("</TD></TR>\n");


if (curTrack == NULL)
    {
    struct trackDb *tdb = hTrackDbForTrack(curTable);
    struct trackDb *cTdb = hCompositeTrackDbForSubtrack(tdb);
    if (cTdb)
	curTrack = cTdb;
    else
	curTrack = tdb;
    isMaf = isMafTable(database, curTrack, curTable);
    }

/* debug info
hPrintf("<TR><TD>");
hPrintf("<BR>\n");
hPrintf("Debug Info:<br>\n");
hPrintf("----------------------------<br>\n");
hPrintf("isWig: %d<br>\n", isWig);
hPrintf("isPositional: %d<br>\n", isPositional);
hPrintf("isMaf: %d<br>\n", isMaf);
hPrintf("isBedGr: %d<br>\n", isBedGr);
hPrintf("isChromGraphCt: %d<br>\n", isChromGraphCt);
hPrintf("</TD></TR>\n");
*/

hPrintf("<TR><TD>");

hPrintf("<BR>\n");
hPrintf("name of data set: ");
cartMakeTextVar(cart, hggDataSetName, "", 16);
hPrintf("<BR>");

hPrintf("description: ");
cartMakeTextVar(cart, hggDataSetDescription, "", 64);
hPrintf("<BR>\n");

hPrintf("display min value: ");
cartMakeTextVar(cart, hggMinVal, "", 5);
hPrintf(" max value: ");
cartMakeTextVar(cart, hggMaxVal, "", 5);
hPrintf("<BR>\n");

hPrintf("label values: ");
cartMakeTextVar(cart, hggLabelVals, "", 32);
hPrintf("<BR>\n");
hPrintf("draw connecting lines between markers separated by up to ");
cartMakeIntVar(cart, hggMaxGapToFill, 25000000, 8);
hPrintf(" bases.<BR>");
hPrintf(" ");

char *convertType = 
   cartUsualString(cart, hggBedConvertType, hggBedDepth);

jsTrackingVar(hggBedConvertTypeJs, convertType);

if (isPositional && !isWig && !isMaf && !isBedGr && !isChromGraphCt)
    {
    hPrintf("conversion type:&nbsp;");

    jsMakeTrackingRadioButton(hggBedConvertType, hggBedConvertTypeJs,
        hggBedDepth, convertType);
    hPrintf("&nbsp;depth&nbsp&nbsp");

    jsMakeTrackingRadioButton(hggBedConvertType, hggBedConvertTypeJs,
        hggBedCoverage, convertType);
    hPrintf("&nbsp;coverage");

    hPrintf("<br>\n");
    hPrintf(" ");
    }
hPrintf("</TD></TR>\n");

hPrintf("</TABLE>\n");

hPrintf("<i>Note: Loading some tables can take up to a minute. ");
hPrintf("If you are importing more than one data set please give them ");
hPrintf("different names.  Only the most recent data set of a given name is ");
hPrintf("kept.  Otherwise data sets will be kept for at least 48 hours from ");
hPrintf("last use.  After that time you may have to import them again.</i>");
hPrintf("<BR>\n");
hPrintf("<BR>\n");


hPrintf(" ");
cgiMakeButton(hggSubmitImport, "submit");

// TODO: should I add a cancel button?

hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
	hggImport,
	hggGroup, hggTrack, hggTable, 
	hggBedConvertType
	};
    jsCreateHiddenForm(cart, getScriptName(), saveVars, ArraySize(saveVars));
    }

/* Put up section that describes table types allowed. */
webNewSection("Import table types");
hPrintf("%s", 
"<P>All tables that have positional information can be imported. "
"This includes BED, PSL, wiggle, MAF, bedGraph and other standard types. "
"You can also use data in custom tracks.  The chromGraph custom tracks "
"are not available for import selection because these are already in the format desired. "
"All types will be converted to a chromGraph custom track with a window-size of 10,000 bases. "
"</P><br>"
);

webNewSection("Using the import page");
hPrintf(
"To import a table or custom track, "
"choose the group, track, and table from the drop-down lists, "
"and then submit. The other controls on this form are optional, though "
"filling them out will sometimes enhance the display. "
"The controls for display min and "
"max values and connecting lines can be set later via the configuration "
"page as well. Here is a description of each control."
"<UL>"
"<LI><B>name of data set:</B> This will be displayed in the graph list in "
" the Genome Graphs tool and as the track name in the Genome Browser. Only "
" the first 16 characters are visible in some contexts. For data sets with "
" multiple graphs, this is the first part of the name, shared with all "
" members of the data set.</LI>"
"<LI><B>description:</B> Enter a short sentence describing the data set. "
" It will be displayed in the Genome Graphs tool and in the Genome Browser.</LI>"
"<LI><B>display min value/max value:</B> Set the range of the data set to "
" be plotted. If left blank, the range will be taken from the min/max values in the "
" data set itself. If you would like all of your data sets to share the same scale, "
" you will need to set this.</LI>"
"<LI><B>label values:</B> A comma-separated list of numbers for the vertical axis. "
" If left blank the axis will be labeled at the 1/3 and 2/3 point. </LI>"
"<LI><B>draw connecting lines:</B> Lines connecting data points separated by "
" no more than this number of bases are drawn.  </LI>"
"<LI><B>depth or coverage:</B>"
" When importing positional tables, you can choose to convert those tables to "
" the chromGraph format by using either the depth or coverage conversion "
" method. Both conversion methods use a non-overlapping window size of 10,000 "
" bases when converting to the chromGraph format. In the depth method, the "
" weighted average for each 10,000 base window is assigned to a single point "
" in the center of this window. The coverage method is binary. If there is "
" even one point in the 10,000 base window, the resulting graph will have a "
" value of 1 over that range.</LI>"
"</P>"
);
cartWebEnd();
}




static void addIfNonempty(struct hash *hash, char *cgiVar, char *trackVar)
/* If cgiVar exists and is non-empty, add it to ra. */
{
char *val = skipLeadingSpaces(cartUsualString(cart, cgiVar, ""));
if (val[0] != 0)
    hashAdd(hash, trackVar, val);
}

void updateCustomTracksImport(struct customTrack *upList) 
/* Update custom tracks file with current upload data */
{
struct customTrack *oldList = customTracksParseCart(cart, NULL, NULL);
struct customTrack *outList = customTrackAddToList(oldList, upList, NULL, FALSE);
customTracksSaveCart(cart, outList);
hPrintf("These data are now available in the drop-down menus on the ");
hPrintf("main page for graphing.<BR>");
}

void trySubmitImport(struct sqlConnection *conn, char *rawText)
/* Called when they've submitted from uploads page */
{
struct lineFile *lf = lineFileOnString("uploaded data", TRUE, rawText);
struct customPp *cpp = customPpNew(lf);
struct hash *settings = hashNew(8);
addIfNonempty(settings, hggMinVal, "minVal");
addIfNonempty(settings, hggMaxVal, "maxVal");
addIfNonempty(settings, hggMaxGapToFill, "maxGapToFill");
addIfNonempty(settings, hggLabelVals, "linesAt");

struct customTrack *trackList = chromGraphParser(cpp,
	cartUsualString(cart, hggFormatType, cgfFormatTab),
	cartUsualString(cart, hggMarkerType, cgfMarkerGenomic),
	cartUsualString(cart, hggColumnLabels, cgfColLabelGuess), 
	nullIfAllSpace(cartUsualString(cart, hggDataSetName, NULL)),
	nullIfAllSpace(cartUsualString(cart, hggDataSetDescription, NULL)),
	settings, TRUE);
updateCustomTracksImport(trackList);
}


char *makeCoverageCgFromBedOld(struct sqlConnection *conn)
/* use range tree to create coverage chromGraph from bed data */
{
struct dyString *dy = dyStringNew(0);
struct rbTree *tree = NULL;
char *chrom = "";
int chromSize = 0;
struct slName *chr, *chromList = getChroms();
for (chr = chromList; chr != NULL; chr = chr->next)
    {
    char *table = curTable;
    struct hTableInfo *hti = getHti(database, table);
    int fields = hTableInfoBedFieldCount(hti);
    struct bed *bedList = NULL, *bed;
    struct lm *lm = lmInit(64*1024);
    chrom = chr->name;
    bedList = getBeds(chrom, lm, &fields);

    if (!bedList)
	continue;
    
    chromSize = hChromSize(chrom);
    tree = rangeTreeNew();
    for(bed=bedList;bed;bed=bed->next)
	{
	rangeTreeAdd(tree, bed->chromStart, bed->chromEnd);
	}
    lmCleanup(&lm);
    
    struct range *range=NULL, *rangeList = rangeTreeList(tree);
    int lastEnd = -1;
    for(range=rangeList;range;range=range->next)
	{
	if ((range->start - lastEnd) >= 3)
	    {
	    if (lastEnd >= 0)
		{
		dyStringPrintf(dy,"%s\t%d\t%f\n", chrom, lastEnd-1, 1.0);
		dyStringPrintf(dy,"%s\t%d\t%f\n", chrom, lastEnd, 0.0);
		}
	    if ((range->start-1) >= 0)
		dyStringPrintf(dy,"%s\t%d\t%f\n", chrom, range->start-1, 0.0); 
	    dyStringPrintf(dy,"%s\t%d\t%f\n", chrom, range->start, 1.0);
	    }
	lastEnd = range->end;
	}
    if (lastEnd >= 1)
	{
	dyStringPrintf(dy,"%s\t%d\t%f\n", chrom, lastEnd-1, 1.0);
	if (lastEnd < chromSize)
	    dyStringPrintf(dy,"%s\t%d\t%f\n", chrom, lastEnd, 0.0);
	}
    rbTreeFree(&tree);
    }
    
return dyStringCannibalize(&dy);

}

char *makeCoverageCgFromBed(struct sqlConnection *conn)
/* create coverage chromGraph from bed data */
{
struct dyString *dy = dyStringNew(0);
char *chrom = "";
int chromSize = 0;
int windowSize = 10000;
int numWindows = 0;
double *depth = NULL;
int overlap = 0;
struct slName *chr, *chromList = getChroms();
for (chr = chromList; chr != NULL; chr = chr->next)
    {
    char *table = curTable;
    struct hTableInfo *hti = getHti(database, table);
    int fields = hTableInfoBedFieldCount(hti);
    struct bed *bedList = NULL, *bed;
    struct lm *lm = lmInit(64*1024);
    chrom = chr->name;
    bedList = getBeds(chrom, lm, &fields);

    if (!bedList)
	continue;
    
    chromSize = hChromSize(chrom);

    //debug
    //hPrintf("chrom %s size=%d numFeatures=%d<br>\n", chrom, chromSize, slCount(bedList)); fflush(stdout);
    
    numWindows = ((chromSize+windowSize-1)/windowSize);

    if (numWindows>0)
	depth = needMem(numWindows*sizeof(double));
    for(bed=bedList;bed;bed=bed->next)
	{
	int i;
	int start = bed->chromStart;
	int end = bed->chromEnd;
	for(i = start/windowSize; i*windowSize < end; ++i)
	    {
	    overlap = rangeIntersection(start, end, i*windowSize, (i+1)*windowSize);
	    if (overlap > 0)
		{
		depth[i] += ((double)overlap)/windowSize;
		}
	    }
	}
    lmCleanup(&lm);
    int i;
    for(i=0;i<numWindows;++i)
	{
	int midPoint = i*windowSize+(windowSize/2);
	if (midPoint < chromSize)
	    {
	    dyStringPrintf(dy,"%s\t%d\t%f\n", 
		chrom, i*windowSize+(windowSize/2), depth[i] > 0.0 ? 1.0 : 0.0);
	    }
	}
    freez(&depth);
    }

return dyStringCannibalize(&dy);
}


char *makeDepthCgFromBed(struct sqlConnection *conn, boolean isBedGr)
/* create depth chromGraph from bed, bedGraph, or Maf data */
{
struct dyString *dy = dyStringNew(0);
char *chrom = "";
int chromSize = 0;
int windowSize = 10000;
int numWindows = 0;
double *depth = NULL;
int overlap = 0;
struct slName *chr, *chromList = getChroms();
for (chr = chromList; chr != NULL; chr = chr->next)
    {
    char *table = curTable;
    struct hTableInfo *hti = getHti(database, table);
    int fields = hTableInfoBedFieldCount(hti);
    struct bed *bedList = NULL, *bed;
    struct lm *lm = lmInit(64*1024);
    chrom = chr->name;
    bedList = getBeds(chrom, lm, &fields);

    if (!bedList)
	continue;
    
    chromSize = hChromSize(chrom);

    //debug
    //hPrintf("chrom %s size=%d numFeatures=%d<br>\n", chrom, chromSize, slCount(bedList)); fflush(stdout);
    
    numWindows = ((chromSize+windowSize-1)/windowSize);

    if (numWindows>0)
	depth = needMem(numWindows*sizeof(double));
    for(bed=bedList;bed;bed=bed->next)
	{
	int i;
	int start = bed->chromStart;
	int end = bed->chromEnd;
	for(i = start/windowSize; i*windowSize < end; ++i)
	    {
	    overlap = rangeIntersection(start, end, i*windowSize, (i+1)*windowSize);
	    if (overlap > 0)
		{
		if (isBedGr)
		    depth[i] += ((((double)overlap)/windowSize)*bed->expScores[0]);
		else
    		    depth[i] += ((double)overlap)/windowSize;
		}
	    }
	}
    lmCleanup(&lm);
    int i;
    for(i=0;i<numWindows;++i)
	{
	int midPoint = i*windowSize+(windowSize/2);
	if (midPoint < chromSize)
	    {
	    dyStringPrintf(dy,"%s\t%d\t%f\n", 
		chrom, i*windowSize+(windowSize/2), depth[i]);
	    }
	}
    freez(&depth);
    }

return dyStringCannibalize(&dy);
}


char *makeAverageCgFromWiggle(struct sqlConnection *conn)
/* use averaging to create chromGraph from wig ascii data */
{
struct dyString *dy = dyStringNew(0);
char *chrom = "";
int chromSize = 0;
int windowSize = 10000;
int numWindows = 0;
double *depth = NULL;
bool *hit = NULL;
int overlap = 0;
unsigned long long grandTotalValues=0;
struct slName *chr, *chromList = getChroms();
for (chr = chromList; chr != NULL; chr = chr->next)
    {
    int totalValues=0;
    chrom = chr->name;
    struct wiggleDataStream *wds = wigChromRawStats(chrom);
    struct wiggleStats *stats=NULL, *statsList = wds->stats;

    if (!wds->stats)
	continue;
    
    chromSize = hChromSize(chrom);

    numWindows = ((chromSize+windowSize-1)/windowSize);

    if (numWindows>0)
	{
	AllocArray(depth, numWindows);
	AllocArray(hit, numWindows);
	}

    for(stats=statsList;stats;stats=stats->next)
	{
	totalValues += stats->count;
	int i; 
	int start = stats->chromStart;
	int end = stats->chromEnd;
	double sumData = stats->mean;
	for(i = start/windowSize; i*windowSize < end; ++i)
	    {
	    overlap = rangeIntersection(start, end, i*windowSize, (i+1)*windowSize);
	    if (overlap > 0)
		{
		depth[i] += ((((double)overlap)/windowSize)*sumData);
		hit[i] = TRUE;
		}
	    }
	}

    //debug
    //hPrintf("chrom %s size=%d span=%d numValues=%d total values=%d<br>\n", 
	//chrom, chromSize, statsList->span, statsList->count, totalValues);

    wiggleDataStreamFree(&wds);
    statsList = NULL;

    /* write accumulated chromGraph windowed info */
    int i;
    for(i=0;i<numWindows;++i)
	{
	if (hit[i])
	    {
	    int midPoint = i*windowSize+(windowSize/2);
	    if (midPoint < chromSize)
		{
		dyStringPrintf(dy,"%s\t%d\t%f\n", 
		    chrom, i*windowSize+(windowSize/2), depth[i]);
		}
	    }
	}
    freez(&depth);
    freez(&hit);
    grandTotalValues+=totalValues;
    fflush(stdout);  /* so user can see progress */
    }
//debug
//hPrintf("Genome Total Number of Values = %llu<br>\n", grandTotalValues);


return dyStringCannibalize(&dy);
}


void submitImport(struct sqlConnection *conn)
/* Called when they've submitted from import page */
{
boolean isWig = FALSE, isPositional = FALSE, isMaf = FALSE, isBedGr = FALSE,
        isChromGraphCt = FALSE;
struct hTableInfo *hti = NULL;

cartWebStart(cart, "Table Import in Progress ");
hPrintf("<FORM ACTION=\"../cgi-bin/hgGenome\">");
cartSaveSession(cart);


//char *selGroup = cartString(cart, hggGroup);
//char *selTrack = cartString(cart, hggTrack);
//char *selTable = cartString(cart, hggTable);

allJoiner = joinerRead("all.joiner");
initGroupsTracksTables(conn);

if (strchr(curTable, '.') == NULL)  /* In same database */
    {
    hti = getHti(database, curTable);
    isPositional = htiIsPositional(hti);
    }
isWig = isWiggle(database, curTable);
isMaf = isMafTable(database, curTrack, curTable);
isBedGr = isBedGraph(curTable);
if (isCustomTrack(curTable))
    {
    isChromGraphCt = isChromGraph(curTrack);
    }

if (curTrack == NULL)
    {
    struct trackDb *tdb = hTrackDbForTrack(curTable);
    struct trackDb *cTdb = hCompositeTrackDbForSubtrack(tdb);
    if (cTdb)
	curTrack = cTdb;
    else
	curTrack = tdb;
    isMaf = isMafTable(database, curTrack, curTable);
    }

/* debug info
hPrintf("Debug Info:<br>\n");
hPrintf("----------------------------<br>\n");
hPrintf("selected group: %s<br>\n",selGroup);
hPrintf("selected track: %s<br>\n",selTrack);
hPrintf("selected table: %s<br>\n",selTable);
hPrintf("isWig: %d<br>\n", isWig);
hPrintf("isPositional: %d<br>\n", isPositional);
hPrintf("isMaf: %d<br>\n", isMaf);
hPrintf("isBedGr: %d<br>\n", isBedGr);
hPrintf("isChromGraphCt: %d<br>\n", isChromGraphCt);
*/


if (isPositional && !isWig && !isMaf && !isBedGr && !isChromGraphCt)
    {  /* simple positional */
    char *convertType = 
    cartUsualString(cart, hggBedConvertType, hggBedDepth);
    char *rawText = NULL;
    
    if (sameString(convertType,hggBedCoverage))
	rawText = makeCoverageCgFromBed(conn);
    else
	rawText = makeDepthCgFromBed(conn, isBedGr);

    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
   	trySubmitImport(conn, rawText);
    errCatchFinish(&errCatch);

    }
else if (isPositional && !isWig && !isMaf && isBedGr && !isChromGraphCt)
    {  /* bedGraph */
    char *rawText = NULL;
    
    char *bedGraphField = getBedGraphField(curTable,curTrack->type);

    //debug: remove:
    hPrintf("bedGraphField = %s<br>\n",bedGraphField); fflush(stdout);


    rawText = makeDepthCgFromBed(conn, isBedGr);

    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
   	trySubmitImport(conn, rawText);
    errCatchFinish(&errCatch);

    }
else if (isPositional && !isWig && isMaf && !isBedGr && !isChromGraphCt)
    {  /* maf */
    char *rawText = NULL;
    
    rawText = makeDepthCgFromBed(conn, isMaf);

    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
   	trySubmitImport(conn, rawText);
    errCatchFinish(&errCatch);

    }
else if (isPositional && isWig && !isMaf && !isBedGr && !isChromGraphCt)
    { /* wiggle */
    char *rawText = NULL;
    
    rawText = makeAverageCgFromWiggle(conn);

    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
   	trySubmitImport(conn, rawText);
    errCatchFinish(&errCatch);

    }


hPrintf("<CENTER>");
cgiMakeButton("submit", "OK");
hPrintf("</CENTER>");
hPrintf("</FORM>");
cartWebEnd();
}
