/* correlate - handle correlating beds. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "trackDb.h"
#include "bits.h"
#include "bed.h"
#include "hdb.h"
#include "featureBits.h"
#include "obscure.h"
#include "wiggle.h"
#include "customTrack.h"
#include "jsHelper.h"
#include "hgTables.h"
#define INCL_HELP_TEXT
#include "correlate.h"	/* our structure defns and the corrHelpText string */
#include "bedGraph.h"
#include "hgMaf.h"

static char const rcsid[] = "$Id: correlate.c,v 1.66.6.2 2008/08/07 16:02:38 markd Exp $";

#define MAX_POINTS_STR	"300,000,000"
#define MAX_POINTS	300000000

static char *maxResultsMenu[] =
{
    "20,000,000",
    "40,000,000",
    "60,000,000",
    MAX_POINTS_STR,
};
static int maxResultsMenuSize = ArraySize(maxResultsMenu);

static struct dataVector *allocDataVector(char *chrom, int size)
/* allocate a dataVector for 'size' number of data points on specified chrom */
{
struct dataVector *v;
AllocVar(v);
v->chrom = cloneString(chrom);
v->start = 0;
v->end = 0;
v->name = NULL;
v->position = needHugeMem((size_t)(sizeof(int) * size));
v->value = needHugeMem((size_t)(sizeof(float) * size));
v->count = 0;		/*	nothing here yet	*/
v->maxCount = size;	/*	do not run past this limit	*/
v->min = INFINITY;	/*	must be less than this	*/
v->max = -INFINITY;	/*	must be greater than this */
v->sumData = 0.0;	/*	starting sum is zero	*/
v->sumSquares = 0.0;	/*	starting sum is zero	*/
v->sumProduct = 0.0;	/*	starting sum is zero	*/
v->r = 0.0;		/*	an initial r is zero	*/
v->fetchTime = 0;	/*	will accumulate fetch timings */
v->calcTime = 0;	/*	will accumulate calculation timings */
return v;
}

void freeDataVector(struct dataVector **v)
/*	free up space belonging to a dataVector	*/
{
struct dataVector *dv;
dv=*v;
if (dv)
    {
    freeMem(dv->chrom);
    freeMem(dv->name);
    freeMem(dv->position);
    freeMem(dv->value);
    }
freez(v);
}

static struct trackTable *allocTrackTable()
/*	allocate a trackTable item */
{
struct trackTable *t;
AllocVar(t);	/*	everything is zero which is what we want	*/
return t;
}

static void freeTrackTable(struct trackTable **t)
/*	free up space belonging to a trackTable	*/
{
struct trackTable *tTable;
tTable=*t;
if (tTable)
    {
    freeMem(tTable->tableName);
    freeMem(tTable->shortLabel);
    freeMem(tTable->longLabel);
    freeMem(tTable->actualTable);
    freeMem(tTable->bedGraphColumnName);
    freeDataVector(&tTable->vSet);
    }
freez(t);
}

static void freeTrackTableList(struct trackTable **tl)
/*	free a list of track tables	*/
{
struct trackTable *table, *next;
for (table = *tl; table != NULL; table = next)
    {
    next = table->next;
    freeTrackTable(&table);
    }
*tl = NULL;
}

/*	This restrictedTrackList is created in getLimitedTrackList()
 *	It is a subset of the fullTrackList
 */
static struct trackDb *restrictedTrackList = NULL;

/* We keep two copies of variables, so that we can
 * cancel out of the page. */

static char *curVars[] = {hgtaCorrelateGroup, hgtaCorrelateTrack,
	hgtaCorrelateTable, hgtaCorrelateOp,
	};
static char *nextVars[] = {hgtaNextCorrelateGroup, hgtaNextCorrelateTrack,
	hgtaNextCorrelateTable, hgtaNextCorrelateOp,
	};

boolean anyCorrelation()
/* Return TRUE if there's an correlation to do. */
{
return cartVarExists(cart, hgtaCorrelateTrack);
}

static char *onChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, "document.hiddenForm.submit();\"");
return dyStringCannibalize(pDy);
}

static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = jsOnChangeStart();
jsDropDownCarryOver(dy, hgtaNextCorrelateGroup);
jsDropDownCarryOver(dy, hgtaNextCorrelateTrack);
/*jsTrackedVarCarryOver(dy, hgtaNextCorrelateOp, "op");*/
return dy;
}

static char *onChangeEither()
/* Get group-changing javascript. */
{
struct dyString *dy = onChangeStart();
return onChangeEnd(&dy);
}

#ifdef NOT
static void makeOpButton(char *val, char *selVal)
/* Make op radio button including a little Javascript
 * to save selection state. */
{
jsMakeTrackingRadioButton(hgtaNextCorrelateOp, "op", val, selVal);
}
#endif

boolean correlateTrackTableOK(struct trackDb *tdb, char *table)
/*	is this table OK to correlate with ?	*/
{
if (!tdb)
    return FALSE;
if (startsWith("bedGraph", tdb->type) ||
    startsWith("bed5FloatScore",tdb->type) ||
    startsWith("wig ",tdb->type) ||
    startsWith("genePred",tdb->type) ||
    startsWith("psl",tdb->type) ||
    startsWith("bed ",tdb->type))
        return TRUE;
if (startsWith("wigMaf ",tdb->type))
    {
    /* can correlate if we've picked a conservation wiggle, otherwise not.
     * later we can add support for the other table types */
    if (table == NULL)
        /* don't care about table */
        return TRUE;
    struct consWiggle *wig, *wigs = wigMafWiggles(database, tdb);
    if (!wigs)
        return FALSE;
    /* check if table is one of the wiggles */
    for (wig = wigs; wig != NULL; wig = wig->next)
        if (sameString(table, wig->table))
            return TRUE;
    return FALSE;
    }
return FALSE;
}

boolean correlateTrackOK(struct trackDb *tdb)
/*	is this track OK to correlate with ?	*/
{
return correlateTrackTableOK(tdb, NULL);
}

static struct trackDb *getLimitedTrackList(struct grp **grpList)
/*	create list of tracks limited to specific types, and return the
 *	groups of those tracks	*/
{
struct trackDb *tdb;
struct hash *groupsInTrackList = newHash(0);
struct grp *groupsAll, *groupList = NULL, *group;

if (NULL == fullTrackList)
    errAbort("fullTrackList is null in getLimitedTrackList");

/*	already been done ?  Then return previous answer	*/
if (restrictedTrackList)
    return(restrictedTrackList);


/*	for each track that is OK to correlate, remember its group on
 *	the groupsInTrackList, and clone the trackDb structure onto the
 *	restrictedTrackList
 */
for (tdb = fullTrackList; tdb != NULL; tdb = tdb->next)
    {
    if (correlateTrackOK(tdb))
	{
	struct trackDb *tdbClone;
	if (!hashLookup(groupsInTrackList,tdb->grp))
	    {
	    hashAdd(groupsInTrackList, tdb->grp, NULL);
	    }
	tdbClone = cloneMem(tdb,sizeof(struct trackDb));
	tdbClone->next = NULL;
	slAddHead(&restrictedTrackList, tdbClone);
	}
    }

if (grpList)
    {
    groupsAll = hLoadGrps(database);
    for (group = slPopHead(&groupsAll); group != NULL;
		group = slPopHead(&groupsAll))
	{
	if (hashLookup(groupsInTrackList, group->name))
	    {
	    slAddTail(&groupList, group);
	    }
	else
	    grpFree(&group);
	}
    *grpList = groupList;
    }

hashFree(&groupsInTrackList);
slReverse(&restrictedTrackList);

return(restrictedTrackList);
}

static struct trackDb *showTrackFieldLimited(struct grp *selGroup,
	char *trackVar, char *trackScript, struct trackDb *limitedTrackList)
/* Show track control. Returns selected track. Limited to limitedTrackList */
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
    hPrintf("<SELECT NAME=%s %s>\n", trackVar, trackScript);
    if (allTracks)
        {
	selTrack = findSelectedTrack(limitedTrackList, NULL, trackVar);
	slSort(&limitedTrackList, trackDbCmpShortLabel);
	}
    else
	{
	selTrack = findSelectedTrack(limitedTrackList, selGroup, trackVar);
	}
    for (track = limitedTrackList; track != NULL; track = track->next)
	{
	if (allTracks || sameString(selGroup->name, track->grp))
	    hPrintf(" <OPTION VALUE=%s%s>%s\n", track->tableName,
		(track == selTrack ? " SELECTED" : ""),
		track->shortLabel);
	}
    hPrintf("</SELECT>\n");
    }
hPrintf("\n");
return selTrack;
}

static void fillInTrackTable(struct trackTable *table,
	struct sqlConnection *conn)
/*	determine real tdb, and attach labels, only tdb and tableName
 *	have been set so far, these may be composites, wigMafs or
 *	something like that where the actual tdb and table is something
 *	else.	*/
{
boolean isCustomDbTable = FALSE;
struct customTrack *ct = NULL;
struct trackDb *tdb = findCompositeTdb(table->tdb,table->tableName);
table->shortLabel = cloneString(tdb->shortLabel);
table->longLabel = cloneString(tdb->longLabel);
table->actualTdb = tdb;
table->actualTable = cloneString(tdb->tableName);
table->isBedGraph = FALSE;
table->isWig = FALSE;
table->dbTableName = NULL;

if (isCustomTrack(table->actualTable))
    {
    ct = lookupCt(table->actualTable);
    if (ctDbAvailable(ct->dbTableName))
	{
	table->dbTableName = ct->dbTableName;
	isCustomDbTable = TRUE;
	}
    }

if (startsWith("bedGraph", tdb->type))
    {
    table->isBedGraph = TRUE;
    /*	find the column name that belongs to the specified numeric
     *	column from the bedGraph type line
     */
    if (isCustomDbTable)
        {
	conn = hAllocConnProfile(CUSTOM_TRACKS_PROFILE, CUSTOM_TRASH);
        freeMem(table->actualTable);
        table->actualTable = cloneString(ct->dbTableName);
        }

    /* there are no bedGraph custom tracks yet, but when they do show
     * up, they will only correlate if they are in the database 
     * XXXX Hiram, what does this mean? Why would the custom tables not be in the db?
     * Also, what's up with the following || test?
     */
    if (!table->isCustom || isCustomDbTable)
	{
	char query[256];
	struct sqlResult *sr;
	char **row;
	int wordCount;
	char *words[8];
	char *typeLine = cloneString(tdb->type);
	int colCount = 0;

	wordCount = chopLine(typeLine,words);
	if (wordCount > 1)
	    table->bedGraphColumnNum = sqlUnsigned(words[1]);
	freez(&typeLine);
	safef(query, ArraySize(query), "describe %s", table->actualTable);
	sr = sqlGetResult(conn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    ++colCount;
	    if ((1 == colCount) && sameString(row[0], "bin"))
		colCount = 0;
	    if (colCount == table->bedGraphColumnNum)
		table->bedGraphColumnName = cloneString(row[0]);
	    }
	sqlFreeResult(&sr);
	if (isCustomDbTable)
	    hFreeConn(&conn);
	}
    }
else if (sameString("cpgIsland", tdb->tableName))
    {
    table->isBedGraph = TRUE;
    table->bedGraphColumnName = cloneString("perCpg");
    table->bedGraphColumnNum = 8;
    }
else if (startsWith("bed ", tdb->type))
    {
    int wordCount;
    char *words[8];
    char *typeLine = cloneString(tdb->type);
    wordCount = chopLine(typeLine,words);
    if ((wordCount > 1) && differentWord(".",words[1]))
	{
        table->bedColumns = sqlUnsigned(words[1]);
	if ((table->bedColumns > 4) && (table->bedColumns < 12))
	    {
	    table->isBedGraph = TRUE;
	    table->bedGraphColumnName = cloneString("score");
	    }
	}
    else
        table->bedColumns = 3;
    }
else if (startsWith("bed5FloatScore", tdb->type))
    {
    table->isBedGraph = TRUE;
    table->bedGraphColumnName = cloneString("floatScore");
    table->bedGraphColumnNum = 6;
    }
else if (startsWith("wig",tdb->type))
    {
    if (startsWith("wigMaf",tdb->type))
	{
        struct consWiggle *wig, *wiggles = wigMafWiggles(database, tdb);
        if (!wiggles)
            /* should have found this earlier (correlateOK) */
            errAbort("No conservation wiggle found for track %s",
                        tdb->tableName);
        boolean found = FALSE;
        for (wig = wiggles; wig != NULL; wig = wig->next)
            {
            if (sameString(table->tableName, wig->table))
                found = TRUE;
            }
        if (!found)
            errAbort("Conservation wiggle %s not found for track %s",
                        table->tableName, tdb->tableName);
	freeMem(table->actualTable);
	table->actualTable = cloneString(table->tableName);
	}
    table->isWig = TRUE;
    }
table->isCustom = isCustomTrack(table->actualTable);

}	/*	static void fillInTrackTable()	*/

struct trackTable *trackTableNew(struct trackDb *tdb, char *tableName,
				 struct sqlConnection *conn)
/* Allocate, fill in and return a trackTable. */
{
struct trackTable *tt = allocTrackTable();
if (tdb == NULL)
    errAbort("Program error: NULL tdb passed to trackTableNew");
tt->tdb = tdb;
tt->tableName = cloneString(tableName);
fillInTrackTable(tt, conn);
return tt;
}

/****************************   NOTE   ********************************
 *	the following showGroupFieldLimited, showTable2FieldLimited and
 *	showGroupTrackRowLimited, are almost near duplicates of the
 *	same functions showGroupField, showTableField and showTrackField
 *	in mainPage.c - the specialized limited list function in these
 *	instances should be folded back into the mainPage ones so there
 *	would be only one copy.
 **********************************************************************/

static struct grp *showGroupFieldLimited(char *groupVar, char *groupScript,
    boolean allTablesOk, struct grp *groupList)
/* Show group control. Returns selected group. */
{
struct grp *group;
struct grp *selGroup = findSelectedGroup(groupList, groupVar);

/*	findSelectedGroup() returns the first one on the list if no match */

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
static char *showTable2FieldLimited(struct trackDb *track, char *table)
/* Show table control and label. */
{
struct slName *name, *nameList = NULL;
char *selTable;

if (track == NULL)
    nameList = tablesForDb(findSelDb());
else
    nameList = tablesForTrack(track, FALSE);

/* Get currently selected table.  If it isn't in our list
 * then revert to first in list. */
selTable = cartUsualString(cart, table, nameList->name);
if (!slNameInList(nameList, selTable))
    selTable = nameList->name;
/* Print out label and drop-down list. */
hPrintf("<B>table: </B>");
hPrintf("<SELECT NAME=%s>\n", table);
for (name = nameList; name != NULL; name = name->next)
    {
    struct trackDb *tdb = NULL;
    tdb = findCompositeTdb(track, name->name);
    if (correlateTrackTableOK(tdb, name->name))
	{
	hPrintf("<OPTION VALUE=%s", name->name);
	if (sameString(selTable, name->name))
	    hPrintf(" SELECTED");
	if (tdb != NULL)
	    if (differentWord(tdb->shortLabel, track->shortLabel))
		hPrintf(">%s (%s)\n", tdb->shortLabel, name->name);
	    else
		hPrintf(">%s\n", name->name);
	else
	    hPrintf(">%s\n", name->name);
	}
    }
hPrintf("</SELECT>\n");
return selTable;
}

static struct trackDb *showGroupTrackRowLimited(char *groupVar,
    char *groupScript, char *trackVar, char *trackScript,
	char *tableVar, char **table2Return)
/* Show group & track row of controls, limited by track type.
    Returns selected track */
{
struct trackDb *track2, *trackList;
struct grp *selGroup, *groupList = NULL;
char *table2;

trackList = getLimitedTrackList(&groupList);

hPrintf("Select a group, track and table to correlate with:\n");

hPrintf("<TABLE BORDER=0>\n");
hPrintf("<TR><TD>");

selGroup = showGroupFieldLimited(groupVar, groupScript, FALSE, groupList);
nbSpaces(3);
track2 = showTrackFieldLimited(selGroup, trackVar, trackScript, trackList);

hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
table2 = showTable2FieldLimited(track2, tableVar);
hPrintf("</TD></TR>\n");

if (table2Return)
    *table2Return = table2;

hPrintf("</TABLE>\n");

return track2;
}

static void freeWigAsciiData(struct wigAsciiData **wigData)
/* free up the wiggle ascii data list	*/
{
if (wigData)
    {
    struct wiggleDataStream *wds = NULL;
    /*  create an otherwise empty wds so we can use the object's free */
    wds = wiggleDataStreamNew();
    wds->ascii = *wigData;
    wiggleDataStreamFree(&wds);
    *wigData = NULL;
    }
}

static void correlateReadBed(struct trackTable *table,
    struct dataVector *vector, struct region *region,
	struct sqlConnection *conn)
/*	reads any kind of bed file, from db or custom track	*/
{
int vIndex = vector->count;	/* it is starting at zero	*/
struct bed *bedList = NULL, *bed;
int bedFieldCount = 0;
struct lm *lm = lmInit(64*1024);
char *dbTableName = table->actualTable;

if (NULL != table->dbTableName)
    {
    dbTableName = table->dbTableName;
    conn = hAllocConnProfile(CUSTOM_TRACKS_PROFILE, CUSTOM_TRASH);
    }

/*	cookedBedList can read anything but bedGraph, so read bedGraph
 *	by itself, use cookedBedList for the other types.
 *	NOTE - this isn't intersecting the bedGraph types !
 *		but it is doing the data filter if it is set.
 */
if (table->isBedGraph)
    {
    char *filter = filterClause(database, table->actualTable, region->chrom);
    char fields[256];
    struct sqlResult *sr = NULL;
    char **row;
    int rowOffset = 0;

    if (table->isCustom && (NULL == table->dbTableName))
	errAbort("Custom track bed graph correlations not yet implemented");

    safef(fields,ArraySize(fields), "chromStart,chromEnd,%s",
	table->bedGraphColumnName);
    sr = hExtendedRangeQuery(conn, dbTableName, region->chrom,
	region->start, region->end, filter, TRUE, fields, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	struct bedGraph *bedItem;
	lmAllocVar(lm,bedItem);
	bedItem->chrom = region->chrom;
	bedItem->chromStart = sqlUnsigned(row[0]);
	bedItem->chromEnd = sqlUnsigned(row[1]);
	bedItem->dataValue = sqlFloat(row[2]);
	slAddHead(&bedList, (struct bed *)bedItem);
	}
    sqlFreeResult(&sr);
    bedFieldCount = 4;
    }
else
    {
    /*	other bed types have their vector filled with zero first	*/
    int i;
    int indx = vIndex;
    register float value = 0.0;
    for (i = region->start; (i < region->end) && (indx < vector->maxCount);
		++i, ++indx)
	{
	vector->value[indx] = value;
	vector->position[indx] = i;
	}
    vector->count = indx;	/*	this is how many there are	*/
    /*	and now read in the bed list for this data set	*/
    bedList=cookedBedList(conn, table->actualTable, region, lm, &bedFieldCount);
    }
if (NULL != table->dbTableName)
    hFreeConn(&conn);

slSort(&bedList, bedLineCmp);   /* make sure it is sorted by chrom,chromStart*/

if (table->isBedGraph)	/*	bedGraphs fill with their value */
    {
    /*	this lastEnd business will take care of the overlapping bed
     *	situation.  As each bed item is processed, it determines the
     *	lastEnd position at its end, the next item has to have data beyond
     *	that position.  The bed list was ordered by chromStart above so
     *	we know they will be processed in the proper order.
     */
    int lastEnd = 0;	/*	to ensure ever increasing positions */
    if (vector->count > 0)
	lastEnd = vector->position[(vector->count)-1];

    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	register int bases = 0;
	int cStart = bed->chromStart;
	int cEnd = bed->chromEnd;
	int start, size, vEnd;
	int i;

	/*	make sure we stay within the region specified	*/
	if (cEnd < region->start || cStart > region->end)
	    continue;	/* item is completely out of the region */

	/*	trim to region if necessary	*/
	if (cStart < region->start) cStart = region->start;
	if (cEnd > region->end) cEnd = region->end;
	if (cEnd <= lastEnd)
	    continue;	/*	no new area covered by this item	*/

	start = cStart;
	if (start < lastEnd) start = lastEnd;
	size = cEnd - start;
	vEnd = vIndex + size;
	if (vEnd > vector->maxCount)
	    {
	    vEnd = vector->maxCount;
	    size = vEnd - vIndex;
	    }
	if (size > 0)
	    {
	    register float value = ((struct bedGraph *)bed)->dataValue;
	    for (i = start; vIndex < vEnd; ++i, ++vIndex)
		{
		vector->value[vIndex] = value;
		vector->position[vIndex] = i;
		}
	    bases += size;
	    lastEnd = i;
	    vector->count += bases;
	    }
	}	/* for (bed = bedList; bed != NULL; bed = bed->next) */
    }	/*	processing bedGraph data	*/
else if (bedFieldCount < 12)
    /*	positional only beds fill each item's span with 1.0	*/
    {
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	register float value = 1.0;
	int cStart = bed->chromStart;
	int cEnd = bed->chromEnd;
	int vEnd;

	/*	make sure we stay within the region specified	*/
	if (cEnd < region->start || cStart > region->end)
	    continue;
	if (cStart < region->start)
	    cStart = region->start;
	if (cEnd > region->end)
	    cEnd = region->end;
	/* index 0 is the first base in region */
	vIndex = cStart - region->start;
	vEnd = cEnd - region->start;
	if (vEnd > vector->maxCount) vEnd = vector->maxCount;
	for ( ;  vIndex < vEnd; ++vIndex)
	    vector->value[vIndex] = value;
	}
    }
else if (bedFieldCount > 11)  /* bed 12 +, filling exon locations with 1.0 */
    {
    register float value = 1.0;

    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	int blockCount = bed->blockCount;
	int *sizes = bed->blockSizes;
	int *starts = bed->chromStarts;
	int block = 0;
	int chromStart = bed->chromStart;

	for (block = 0; block < blockCount; ++block)
	    {
	    int blockStart = starts[block] + chromStart;
	    int blockEnd = blockStart + sizes[block];
	    int vEnd;

	    /*	stay within region	*/
	    if (blockEnd < region->start || blockStart > region->end)
		continue;
	    if (blockStart < region->start)
		blockStart = region->start;
	    if (blockEnd > region->end)
		blockEnd = region->end;

	    /* index 0 is the first base in region */
	    vIndex = blockStart - region->start;
	    vEnd = blockEnd - region->start;
	    if (vEnd > vector->maxCount) vEnd = vector->maxCount;
	    for ( ;  vIndex < vEnd; ++vIndex)
		vector->value[vIndex] = value;
	    }
	}	/*	for (bed = bedList; bed != NULL; bed = bed->next) */
    }	/* if (bedFieldCount > 11)  bed 12 +, filling exon locations with 1.0 */
    else
	internalErr();	/*	it should have been one of those types */

lmCleanup(&lm);

vector->start = vector->position[0];
if (vector->count > 0)
    vector->end = vector->position[(vector->count)-1] + 1; /* non-inclusive */
else
    vector->end = vector->start;
}	/*	static void correlateReadBed()	*/

struct dataVector *dataVectorFetchOneRegion(struct trackTable *table,
	struct region *region, struct sqlConnection *conn)
/*	fetch all the data for this track and table in the given region */
{
struct dataVector *vector;
int regionSize = region->end - region->start;
long startTime, endTime;

if (! correlateTrackOK(table->actualTdb))
    internalErr();	/*	this should have been ensured long before now */

startTime = clock1000();
/*	assume entire region is going to produce numbers	*/
vector = allocDataVector(region->chrom, regionSize);

/*	wiggle tables work properly from database or custom tracks,
 *	    intersections and data value limits will
 *	    function in getWiggleData().
 */
if (table->isWig)
    {
    int span = 1;
    struct wigAsciiData *wigData = NULL;
    struct wigAsciiData *asciiData;
    struct wigAsciiData *next;
    int vIndex = vector->count;	/* it is starting at zero	*/
    register int bases = 0;
    char *dbTableName = table->actualTable;

    if (NULL != table->dbTableName)
	{
	dbTableName = table->dbTableName;
	conn = hAllocConnProfile(CUSTOM_TRACKS_PROFILE, CUSTOM_TRASH);
	}

    /*	we still do not have a proper minSpan finder for custom tracks */
    /*	they should have a spanList setup during their encoding, then it
     *	would always be there.
     */
    if (table->isCustom && (NULL == table->dbTableName))
	span = 1;
    else
	span = minSpan(conn, dbTableName, region->chrom,
	    region->start, region->end, cart, table->actualTdb);

    wigData = getWiggleData(conn, table->actualTable, region,
	vector->maxCount, span);
    for (asciiData = wigData; asciiData; asciiData = next)
	{
	struct asciiDatum *data = asciiData->data;
	int i;
	next = asciiData->next;
	for (i = 0; i < asciiData->count; ++i, ++data)
	    {
	    float value = data->value;
	    int cStart = data->chromStart;
	    int cEnd = cStart + asciiData->span;
	    int j;
	    if (cEnd < region->start || cStart > region->end)
		continue;
	    if (cStart < region->start)
		cStart = region->start;
	    if (cEnd > region->end)
		cEnd = region->end;
	    for (j = cStart;
		    (j < cEnd) && (vIndex < vector->maxCount);++j,++vIndex)
		{
		vector->value[vIndex] = value;
		vector->position[vIndex] = j;
		++bases;
		}
	    }
	}
    vector->count += bases;

    freeWigAsciiData(&wigData);
    vector->start = vector->position[0];
    if (vector->count > 0)
	vector->end = vector->position[(vector->count)-1] + 1;/*non-inclusive*/
    else
	vector->end = vector->start;
    }
else
    correlateReadBed(table, vector, region, conn);

if (table->dbTableName != NULL)
    hFreeConn(&conn);

endTime = clock1000();

vector->fetchTime += endTime - startTime;

if (vector->count > 0)
    return vector;
else
    freeDataVector(&vector);
return NULL;
}	/*	struct dataVector *dataVectorFetchOneRegion()	*/

static void scavengeVectorSpace(struct dataVector *dv)
/* Realloc dv->value and dv->position if newCount is sufficiently smaller 
 * than dv->maxCount. */
{
/*	if our count of numbers is more than 1000K less than planned for,
 *	save some space and reallocate the data lists.
 *	The potential savings here is three different data vectors,
 *	where each data point occupies about 8 bytes on a vector,
 *	thus: bytes saved at least 1024000 * 3 * 8 = 24,576,000
 */
if ((dv->count > 0) && ((dv->maxCount - dv->count) > 1024000))
    {
    int *ip = NULL;
    float *fp = NULL;

    ip = needHugeMem((size_t)(sizeof(int) * dv->count));
    memcpy((void *)ip, (void *)dv->position, (size_t)(sizeof(int)*dv->count));
    freeMem(dv->position);
    dv->position = ip;

    fp = needHugeMem((size_t)(sizeof(float) * dv->count));
    memcpy((void *)fp, (void *)dv->value, (size_t)(sizeof(float)*dv->count));
    freeMem(dv->value);
    dv->value = fp;

    dv->maxCount = dv->count;
    }
}

static void truncateVectors(struct dataVector *v1, struct dataVector *v2,
	int size)
/* reduce two intersected vectors to specified size */
{
int i;
double sumProducts = 0.0;
for (i = 0; i < size; ++i)
    sumProducts += v1->value[i] * v2->value[i];
v1->count = size;
v2->count = size;
v1->sumProduct = sumProducts;
v2->sumProduct = sumProducts;
v1->start = v1->position[0];
v1->end = v1->position[(v1->count)-1] + 1; /* non-inclusive */
v2->start = v2->position[0];
v2->end = v2->position[(v2->count)-1] + 1; /* non-inclusive */
}

static void intersectVectors(struct dataVector *v1, struct dataVector *v2)
/* collapse vector data sets down to only values when positions are equal
 *	Also, compute sumProduct while we have the values in hand. */
{
int v1Index = 0;
int v2Index = 0;
int i, j;
double sumProducts = 0.0;
long startTime, endTime;

startTime = clock1000();

i = 0;
j = 0;
v1Index = 0;
v2Index = 0;

while ((i < v1->count) && (j < v2->count))
    {
    if (v1->position[i] == v2->position[j])	/*	points accepted */
	{
	if (v1Index < i)		/* only move data when necessary */
	    {
	    v1->position[v1Index] = v1->position[i];
	    v1->value[v1Index] = v1->value[i];
	    }
	if (v2Index < j)		/* only move data when necessary */
	    {
	    v2->position[v2Index] = v2->position[j];
	    v2->value[v2Index] = v2->value[j];
	    }
	sumProducts += v1->value[v1Index] * v2->value[v2Index];
	++v1Index; ++v2Index; ++i; ++j;
	}
    else if (v1->position[i] < v2->position[j])
	++i;				/* skip data point on v1	*/
    else if (v1->position[i] > v2->position[j])
	++j;				/* skip data point on v2	*/
    }
/*	ensure sanity check	*/
if (v1Index != v2Index)
    errAbort("intersected table counts not equal: %d != %d\n",
	    v1->count, v2->count);
v1->count = v1Index;
v2->count = v2Index;
v1->sumProduct = sumProducts;
v2->sumProduct = sumProducts;
/*	if our count of numbers is more than 1000K less than planned for,
 *	save some space and reallocate the data lists.
 *	The potential savings here is three different data vectors,
 *	where each data point occupies about 8 bytes on a vector,
 *	thus: bytes saved at least 1024000 * 3 * 8 = 24,576,000
 */
if ((v1Index > 0) && ((v1->maxCount - v1Index) > 1024000))
    {
    int *ip;
    float *fp;

    ip = needHugeMem((size_t)(sizeof(int) * v1Index));
    memcpy((void *)ip, (void *)v1->position, (size_t)(sizeof(int)*v1Index));
    freeMem(v1->position);
    v1->position = ip;
    ip = needHugeMem((size_t)(sizeof(int) * v2Index));
    memcpy((void *)ip, (void *)v2->position, (size_t)(sizeof(int)*v2Index));
    freeMem(v2->position);
    v2->position = ip;

    fp = needHugeMem((size_t)(sizeof(float) * v1Index));
    memcpy((void *)fp, (void *)v1->value, (size_t)(sizeof(float)*v1Index));
    freeMem(v1->value);
    v1->value = fp;
    fp = needHugeMem((size_t)(sizeof(float) * v2Index));
    memcpy((void *)fp, (void *)v2->value, (size_t)(sizeof(float)*v2Index));
    freeMem(v2->value);
    v2->value = fp;
    }

v1->start = v1->position[0];
v1->end = v1->position[(v1->count)-1] + 1; /* non-inclusive */
v2->start = v2->position[0];
v2->end = v2->position[(v2->count)-1] + 1; /* non-inclusive */

#ifdef NOT_WORKING
XXXX something is wrong with needHugeMemResize, it corrupts the arena
if (1 || (v1Index * 2) < v1->maxCount)
    {
hPrintf("<P>before realloc: %#x, %#x, %#x, %#x</P>\n",
	(unsigned long) v1->position, (unsigned long) v1->value,
	(unsigned long) v2->position, (unsigned long) v2->value);
carefulCheckHeap();
    v1->position = (int *)needHugeMemResize((void *)v1->position, (size_t)(sizeof(int)*v1Index));
carefulCheckHeap();	XXXX exits here after this previous call
    v1->value = (float *)needHugeMemResize((void *)v1->value, (size_t)(sizeof(float)*v1Index));
    v2->position = (int *)needHugeMemResize((void *)v2->position, (size_t)(sizeof(int)*v2Index));
    v2->value = (float *)needHugeMemResize((void *)v2->value, (size_t)(sizeof(float)*v2Index));
hPrintf("<P>after realloc: %#x, %#x, %#x, %#x</P>\n",
	(unsigned long) v1->position, (unsigned long) v1->value,
	(unsigned long) v2->position, (unsigned long) v2->value);
    }
#endif

endTime = clock1000();
v1->calcTime += endTime - startTime;
}

static void intersectVectorWithBoolean(struct dataVector *v1,
				       struct dataVector *v2)
/* v1 has dataVector with real values; v2 has dataVector with booleans, 
 * i.e. all points in range are present and value is 1 where there is an 
 * item and 0 where there is no item. 
 * Remove positions/values from v1 for which v2 contains 0. 
 * Note: this does not change v2, unlike intersectVectors, nor does it 
 * compute any statistics.  It is solely for distilling v1. */
{
int v1Index = 0;
int i = 0, j = 0;
long startTime, endTime;

startTime = clock1000();

i = 0;
j = 0;
v1Index = 0;

while ((i < v1->count) && (j < v2->count))
    {
    if (v1->position[i] == v2->position[j])	/*	points in both... */
	{
	if (v2->value[j])                       /*	v2 TRUE: keep v1 */
	    {
	    if (v1Index < i) /* only move data when necessary */
		{
		v1->position[v1Index] = v1->position[i];
		v1->value[v1Index] = v1->value[i];
		}
	    ++v1Index; ++i; ++j;
	    }
	else                                    /*	v2 FALSE: skip v1 */
	    {
	    ++i; ++j;
	    }
	}
    else if (v1->position[i] < v2->position[j])
	++i;				/* 	skip data point on v1 */
    else if (v1->position[i] > v2->position[j])
	++j;				/* 	skip data point on v2 */
    }
v1->count = v1Index;
v1->start = v1->position[0];
v1->end = v1->position[(v1->count)-1] + 1; /* non-inclusive */
scavengeVectorSpace(v1);
endTime = clock1000();
v1->calcTime += endTime - startTime;
}

void dataVectorBinaryOp(struct dataVector *v1, struct dataVector *v2,
			enum dataVectorBinaryOpType op, boolean requireBoth)
/* Store op(v1, v2) in v1.  
 * If requireBoth is TRUE, remove a value from v1 if v2 has no value at the 
 * same position; if FALSE, leave v1 value unchanged if v2 has no value at 
 * same position. */
{
int i=0, j=0;
long startTime=0, endTime=0;

if (v1 == NULL)
    return;
if (requireBoth)
    {
    if (v2 == NULL || v2->count < 1)
	{
	v1->count = 0;
	return;
	}
    intersectVectors(v1, v2);
    }

startTime = clock1000();
for (i=0, j=0;  i < v1->count && j < v2->count; )
    {
    if (v1->position[i] == v2->position[j])
	{
	switch (op)
	    {
	    case dataVectorBinaryOpSum:
		v1->value[i] += v2->value[j];
		break;
	    case dataVectorBinaryOpProduct:
		v1->value[i] *= v2->value[j];
		break;
	    case dataVectorBinaryOpMin:
		if (v2->value[j] < v1->value[i])
		    v1->value[i] = v2->value[j];
		break;
	    case dataVectorBinaryOpMax:
		if (v2->value[j] > v1->value[i])
		    v1->value[i] = v2->value[j];
		break;
	    default:
		errAbort("dataVectorBinaryOp: unrecognized type %d", op);
	    }
	i++;  j++;
	}
    else if (v1->position[i] < v2->position[j])
	i++;
    else
	j++;
    }
endTime = clock1000();
v1->calcTime += endTime - startTime;
}

void dataVectorNormalize(struct dataVector *dv, float denominator)
/* Divide each value in dv by denominator. */
{
int i=0;
long startTime=0, endTime=0;
if (dv == NULL)
    return;
startTime = clock1000();
for (i=0;  i < dv->count;  i++)
    {
    dv->value[i] /= denominator;
    }
endTime = clock1000();
dv->calcTime += endTime - startTime;
}

void dataVectorFilterMin(struct dataVector *dv, float minScore)
/* Remove values less than minScore from dv. */
{
int i=0, dvIndex=0;
long startTime=0, endTime=0;
if (dv == NULL)
    return;
startTime = clock1000();
for (i=0;  i < dv->count;  i++)
    {
    if (dv->value[i] >= minScore)
	{
	dv->value[dvIndex] = dv->value[i];
	dv->position[dvIndex] = dv->position[i];
	dvIndex++;
	}
    }
dv->count = dvIndex;
dv->start = dv->position[0];
dv->end = dv->position[(dv->count)-1] + 1; /* non-inclusive */
scavengeVectorSpace(dv);
endTime = clock1000();
dv->calcTime += endTime - startTime;
}

struct dataVector *dataVectorInvert(struct dataVector *dv, int start, int end)
/* Return a new dataVector that has a value of 1 where dv has no value, 
 * and no value where dv has any value, with positions in [start,end). */
{
struct dataVector *dvNew = allocDataVector("inv", end-start);
if (dv == NULL || dv->count < 1)
    {
    int i=0;
    for (i=0;  i < end-start;  i++)
	{
	dvNew->position[i] = i + start;
	dvNew->value[i] = 1;
	}
    dvNew->count = end-start;
    }
else
    {
    int oldIndex = 0, newIndex = 0;
    int pos = start;
    while (oldIndex < dv->count && pos < end)
	{
	if (dv->position[oldIndex] < start)
	    oldIndex++;
	else if (dv->position[oldIndex] == pos)
	    {
	    oldIndex++;
	    pos++;
	    }
	else /* dv->pos > pos */
	    {
	    dvNew->position[newIndex] = pos;
	    dvNew->value[newIndex] = 1;
	    pos++;
	    newIndex++;
	    }
	}
    while (pos < end)
	{
	dvNew->position[newIndex] = pos;
	dvNew->value[newIndex] = 1;
	pos++;
	newIndex++;
	}
    dvNew->count = newIndex;
    scavengeVectorSpace(dvNew);
    }
return dvNew;
}

struct dataVector *dataVectorBoolComp(struct dataVector *dv, int start, int end)
/* Return a new dataVector that has a value of 1 where dv has a value of 0, 
 * and a value of 0 where dv has any nonzero value, with positions in 
 * [start,end). */
{
struct dataVector *dvNew = allocDataVector("comp", end-start);
if (dv == NULL || dv->count < 1)
    {
    int i=0;
    for (i=0;  i < end-start;  i++)
	{
	dvNew->position[i] = i + start;
	dvNew->value[i] = 1;
	}
    dvNew->count = end-start;
    }
else
    {
    int oldIndex = 0, newIndex = 0;
    int pos = start;
    while (oldIndex < dv->count && pos < end)
	{
	if (dv->position[oldIndex] < start)
	    oldIndex++;
	else if (dv->position[oldIndex] == pos)
	    {
	    dvNew->position[newIndex] = pos;
	    if (dv->value[oldIndex])
		dvNew->value[newIndex] = 0;
	    else
		dvNew->value[newIndex] = 1;
	    oldIndex++;
	    newIndex++;
	    pos++;
	    }
	else /* dv->pos > pos */
	    {
	    dvNew->position[newIndex] = pos;
	    dvNew->value[newIndex] = 1;
	    pos++;
	    newIndex++;
	    }
	}
    while (pos < end)
	{
	dvNew->position[newIndex] = pos;
	dvNew->value[newIndex] = 1;
	pos++;
	newIndex++;
	}
    dvNew->count = newIndex;
    scavengeVectorSpace(dvNew);
    }
return dvNew;
}

void dataVectorIntersect(struct dataVector *dv1, struct dataVector *dv2,
			 boolean dv2IsWiggle, boolean complementDv2)
/* Remove a value from dv1 if dv2 has no value (or if complementDv2, any value)
 * at the same position.  Note: this will change dv2 too, unless complementDv2 
 * is set! */
{
if (dv1 == NULL || dv1->count < 1)
    return;
if (complementDv2)
    {
    int dv1Start = dv1->position[0];
    int dv1End = dv1->position[dv1->count-1];
    struct dataVector *dv2Inv = NULL;
    if (dv2 == NULL || dv2->count < 1)
	return; /* Complement of nothing is everything, so no change to dv1 */
    if (dv2IsWiggle)
	dv2Inv = dataVectorInvert(dv2, dv1Start, dv1End);
    else
	dv2Inv = dataVectorBoolComp(dv2, dv1Start, dv1End);
    if (dv2Inv == NULL || dv2Inv->count < 1)
	dv1->count = 0;
    else
	{
	if (dv2IsWiggle)
	    intersectVectors(dv1, dv2Inv);
	else
	    intersectVectorWithBoolean(dv1, dv2Inv);
	}
    freeDataVector(&dv2Inv);
    }
else
    {
    if (dv2 == NULL || dv2->count < 1)
	dv1->count = 0;
    else
	{
	if (dv2IsWiggle)
	    intersectVectors(dv1, dv2);
	else
	    intersectVectorWithBoolean(dv1, dv2);
	}
    }
}

int dataVectorWriteWigAscii(struct dataVector *dataVectorList,
			    char *filename, int maxOut, char *description)
/* Write wigAscii for all dataVectors in dataVectorList to filename. 
 * Return the number of datapoints written. */
{
FILE *out = mustOpen(filename, "w");
struct dataVector *dv = NULL;
int i = 0, total = 0;
boolean firstTime = TRUE;

for (dv = dataVectorList;  dv != NULL;  dv = dv->next)
    {
    int countLimit = maxOut ? min(dv->count, (maxOut - total)) : dv->count;
    if (dv->count < 1)
	continue;
    if (firstTime)
	{
	time_t now = time(NULL);
	char *dateStamp = sqlUnixTimeToDate(&now,TRUE);	/* TRUE == gmTime */
	fprintf(out, "#\toutput date: %s UTC\n", dateStamp);
	freez(&dateStamp);
	firstTime = FALSE;
	}
    fprintf(out, "#\tchrom specified: %s\n", dv->chrom);
    fprintf(out, "#\tposition specified: %d-%d\n", dv->start, dv->end);
    if (description)
	fprintf(out, "%s", description);
    fprintf(out, "variableStep chrom=%s span=1\n", dv->chrom);
    for (i=0;  i < countLimit;  i++)
	{
	fprintf(out, "%d\t%g\n",
	       dv->position[i]+1, dv->value[i]);
	}
    total += countLimit;
    if (maxOut && (total >= maxOut))
        break;
    }
carefulClose(&out);
return total;
}

struct bed *dataVectorToBedList(struct dataVector *dvList)
/* Allocate and return a bedList of ranges where dataVectors in dvList 
 * contain values. */
{
struct dataVector *dv = NULL;
struct bed *bedList = NULL;
int i=0, start=0, lastPos=0;
int n=0;

for (dv = dvList;  dv != NULL; dv = dv->next)
    {
    start = lastPos = dv->position[0];
    for (i=1;  i < dv->count;  i++)
	{
	int pos = dv->position[i];
	if (pos != (lastPos+1))
	    {
	    struct bed *bed = NULL;
	    char buf[256];
	    AllocVar(bed);
	    bed->chrom = cloneString(dv->chrom);
	    bed->chromStart = start;
	    bed->chromEnd = lastPos+1;
	    safef(buf, sizeof(buf), "%s.%d", dv->chrom, ++n);
	    bed->name = cloneString(buf);
	    slAddHead(&bedList, bed);
	    start = pos;
	    }
	lastPos = pos;
	}
    if (dv->count > 0)
	{
	struct bed *bed = NULL;
	char buf[256];
	AllocVar(bed);
	bed->chrom = cloneString(dv->chrom);
	bed->chromStart = start;
	bed->chromEnd = lastPos+1;
	safef(buf, sizeof(buf), "%s.%d", dv->chrom, ++n);
	bed->name = cloneString(buf);
	slAddHead(&bedList, bed);
	}
    }
slReverse(&bedList);
return bedList;
}

int dataVectorWriteBed(struct dataVector *dvList, char *filename, int maxOut,
		       char *description)
/* Print out bed of ranges where dataVectors in dvList contain values. 
 * Return the number of datapoints distilled into bed (the number of bed 
 * items will usually be smaller than the number of datapoints). */
{
FILE *out = mustOpen(filename, "w");
struct dataVector *dv = NULL;
int i=0, total=0, start=0, lastPos=0;
int n=0;
boolean firstTime = TRUE;

for (dv = dvList;  dv != NULL; dv = dv->next)
    {
    int countLimit = maxOut ? min(dv->count, (maxOut - total)) : dv->count;
    if (dv->count < 1)
	continue;
    if (firstTime)
	{
	if (description)
	    fprintf(out, "%s", description);
	firstTime = FALSE;
	}
    start = lastPos = dv->position[0];
    for (i=1;  i < countLimit;  i++)
	{
	int pos = dv->position[i];
	if (pos != (lastPos+1))
	    {
	    fprintf(out, "%s\t%d\t%d\t%s.%d\n", dv->chrom, start, lastPos+1,
		    dv->chrom, ++n);
	    start = pos;
	    }
	lastPos = pos;
	}
    fprintf(out, "%s\t%d\t%d\t%s.%d\n", dv->chrom, start, lastPos+1,
	    dv->chrom, ++n);
    total += countLimit;
    if (maxOut && (total >= maxOut))
        break;
    }
carefulClose(&out);
return total;
}

static struct dataVector *tbDataVectorOneRegion(struct trackTable *tt,
	struct region *region, struct sqlConnection *conn)
/* Get a data vector on which all table browser processing has been performed:
 * filter, subtrack merge, and intersection (some of which require a lower-
 * level data vector fetch followed by some processing).  */
{
struct dataVector *dv = NULL;
/* For wiggle and bedGraph, subtrack merge is implemented on top of 
 * dataVectorFetchOneRegion.  For bed-able tables, it's handled below 
 * dataVectorFetchOneRegion (under cookedBedList). */
if (isWiggle(database, tt->tableName))
    dv = wiggleDataVector(tt->tdb, tt->tableName, conn, region);
else if (isBedGraph(tt->tableName))
    dv = bedGraphDataVector(tt->tableName, conn, region);
else
    dv = dataVectorFetchOneRegion(tt, region, conn);
return dv;
}

static void overallCounts(struct trackTable *t)
/*	find overall min,max,sum,sumSquares in a set of data vectors	*/
{
register double minimum = INFINITY;
register double maximum = -INFINITY;
double sumData = 0.0;
double sumSquares = 0.0;
int count = 0;
struct dataVector *dv;

for (dv = t->vSet; dv != NULL; dv = dv->next)
    {
    minimum = min(minimum,dv->min);
    maximum = max(maximum,dv->max);
    count += dv->count;
    sumData += dv->sumData;
    sumSquares += dv->sumSquares;
    }
t->count = count;
t->min = minimum;
t->max = maximum;
t->sumData = sumData;
t->sumSquares = sumSquares;
}

static void calcMeanMinMax(struct dataVector *v)
{
long startTime, endTime;
register double sum = 0.0;
register double sumSquares = 0.0;
register int i;
register double value;
register double min;
register double max;

startTime = clock1000();
min = v->min;	/*	was initialized to INFINITY	*/
max = v->max;	/*	was initialized to -INFINITY	*/
for (i = 0; i < v->count; ++i)
    {
    value = v->value[i];
    min = min(min,value);
    max = max(max,value);
    sum += value;
    sumSquares += (value * value);
    }
v->sumData = sum;
v->sumSquares = sumSquares;
v->min = min;
v->max = max;
endTime = clock1000();
v->calcTime += endTime - startTime;
}	/*	static void calcMeanMinMax(struct dataVector *v)	*/

static void statsRowOut(char *chrom, char *name, char *shortLabel,
    int chromStart, int chromEnd, int dataPoints, double min, double max,
	double sumData, double sumSquares, double r, long fetchMs, long calcMs,
	    char *table1, char *table2, char *compositeTable1,
		char *compositeTable2, double a, double b)
/*	display a single line of statistics	*/
{
double mean = 0.0;
double variance = 0.0;
double stddev = 0.0;

if (dataPoints > 0)
    {
    mean = sumData/dataPoints;
    if (dataPoints > 1)
	{
	variance = (sumSquares - ((sumData*sumData)/dataPoints)) /
		(double)(dataPoints-1);
	if (variance > 0.0)
	    stddev = sqrt(variance);
	}
    }

hPrintf("<TR>");
/*	Check for first or second data rows, 2nd has 0,0 coordinates.
 *	Summary row has 1,1 start,end for the first data	*/
if (chromStart || chromEnd)
    {
    if ((1 == chromStart) && (1 == chromEnd))	/* OVERALL first row */
	hPrintf("<TD ALIGN=LEFT ROWSPAN=2>%s", chrom);
    else
	{
	char posBuf[64];
	safef(posBuf, ArraySize(posBuf), "%s:%d-%d",
		chrom, chromStart+1, chromEnd);
	hPrintf("<TD ALIGN=LEFT ROWSPAN=2>");
	/* Construct browser anchor URL with tracks we're looking at open. */
	hPrintf("<A HREF=\"%s?%s", hgTracksName(), cartSidUrlString(cart));
	hPrintf("&db=%s", database);
	hPrintf("&position=%s", posBuf);
	if ((compositeTable1 != NULL)&&differentWord(table1, compositeTable1))
	    {
	    hPrintf("&%s=%s", compositeTable1, hTrackOpenVis(database, compositeTable1));
	    hPrintf("&%s_sel=1", table1);
	    }
	else
	    hPrintf("&%s=%s", table1, hTrackOpenVis(database, table1));
	if (table2 != NULL)
	    {
	    if ((compositeTable2 != NULL)
			&&differentWord(table2, compositeTable2))
		{
		hPrintf("&%s=%s", compositeTable2,
			hTrackOpenVis(database, compositeTable2));
		hPrintf("&%s_sel=1", table2);
		}
	    else
		hPrintf("&%s=%s", table2, hTrackOpenVis(database, table2));
	    }
	hPrintf("\" TARGET=_blank>");
	if (name)
	    {
	    hPrintf("%s</A>", name);
	    }
	else
	    {
	    hPrintf("%s:", chrom);
	    if (!((1 == chromStart) && (1 == chromEnd)))
		{
		/*	browser 1-relative start coordinates	*/
		printLongWithCommas(stdout,(long)chromStart+1);
		hPrintf("-");
		printLongWithCommas(stdout,(long)chromEnd);
		}
	    hPrintf("</A>");
	    }
	}
    hPrintf("<BR>\n");
    printLongWithCommas(stdout,(long)dataPoints);
    hPrintf("&nbsp;data&nbsp;points</TD>");
    }

if (chromStart || chromEnd)
    {
    hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", r);
    hPrintf("<TD ALIGN=RIGHT><B>%.4g</B></TD>", r*r);
    }
else
    {
    hPrintf("<TD COLSPAN=2>&nbsp;</TD>");
    }
hPrintf("<TD ALIGN=LEFT>%s</TD>", shortLabel);
hPrintf("<TD ALIGN=RIGHT>%.3g</TD>", min);
hPrintf("<TD ALIGN=RIGHT>%.3g</TD>", max);
hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", mean);
hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", variance);
hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", stddev);
if (chromStart || chromEnd)
    {
    hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", a);
    hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", b);
    }
else
    hPrintf("<TD COLSPAN=2>&nbsp;</TD>");
/*
hPrintf("<TD ALIGN=RIGHT>%.3f</TD>", 0.001*fetchMs);
hPrintf("<TD ALIGN=RIGHT>%.3f</TD></TR>\n", 0.001*calcMs);
*/
hPrintf("</TR>\n");
}

static void tableLabels()
{
hPrintf("<TR><TH>Position&nbsp;and<BR>#&nbsp;of&nbsp;data&nbsp;points&nbsp;in<BR>"
	"intersection</TH>");
hPrintf("<TH>Correlation<BR>coefficient<BR>r</TH>");
hPrintf("<TH>r<sup>2</sup></TH>");
hPrintf("<TH>Track</TH>");
hPrintf("<TH>Minimum</TH><TH>Maximum</TH><TH>Mean</TH><TH>Variance</TH>");
hPrintf("<TH>Standard<BR>deviation</TH>");
hPrintf("<TH COLSPAN=2>");
hPrintf("<TABLE BGCOLOR=\"%s\" BORDER=0 WIDTH=100%%>", HG_COL_INSIDE);
hPrintf("<TR WIDTH=100%%><TH COLSPAN=2>Regression<BR>line<BR>");
hPrintf("y&nbsp;=&nbsp;m*x&nbsp;+&nbsp;b</TH></TR>");
hPrintf("<TR><TH>m</TH><TH>b</TH></TR></TABLE>");
hPrintf("</TH>");
/*
hPrintf("<TH>Data&nbsp;fetch<BR>time&nbsp;(sec)</TH>");
hPrintf("<TH>Calculation<BR>time&nbsp;(sec)</TH></TR>\n");
*/
hPrintf("</TR>\n");
}

static void showThreeVectors(struct trackTable *table1,
    struct trackTable *table2, struct dataVector *result)
{
struct dataVector *v1 = table1->vSet;
struct dataVector *v2 = table2->vSet;
int rowsOutput = 0;
int totalBases1 = 0;
int totalBases2 = 0;
long totalFetch1 = 0;
long totalFetch2 = 0;
long totalCalc1 = 0;
long totalCalc2 = 0;
double totalSum1 = 0.0;
double totalSumSquares1 = 0.0;
double totalSum2 = 0.0;
double totalSumSquares2 = 0.0;
double min1 = INFINITY;
double min2 = INFINITY;
double max1 = -INFINITY;
double max2 = -INFINITY;

/*	start the table with the funky border thing to make sure all the
 *	inner border lines work
 */
hPrintf("<P><!--outer table is for border purposes-->\n");
hPrintf("<TABLE BGCOLOR=\"#%s",HG_COL_BORDER);
hPrintf("\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");
hPrintf("<TABLE BGCOLOR=\"%s\" BORDER=1>", HG_COL_INSIDE);

tableLabels();

rowsOutput = 0;

for ( ; (v1 != NULL) && (v2 !=NULL); v1 = v1->next, v2=v2->next)
    {
    ++rowsOutput;
    min1 = min(min1,v1->min);
    min2 = min(min2,v2->min);
    max1 = max(max1,v1->max);
    max2 = max(max2,v2->max);
    totalBases1 += v1->count;
    totalBases2 += v2->count;
    totalFetch1 += v1->fetchTime;
    totalFetch2 += v2->fetchTime;
    totalCalc1 += v1->calcTime;
    totalCalc2 += v2->calcTime;
    totalSum1 += v1->sumData;
    totalSum2 += v2->sumData;
    totalSumSquares1 += v1->sumSquares;
    totalSumSquares2 += v2->sumSquares;
    }

if ((totalBases1 != totalBases2) || (totalBases1 != result->count))
    {
    hPrintf("<P>what is this, total bases do not add up properly</P>\n");
    hPrintf("<P>totalBases1,2: %d, %d, result count: %d</P>\n",
		totalBases1, totalBases2, result->count);
    }

if (0 == rowsOutput)
    hPrintf("<TR><TD COLSPAN=11>EMPTY RESULT SET</TD></TR>\n");
else if (rowsOutput > 1)
    {
    statsRowOut("OVERALL", NULL, table1->shortLabel, 1, 1,
	totalBases1, min1, max1, totalSum1, totalSumSquares1,
	    result->r, totalFetch1, totalCalc1, table1->actualTable,
		table2->actualTable, NULL, NULL, result->m, result->b);
    statsRowOut("OVERALL", NULL, table2->shortLabel, 0, 0,
	totalBases2, min2, max2, totalSum2, totalSumSquares2,
	    result->r, totalFetch2, totalCalc2, table1->actualTable,
		table2->actualTable, NULL, NULL, 0.0, 0.0);
    hPrintf("<TR><TD COLSPAN=11><HR></TD></TR>\n");
    }

rowsOutput = 0;
v1 = table1->vSet;
v2 = table2->vSet;
for ( ; (v1 != NULL) && (v2 !=NULL); v1 = v1->next, v2=v2->next)
    {
    ++rowsOutput;
    statsRowOut(v1->chrom, v1->name, table1->shortLabel, v1->start, v1->end,
	v1->count, v1->min, v1->max, v1->sumData,
	    v1->sumSquares, v1->r, v1->fetchTime,
		v1->calcTime, table1->actualTable, table2->actualTable,
		    table1->tdb->tableName, table2->tdb->tableName, v1->m,
			v1->b);
    statsRowOut(v2->chrom, v2->name, table2->shortLabel, 0, 0,
	v2->count, v2->min, v2->max, v2->sumData,
	    v2->sumSquares, v2->r, v2->fetchTime,
		v2->calcTime, table1->actualTable, table2->actualTable,
		    NULL, NULL, 0.0, 0.0);
    }

if (rowsOutput > 1)
    {
    hPrintf("<TR><TD COLSPAN=11><HR></TD></TR>\n");
    statsRowOut("OVERALL", NULL, table1->shortLabel, 1, 1,
	totalBases1, min1, max1, totalSum1, totalSumSquares1,
	    result->r, totalFetch1, totalCalc1, table1->actualTable,
		table2->actualTable, NULL, NULL, result->m, result->b);
    statsRowOut("OVERALL", NULL, table2->shortLabel, 0, 0,
	totalBases2, min2, max2, totalSum2, totalSumSquares2,
	    result->r, totalFetch2, totalCalc2, table1->actualTable,
		table2->actualTable, NULL, NULL, 0.0, 0.0);

    tableLabels();
    }

hPrintf("</TABLE>\n");
/*	and end the special container table	*/
hPrintf("</TD></TR></TABLE></P>\n");

/*	debugging when 1 region, less than 100000 dataPoints, show all values	*/
if ((1 == 0) && ((1 == rowsOutput) || (1 != rowsOutput)) &&
	(table1->count < 100000) && (table2->count < 100000))
    {
    v1 = table1->vSet;
    v2 = table2->vSet;
    hPrintf("<PRE>\n");
    hPrintf("# Position\t%s\t%s\tResiduals\n",
	    table1->shortLabel, table2->shortLabel);

    for ( ; (v1 != NULL) && (v2 !=NULL); v1 = v1->next, v2=v2->next)
	{
	int start, end, i;
	int v1Index = 0;
	int v2Index = 0;
	start = min(v1->start,v2->start);
	end = max(v1->end,v2->end);

	for (i = start; i < end; ++i)
	    {
	    int printResidual = 0;

	    if ((i == v1->position[v1Index]) || (i == v2->position[v2Index]))
		hPrintf("%d\t", i+1);

	    if (i >= v1->start)
		{
		if (i == v1->position[v1Index])
		    {
		    hPrintf("%g\t", v1->value[v1Index]);
		    ++v1Index;
		    ++printResidual;
		    }
		else if (i > v1->position[v1Index])
		    {
		    hPrintf("N/A\t");
		    ++v1Index;
		    }
		}
	    else
		hPrintf("N/A\t");
	    if (i >= v2->start)
		{
		if (i == v2->position[v2Index])
		    {
		    hPrintf("%g\t", v2->value[v2Index]);
		    ++v2Index;
		    ++printResidual;
		    }
		else if (i > v2->position[v2Index])
		    {
		    hPrintf("N/A\t");
		    ++v2Index;
		    }
		}
	    else
		hPrintf("N/A\t");
	    if (printResidual == 2)
		hPrintf("%g\n", result->value[v1Index-1]);
	    }
	}
    hPrintf("</PRE>\n");
    }
}	/*	static void showThreeVectors()	*/

static int windowData(struct trackTable *tableList, int winSize)
/*	window data in first and second table to winSize	*/
{
struct trackTable *yTable = tableList;
struct trackTable *xTable = tableList->next;
struct dataVector *y;	/* the response variable of interest */
struct dataVector *x;	/* the estimator variable, predicting the response */
int totalDataPoints = 0;

hPrintf("<P><B>window data to %d bases per data point</B></P>\n",winSize);

/*	for each set of data (for each region we have collected data)
 *	The length of both data sets is expected to be the same,
 *	i.e. y->count == x->count
 */
for (y = yTable->vSet, x = xTable->vSet; (y != NULL) && (x != NULL);
		y = y->next, x = x->next)
    {
    /*	only need to process where more than one number	*/
    if (y->count > 1)
	{
	int winStart = y->position[0];
	int winEnd = winStart + winSize;
	int newDataCount = 0;	/* index for accumulating new data */
	int lastPosition = y->position[(y->count)-1];
	double ySumData = y->value[0];
	double xSumData = x->value[0];
	int dataCountInWindow = 1;
	int i;

	y->sumProduct = 0.0;
	x->sumProduct = y->sumProduct;

	if (winEnd > lastPosition) winEnd = lastPosition;

	for (i = 1; i < y->count; ++i)
	    {
	    if (y->position[i] < winEnd)	/* accumulating in window */
		{
		ySumData += y->value[i];
		xSumData += x->value[i];
		++dataCountInWindow;
		}
	    else 				/* window completed	*/
		{
		if (dataCountInWindow > 0)
		    {
		    y->value[newDataCount] = ySumData / dataCountInWindow;
		    x->value[newDataCount] = xSumData / dataCountInWindow;
		    y->position[newDataCount] = winStart;
		    x->position[newDataCount] = winStart;
		    y->sumProduct += y->value[newDataCount] *
				x->value[newDataCount];
		    x->sumProduct = y->sumProduct;
		    ++newDataCount;
		    }
		winStart = y->position[i];
		winEnd = winStart + winSize;
		if (winEnd > lastPosition) winEnd = lastPosition;
		ySumData = y->value[i];		/*	new sum starts	*/
		xSumData = x->value[i];
		dataCountInWindow = 1;
		}
	    }
	if (dataCountInWindow > 0)		/*	last one perhaps ? */
	    {
	    y->value[newDataCount] = ySumData / dataCountInWindow;
	    x->value[newDataCount] = xSumData / dataCountInWindow;
	    y->position[newDataCount] = winStart;
	    x->position[newDataCount] = winStart;
	    y->sumProduct += y->value[newDataCount] * x->value[newDataCount];
	    x->sumProduct = y->sumProduct;
	    ++newDataCount;
	    }
	totalDataPoints += newDataCount;
	y->count = newDataCount;
	x->count = newDataCount;
	}
    else
	totalDataPoints += y->count;	/*	only one data value	*/
    }
return (totalDataPoints);
}	/*	static int windowData()	*/

static int collectData(struct sqlConnection *conn,
	struct trackTable *tableList, int maxLimitCount)
/*	for each track in the list, collect its raw data */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
struct region *regionList = getRegions();
struct region *region;
int regionCount = 0;
struct trackTable *table1;
struct trackTable *table2;
struct dataVector *vSet1;
struct dataVector *vSet2;
int totalBases = 0;
boolean reachedMaxLimit = FALSE;

vSet1 = NULL;	/*	initialize linked list	*/
vSet2 = NULL;	/*	initialize linked list	*/

table1 = tableList;
table2 = tableList->next;

/*	count the regions, just to get an idea of where we may be going */
for (region = regionList; region != NULL; region = region->next)
    ++regionCount;

//hPrintf("<P>regionCount: %d</P>\n", regionCount);

/*	show the region(s) being worked on	*/
if (1 == regionCount)
    {
    long bases = 0;
    region = regionList;
    bases = region->end - region->start;
    hPrintf("<P><B>position:</B> %s:", region->chrom);
    printLongWithCommas(stdout,(long)(region->start+1)); /* 1-relative */
    hPrintf("-");
    printLongWithCommas(stdout,(long)region->end);
    hPrintf("&nbsp;<B>bases:</B>&nbsp;");
    printLongWithCommas(stdout,bases);
    hPrintf("</P>\n");
    }
else if (sameString(regionType,"genome"))
    {
    hPrintf("<P><B>position:</B> full genome, %d chromosomes</P>\n", regionCount);
    }
else if (sameString(regionType,"encode"))
    hPrintf("<P><B>position:</B> %d encode regions</P>\n", regionCount);

if ( (table1->isCustom && checkWigDataFilter("ct", curTable, NULL, NULL, NULL))
	|| checkWigDataFilter(database, curTable, NULL, NULL, NULL))
    {
    hPrintf("<P><B>data filter on</B> '%s':", table1->shortLabel);
    wigShowFilter(conn);
    hPrintf("</P>\n");
    }
if (anySubtrackMerge(database, table1->actualTable))
    {
    hPrintf("<P><B>subtrack merge as specified on </B>'%s'\n",
	    table1->shortLabel);
    }
if (anyIntersection())
    {
    hPrintf("<P><B>intersecting both tables</B> ");
    hPrintf("'%s' <B>and</B> '%s' <B>with</B> '%s':</P>\n",
	    table1->shortLabel, table2->shortLabel,
	    cartString(cart, hgtaIntersectTrack));
    }

/*	walk through the regions and fetch each table's data
 *	after you have them both, condense them down to only those
 *	places that overlap each other
 */
for (region = regionList; (totalBases < maxLimitCount) && (region != NULL);
	region = region->next)
    {
    struct dataVector *v1;
    struct dataVector *v2;
    long bases = region->end - region->start;

    if (bases > MAX_POINTS)
	{
	hPrintf("<P><B>warning:</B> This region: %s:%d-%d is too large",
		region->chrom, region->start, region->end);
	hPrintf(" to process.  This function can currently only work on\n");
	hPrintf("regions of less than %s bases. Perhaps future\n",
		MAX_POINTS_STR);
	hPrintf("improvements will raise this limit.\n");
	errAbort(" ");
	}

    /*	makes no sense to work on less than two numbers	*/
    if (bases < 2)
	continue;	/*	next region	*/

    v1 = tbDataVectorOneRegion(table1, region, conn);
    if (v1)	/*	if data there, fetch the second one	*/
	{
	v2 = tbDataVectorOneRegion(table2, region, conn);
	if (v2)	/*	if both data, construct a result set	*/
	    {
	    intersectVectors(v1, v2);
	    if (v1->count < 1)	/*	possibly no intersection	*/
		{
		freeDataVector(&v1);	/* no data in intersection	*/
		freeDataVector(&v2);	/* no data in intersection	*/
		continue;		/* next region	*/
		}
	    if ((totalBases + v1->count) >= maxLimitCount)
		{
		int size = maxLimitCount - totalBases;
		truncateVectors(v1, v2, size);
		reachedMaxLimit = TRUE;
		}
	    totalBases += v1->count;
	    if (region->name)
		{
		v1->name = cloneString(region->name);
		v2->name = cloneString(region->name);
		}
	    slAddTail(&vSet1, v1);
	    slAddTail(&vSet2, v2);
	    }
	else
	    freeDataVector(&v1);	/* no data in second	*/
	}
    }

if (reachedMaxLimit)
    {
    hPrintf("<B>warning:</B> reached maximum data points: ");
    printLongWithCommas(stdout,(long)maxLimitCount);
    hPrintf(" before end of data.<BR>");
    }

/*	returning results, the collected data	*/
table1->vSet = vSet1;
table2->vSet = vSet2;
return totalBases;
}	/*	static void collectData()	*/

static struct dataVector *runRegression(struct trackTable *tableList,
    int totalBases)
/*	runs regression calculations on the first two tables in the
 *	tableList, returning a residual vector result
 */
{
struct trackTable *yTable, *xTable;
struct dataVector *result = NULL;
struct dataVector *y;	/* the response variable of interest */
struct dataVector *x;	/* the estimator variable, predicting the response */
double totalSumProduct = 0.0;
double ySumData = 0.0;
double xSumData = 0.0;
double ySumSquares = 0.0;
double xSumSquares = 0.0;
double yVariance = 0.0;
double xVariance = 0.0;
double yStdDev = 0.0;
double xStdDev = 0.0;
long startTime, endTime;
int rIndex = 0;		/* index for result vector data values	*/

/*	expecting two tables	*/
if (NULL == tableList)
    internalErr();	/*	expecting two tables	*/

yTable = tableList;
xTable = tableList->next;
if (NULL == xTable)
    internalErr();	/*	expecting two tables	*/

/*	find the mean, min and max	*/
for (y = yTable->vSet; y != NULL; y = y->next)
    calcMeanMinMax(y);
for (x = xTable->vSet; x != NULL; x = x->next)
    calcMeanMinMax(x);

/*	XXXX - this single result vector is going to lose the chrom
 *	identification for multiple region results.  We need that later
 *	for sending this overall result back to a custom track.
 *	It can be reconstructed because there is a one-to-one
 *	relationship between the result vector and the y data set
 *	positions.
 */
result = allocDataVector("result", totalBases);
rIndex = 0;		/* index for result vector data values	*/

/*	This loop walks through all the regions in the data vectors.
 *	Each region will have its own r calculated, and stats will be
 *	accumulated to enable an overall result to be calculated at the
 *	end.
 */
for (y = yTable->vSet, x = xTable->vSet; (y != NULL) && (x != NULL);
		y = y->next, x = x->next)
    {
    double sumProduct = y->sumProduct;	/*	sum of products */
    double Sxy = 0.0;		/* aka Numerator of the CoVariance */
    double Sxx = 0.0;	/* aka numerator of the Variance */
    double Syy = 0.0;	/* aka numerator of the Variance */

    startTime = clock1000();

    if ((y->count != x->count) || (0 == y->count))
     errAbort("unequal sized, or zero length data vectors given to regression");

    result->count += y->count;
    totalSumProduct += sumProduct;
    ySumData += y->sumData;
    xSumData += x->sumData;
    ySumSquares += y->sumSquares;
    xSumSquares += x->sumSquares;
    result->fetchTime += y->fetchTime;
    result->fetchTime += x->fetchTime;
    result->calcTime += y->calcTime;
    result->calcTime += x->calcTime;
    /* Sxy - page 101, page 488, Mendenhall, Beaver, Beaver - 2003 */
    Sxy = sumProduct - ((y->sumData*x->sumData)/y->count);
    /* Sxx - page 61, page 488, Mendenhall, Beaver, Beaver - 2003 */
    Sxx = x->sumSquares - ((x->sumData*x->sumData)/x->count);
    Syy = y->sumSquares - ((y->sumData*y->sumData)/y->count);

    /* slope m = Sxy / Sxx  page 489 (== b) Mendenhall, Beaver, Beaver - 2003 */
    if (0.0 != Sxx)
	y->m = Sxy / Sxx;
    else
	y->m = 0;
    /*	what does it mean if the Sxx is zero ?	*/

    /* intercept b = mean(y) - m*mean(x) page 489 (== a) Mendenhall, Beaver,
	Beaver - 2003 */
    y->b = (y->sumData/y->count) - (y->m * (x->sumData/x->count));

    /*	if we still have the two data sets, compute the residuals */
    if ((y->value != NULL) && (x->value != NULL))
	{
	int i;
	for (i = 0; i < y->count; ++i)
	    {
	    /*	residual = actual - predicted */
	    result->value[rIndex]=y->value[i]-((y->m * x->value[i]) + y->b);
	    result->position[rIndex] = y->position[i];
	    ++rIndex;
	    }
	result->start = result->position[0];
	result->end = result->position[rIndex-1] + 1;/*non inclusive*/
	}

    x->r = y->r = 0.0;

    /*	do not compute r for this region if N is < 2	*/
    if (y->count > 1)
	{
    /*	For this region, ready to compute r, N is y->count	*/
    yVariance = Syy / (double)(y->count-1);
    xVariance = Sxx / (double)(x->count-1);
    yStdDev = xStdDev = 0.0;
    if (yVariance > 0.0) yStdDev = sqrt(yVariance);
    if (xVariance > 0.0) xStdDev = sqrt(xVariance);
    /*	page 101,498, Mendenhall, Beaver, Beaver - 2003 */
    if ((yStdDev * xStdDev) != 0.0)
	x->r = y->r = (Sxy / (y->count - 1)) / (yStdDev * xStdDev);
    else
	x->r = y->r = 1.0;
	}
    endTime = clock1000();
    result->calcTime += endTime - startTime;
    }

if (rIndex != result->count)
 errAbort("did not get the correct number of data points in the result vector");

result->r = 0.0;
if (result->count > 1)
    {
    double Sxy = 0.0;
    double Sxx = 0.0;
    double Syy = 0.0;

    Sxy = totalSumProduct - ((ySumData*xSumData)/result->count);
    Sxx = xSumSquares - ((xSumData*xSumData)/result->count);
    Syy = ySumSquares - ((ySumData*ySumData)/result->count);

    if (0.0 != Sxx)
	result->m = Sxy / Sxx;
    else
	result->m = 0.0;

    result->b = (ySumData/result->count) -
	(result->m * (xSumData/result->count));

    yVariance = Syy / (double)(result->count-1);
    xVariance = Sxx / (double)(result->count-1);
    yStdDev = xStdDev = 0.0;
    if (yVariance > 0.0) yStdDev = sqrt(yVariance);
    if (xVariance > 0.0) xStdDev = sqrt(xVariance);
    if ((yStdDev * xStdDev) != 0.0)
	result->r = (Sxy / (result->count - 1)) / (yStdDev * xStdDev);
    else
	result->r = 1.0;
    }

calcMeanMinMax(result);
return result;
}	/*	struct dataVector *runRegression()	*/

static void residualStats(struct dataVector *residuals)
{
hPrintf("<P><!--outer table is for border purposes-->\n");
hPrintf("<TABLE BGCOLOR=\"#%s",HG_COL_BORDER);
hPrintf("\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");

hPrintf("<TABLE BGCOLOR=\"%s\" BORDER=1>", HG_COL_INSIDE);
hPrintf("<TR><TH COLSPAN=3>residuals</TH></TR>\n");
hPrintf("<TR><TH>Minimum</TH><TH>Maximum</TH><TH>Mean</TH></TR>\n");
hPrintf("<TR><TD ALIGN=RIGHT>%.4g</TD>", residuals->min);
hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", residuals->max);
hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", residuals->sumData / residuals->count);
hPrintf("</TR></TABLE>\n");

hPrintf("</TD></TR></TABLE</P>\n");
}

static void showPlots(struct trackTable *table1, struct trackTable *table2,
	struct dataVector *result)
{
struct tempName *scatterPlotGif = NULL;
struct tempName *residualPlotGif = NULL;
struct tempName *histogram1PlotGif = NULL;
struct tempName *histogram2PlotGif = NULL;
double F_statistic = 0.0;
double fitMin, fitMax;
int totalWidthScatter, totalHeightScatter;
int totalWidthResidual, totalHeightResidual;
int totalWidthHisto1, totalHeightHisto1;
int totalWidthHisto2, totalHeightHisto2;

scatterPlotGif = scatterPlot(table1, table2, result, &totalWidthScatter,
	&totalHeightScatter);
residualPlotGif = residualPlot(table1, table2, result, &F_statistic,
	&fitMin, &fitMax, &totalWidthResidual, &totalHeightResidual);
histogram1PlotGif=histogramPlot(table1, &totalWidthHisto1, &totalHeightHisto1);
histogram2PlotGif=histogramPlot(table2, &totalWidthHisto2, &totalHeightHisto2);

hPrintf("<TABLE>");
hPrintf("<TR><TD>\n");

hPrintf("<TABLE BGCOLOR=\"%s\">", HG_COL_INSIDE);
hPrintf("<TR><TH COLSPAN=2>scatter&nbsp;plot,&nbsp;r<sup>2</sup>&nbsp;%.4g</TH></TR>\n", result->r*result->r);
hPrintf("<TR>");
hPrintf("<TD><IMG SRC=\"%s\" WIDTH=%d HEIGHT=%d</TD></TR>\n",
	scatterPlotGif->forHtml, totalWidthScatter, totalHeightScatter);
hPrintf("</TABLE></TD><TD BGCOLOR=\"%s\">\n", HG_COL_INSIDE);

hPrintf("<TABLE BGCOLOR=\"%s\">", HG_COL_INSIDE);
hPrintf("<TR><TH COLSPAN=2>Residuals&nbsp;vs.&nbsp;Fitted,&nbsp;F&nbsp;statistic:&nbsp;%.4g</TH></TR>\n", F_statistic);
hPrintf("<TR>");
hPrintf("<TD><IMG SRC=\"%s\" WIDTH=%d HEIGHT=%d</TD></TR>\n",
	residualPlotGif->forHtml, totalWidthResidual, totalHeightResidual);
hPrintf("</TABLE>\n");
hPrintf("</TD></TR></TABLE>\n");

if (1 == 0)	/*	not really useful	*/
    residualStats(result);

hPrintf("<TABLE>");
hPrintf("<TR><TD>\n");

if ((histogram1PlotGif != NULL) && (histogram2PlotGif != NULL))
    {
    hPrintf("<TABLE BGCOLOR=\"%s\">", HG_COL_INSIDE);
    hPrintf("<TR><TH COLSPAN=2>histogram<BR>%s</TH></TR>\n", table1->shortLabel);
    hPrintf("<TR>");
    hPrintf("<TD><IMG SRC=\"%s\" WIDTH=%d HEIGHT=%d</TD></TR>\n",
	    histogram1PlotGif->forHtml, totalWidthHisto1, totalHeightHisto1);
    hPrintf("</TABLE></TD><TD BGCOLOR=\"%s\">\n", HG_COL_INSIDE);

    hPrintf("<TABLE BGCOLOR=\"%s\">", HG_COL_INSIDE);
    hPrintf("<TR><TH COLSPAN=2>histogram<BR>%s</TH></TR>\n", table2->shortLabel);
    hPrintf("<TR>");
    hPrintf("<TD><IMG SRC=\"%s\" WIDTH=%d HEIGHT=%d</TD></TR>\n",
	    histogram2PlotGif->forHtml, totalWidthHisto2, totalHeightHisto2);
    hPrintf("</TABLE>\n");
    hPrintf("</TD></TR></TABLE>\n");
    }

}

static void tableInfoDebugDisplay(struct trackTable *tableList)
{
/*	some debugging stuff, do we have the tables correctly identified */
    {
    struct trackTable *table;

    hPrintf("<P><!--outer table is for border purposes-->\n");
    hPrintf("<TABLE BGCOLOR=\"#%s",HG_COL_BORDER);
    hPrintf("\" BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"1\"><TR><TD>\n");

    hPrintf("<TABLE BGCOLOR=\"%s\" BORDER=1><TR><TH COLSPAN=9>debugging information</TH></TR><TR><TH>Track</TH><TH>Track<BR>(perhaps composite)</TH><TH>table</TH><TH>type</TH><TH>type</TH><TH>bedGraph<BR>column</TH><TH>bedGraph<BR>column</TH><TH>isCustom</TH><TH>bedColumns</TH></TR>\n", HG_COL_INSIDE);

    for (table = tableList; table != NULL; table = table->next)
	{
	hPrintf("<TR><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%d</TD><TD>%s</TD><TD>%s</TD><TD>%d</TD></TR>\n",
	    table->shortLabel, table->tdb->tableName, table->tableName,
	    table->actualTdb->type,
	    table->isBedGraph ? "bedGraph" : table->isWig ? "wiggle" : "other",
	    table->bedGraphColumnNum, table->bedGraphColumnName,
	    table->isCustom ? "YES" : "NO", table->bedColumns);
	}
    hPrintf("</TABLE>\n");
    hPrintf("</TD></TR></TABLE</P>\n");
    }
}

void doCorrelateMore(struct sqlConnection *conn)
/* Continue working in correlate page. */
{
struct trackDb *tdb2;
char *tableName2;
char *onChange = onChangeEither();
/*char *op;*/
char *table2onEntry;
char *table2;
boolean correlateOK1 = FALSE;
char *maxLimitCountStr = NULL;
int maxLimitCount = 40000000;
char *windowingSizeStr = NULL;
int windowingSize = 1;
char *tmpString;
struct trackTable *tableList;
long startTime, endTime;

startTime = clock1000();

tableList = NULL;	/*	initialize the list	*/


/*	We are going to be TRUE here since this selection was limited on
 *	the mainPage to only these types of tables.
 */
correlateOK1 = correlateTrackOK(curTrack);

table2onEntry = cartUsualString(cart, hgtaNextCorrelateTable, "none");

/*	add first table to the list	*/
tableList = allocTrackTable();
tableList->tdb = curTrack;
tableList->tableName = cloneString(curTable);
fillInTrackTable(tableList, conn);

if (differentWord(table2onEntry,"none") && strlen(table2onEntry))
    {
    htmlOpen("Correlate table '%s' (%s) with table '%s'", tableList->shortLabel, tableList->tableName, table2onEntry);
    }
else
    htmlOpen("Correlate table '%s' (%s)", tableList->shortLabel, tableList->tableName);

hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=GET>\n");
cartSaveSession(cart);

/* Print group, track, and table selection menus */
tdb2 = showGroupTrackRowLimited(hgtaNextCorrelateGroup, onChange, 
    hgtaNextCorrelateTrack, onChange, hgtaNextCorrelateTable, &table2);

tableName2 = tdb2->shortLabel;

maxLimitCountStr = cartUsualString(cart, hgtaCorrelateMaxLimitCount,
	maxResultsMenu[1]);
tmpString = cloneString(maxLimitCountStr);
stripChar(tmpString, ',');
maxLimitCount = sqlUnsigned(tmpString);
freeMem(tmpString);
windowingSizeStr = cartUsualString(cart, hgtaCorrelateWinSize, "1");
tmpString = cloneString(windowingSizeStr);
stripChar(tmpString, ',');
windowingSize = sqlUnsigned(tmpString);
freeMem(tmpString);

/*	limit total data points and window size	options */
hPrintf("<TABLE BORDER=0><TR><TD>Limit total data points in result:&nbsp\n");
cgiMakeDropList(hgtaCorrelateMaxLimitCount, maxResultsMenu, maxResultsMenuSize,
    maxLimitCountStr);
hPrintf("</TD><TD>&nbsp;&nbsp;Window data to:&nbsp;\n");
cgiMakeTextVar(hgtaCorrelateWinSize, cartUsualString(cart, hgtaCorrelateWinSize, "1"), 8);
hPrintf("&nbsp;bases</TD></TR></TABLE>\n");

#ifdef NOT
hPrintf("<P>\n");

op = cartUsualString(cart, hgtaNextCorrelateOp, "any");
jsTrackingVar("op", op);
makeOpButton("any", op);
printf("All %s records that have any overlap with %s <BR>\n",
       name, tableName2);
makeOpButton("none", op);
printf("All %s records that have no overlap with %s <BR>\n",
       name, tableName2);

/*	keep javaScript onClick happy	*/
hPrintf(" <P>\n");

/*	keep javaScript onClick happy	*/
jsTrackingVar("op", op);
#endif

hPrintf("<P>\n");

cgiMakeButton(hgtaDoCorrelateSubmit, "calculate");
hPrintf("&nbsp;");
cgiMakeButton(hgtaDoClearContinueCorrelate, "clear selections");
hPrintf("&nbsp;");
cgiMakeButton(hgtaDoMainPage, "return to table browser");
hPrintf("</FORM>\n");

hPrintf("<P>\n");

/*	If we had a second table coming into this screen, do the
 *	calculations
 */
if (differentWord(table2onEntry,"none") && strlen(table2onEntry))
    {
    if (correlateOK1)
	{
	struct trackTable *tt;
	int totalBases = 0;
	/*	add second table to the list	*/
	tt = allocTrackTable();
	tt->tdb = tdb2;
	tt->tableName = cloneString(table2);
	fillInTrackTable(tt, conn);
	slAddTail(&tableList,tt);

	/*	collected data ends up attached to each table in the
 	 *	table List in a dataVector list
	 */
	totalBases = collectData(conn, tableList, maxLimitCount);
	if (windowingSize > 1)
		totalBases = windowData(tableList, windowingSize);

	if (totalBases > 0)
	    {
	    struct trackTable *table1 = tableList;
	    struct trackTable *table2 = tableList->next;
	    struct dataVector *residuals;

	    residuals = runRegression(tableList, totalBases);
	    overallCounts(table1);
	    overallCounts(table2);
	    showThreeVectors(table1, table2, residuals);
	    showPlots(table1, table2, residuals);
	    freeDataVector(&residuals);
	    }
	else
	    hPrintf("<P>no intersection between the two tables "
		"in this region</P>\n");

if (1 == 0)
    tableInfoDebugDisplay(tableList);	/*	dbg	*/

	freeTrackTableList(&tableList);
	endTime = clock1000();

	hPrintf("<P>total elapsed time for this calculation:");
	hPrintf(" %.3f seconds.</P>\n", 0.001*(endTime-startTime));
	}
    }


hPrintf("%s", corrHelpText);

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[32];
    int varCount = ArraySize(nextVars);
    memcpy(saveVars, nextVars, varCount * sizeof(saveVars[0]));
    saveVars[varCount] = hgtaDoCorrelateMore;
    jsCreateHiddenForm(cart, getScriptName(), saveVars, varCount+1);
    }

htmlClose();
}

static void removeCartVars(struct cart *cart, char **vars, int varCount)
/* Remove array of variables from cart. */
{
int i;
for (i=0; i<varCount; ++i)
    cartRemove(cart, vars[i]);
}

static void copyCartVars(struct cart *cart, char **source, char **dest, int count)
/* Copy from source to dest. */
{
int i;
for (i=0; i<count; ++i)
    {
    char *s = cartOptionalString(cart, source[i]);
    if (s != NULL)
        cartSetString(cart, dest[i], s);
    else
        cartRemove(cart, dest[i]);
    }
}


void doCorrelatePage(struct sqlConnection *conn)
/* Respond to correlate create/edit button */
{
if (ArraySize(curVars) != ArraySize(nextVars))
    internalErr();
copyCartVars(cart, curVars, nextVars, ArraySize(curVars));
doCorrelateMore(conn);
}

void doClearContinueCorrelate(struct sqlConnection *conn)
/* Respond to click on clear selections from correlation calculate page. */
{
removeCartVars(cart, curVars, ArraySize(curVars));
copyCartVars(cart, curVars, nextVars, ArraySize(curVars));
doCorrelateMore(conn);
}

void doClearCorrelate(struct sqlConnection *conn)
/* Respond to click on clear correlation from main hgTable page. */
{
removeCartVars(cart, curVars, ArraySize(curVars));
doMainPage(conn);
}

void doCorrelateSubmit(struct sqlConnection *conn)
/* Respond to submit on correlate page. */
{
copyCartVars(cart, nextVars, curVars, ArraySize(curVars));
doCorrelatePage(conn);
}
