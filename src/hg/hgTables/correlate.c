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
#include "hgTables.h"
#include "correlate.h"	/*	to fetch corrHelpText	*/

static char const rcsid[] = "$Id: correlate.c,v 1.20 2005/06/25 00:23:01 hiram Exp $";

static char *maxResultsMenu[] =
{
    "40,000,000",
    "60,000,000",
};
static int maxResultsMenuSize = ArraySize(maxResultsMenu);

/*	Each track data's values will be copied into one of these structures,
 *	This is also the form that a result vector will take.
 *	There can be a linked list of these for multiple regions.
 *	The positions are in order, there are no gaps, count is the
 *	length of the position and value arrays.
 */
struct dataVector
    {
    struct dataVector *next;	/* linked list for multiple regions */
    char *chrom;		/* Chromosome. */
    int start;			/* Zero-based. */
    int end;			/* Non-inclusive. */
    char *name;		/*	potentially a region name (for encode) */
    int count;		/*	number of data values	*/
    int maxCount;	/*	no more than this number of data values	*/
    int *position;	/*	array of chrom positions, 0-relative	*/
    float *value;	/*	array of data values at these positions	*/
    double min;		/*	minimum data value in the set	*/
    double max;		/*	maximum data value in the set	*/
    double sumData;	/*	sum of all data here, sum(Xi)	*/
    double sumSquares;	/*	sum of squares of all data here, sum(Xi*Xi) */
    double sumProduct;	/*	accumulates sum(Xi * Yi) here */
    double r;		/*	correlation coefficient	*/
    double a;		/*	the a in: y = ax + b [linear regression line] */
    double b;		/*	the b in: y = ax + b [linear regression line] */
    long fetchTime;	/*	msec	*/
    long calcTime;	/*	msec	*/
    };

static struct dataVector *allocDataVector(char *chrom, int bases)
/*	allocate a dataVector for given number of bases	on specified chrom */
{
struct dataVector *v;
AllocVar(v);
v->chrom = cloneString(chrom);
v->start = 0;
v->end = 0;
v->name = NULL;
v->position = needHugeMem((size_t)(sizeof(int) * bases));
v->value = needHugeMem((size_t)(sizeof(float) * bases));
v->count = 0;		/*	nothing here yet	*/
v->maxCount = bases;	/*	do not run past this limit	*/
v->min = INFINITY;	/*	must be less than this	*/
v->max = -INFINITY;	/*	must be greater than this */
v->sumData = 0.0;	/*	starting sum is zero	*/
v->sumSquares = 0.0;	/*	starting sum is zero	*/
v->sumProduct = 0.0;	/*	starting sum is zero	*/
v->r = 0.0;		/*	an initial r is zero	*/
v->fetchTime = 0;
v->calcTime = 0;
return v;
}

static void freeDataVector(struct dataVector **v)
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

/*	there can be a list of these for N number of tables to work with */
/*	the first case will be two only, later improvements may do N tables */
struct trackTable
    {
    struct trackTable *next;
    struct trackDb *tdb;	/*	may be wigMaf primary	*/
    char *tableName;	/* the actual name, without composite confusion */
    char *shortLabel;
    char *longLabel;
    boolean isBedGraph;		/* type of data in table	*/
    boolean isWig;		/* type of data in table	*/
    boolean isCustom;		/* might be a custom track	*/
    int bedGraphColumnNum;	/* the column where the dataValue is */
    char *bedGraphColumnName;	/* the name of the bedGraph column */
    struct trackDb *actualTdb;	/* the actual tdb, without composite/wigMaf confusion */
    char *actualTable;	/* without wigMaf confusion */
    struct dataVector *vSet;	/* the data for this table, all regions */
    };

static struct trackTable *allocTrackTable()
/*	allocate a trackTable item */
{
struct trackTable *t;
AllocVar(t);
t->next = NULL;
t->tdb = NULL;
t->tableName = NULL;
t->shortLabel = NULL;
t->longLabel = NULL;
t->isBedGraph = FALSE;
t->isWig = FALSE;
t->isCustom = FALSE;
t->bedGraphColumnNum = 0;
t->bedGraphColumnName = NULL;
t->actualTdb = NULL;
t->actualTable = NULL;
t->vSet = NULL;
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

#ifdef NOT
/*	this is the end result of all the data collection and analysis */
struct correlationResult
    {
    struct trackTable *table1;
    struct trackTable *table2;
    struct dataVector *residual;
    };

struct correlationResult *allocCorrelationResult(struct trackTable *t1,
        struct trackTable *t2, int totalBases, char *chrom)
{
struct correlationResult *cr;
AllocVar(cr);
cr->table1 = t1;
cr->table2 = t2;
cr->residual = allocDataVector(chrom, totalBases);
return cr;
}

static void freeCorrelationResult(struct correlationResult **cr)
/*	free up correlationResult structure	*/
{
if (cr)
    {
    struct correlationResult *cResult;
    cResult=*cr;
    freeTrackTable(&cResult->table1);
    freeTrackTable(&cResult->table2);
    freeDataVector(&cResult->residual);
    freez(cr);
    }
}
#endif

/* We keep two copies of variables, so that we can
 * cancel out of the page. */

static char *curVars[] = {hgtaCorrelateGroup, hgtaCorrelateTrack,
	hgtaCorrelateTable2, hgtaCorrelateOp,
	};
static char *nextVars[] = {hgtaNextCorrelateGroup, hgtaNextCorrelateTrack,
	hgtaNextCorrelateTable2, hgtaNextCorrelateOp,
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
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
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

static boolean validTable(struct trackDb *tdb)
{
if (tdb && (startsWith("bedGraph", tdb->type) || startsWith("wig ",tdb->type)))
    return TRUE;
else
    return FALSE;
}

static struct trackDb *getLimitedTrackList(struct grp **grpList)
/*	create list of tracks limited to specific types, and return the
 *	groups of those tracks	*/
{
struct trackDb *trackList = NULL, *tdb, *next = NULL;
struct hash *groupsInTrackList = newHash(0);
struct grp *groupsAll, *groupList = NULL, *group;

fullTrackList = getFullTrackList();
groupsAll = hLoadGrps();

for (tdb = fullTrackList; tdb != NULL; tdb = next)
    {
    next = tdb->next;
    if (validTable(tdb))
	{
	if (!hashLookup(groupsInTrackList,tdb->grp))
	    {
	    hashAdd(groupsInTrackList, tdb->grp, NULL);
	    }
	slAddHead(&trackList, tdb);
	}
    }
for (group = slPopHead(&groupsAll);group != NULL;group = slPopHead(&groupsAll))
    {
    if (hashLookup(groupsInTrackList, group->name))
	{
	slAddTail(&groupList, group);
	}
    else
        grpFree(&group);
    }
hashFree(&groupsInTrackList);
if (grpList)
    *grpList = groupList;
slReverse(&trackList);
return(trackList);
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

static struct trackDb *findTdb(struct trackDb *track, char *table)
/*	if the given track is a composite, find the tdb for the table */
{
struct trackDb *tdb = track;

if (track && trackDbIsComposite(track))
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

static void fillInTrackTable(struct trackTable *table,
	struct sqlConnection *conn)
/*	determine real tdb, and attach labels, only tdb and tableName
 *	have been set so far, these may be composites, wigMafs or
 *	something like that where the actual tdb and table is something
 *	else.	*/
{
struct trackDb *tdb;

tdb = findTdb(table->tdb,table->tableName);

table->shortLabel = cloneString(tdb->shortLabel);
table->longLabel = cloneString(tdb->longLabel);
table->actualTdb = tdb;
table->actualTable = cloneString(tdb->tableName);
table->isBedGraph = startsWith("bedGraph", tdb->type);
table->isWig = FALSE;
if (startsWith("wig",tdb->type))
    {
    if (startsWith("wigMaf",tdb->type))
	{
	char *wigTable = trackDbSetting(tdb, "wiggle");
	freeMem(table->actualTable);
	table->actualTable = cloneString(wigTable);
	}
    table->isWig = TRUE;
    }
table->isCustom = isCustomTrack(table->actualTable);

if (table->isBedGraph && (!table->isCustom))
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
    }
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
    struct sqlConnection *conn, boolean allTablesOk, struct grp *groupList)
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
    nameList = tablesForTrack(track);

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
    struct trackDb *tdb = findTdb(track, name->name);
    if (validTable(tdb))
	{
	hPrintf("<OPTION VALUE=%s", name->name);
	if (sameString(selTable, name->name))
	    hPrintf(" SELECTED");
	hPrintf(">%s\n", tdb->shortLabel);
	}
    }
hPrintf("</SELECT>\n");
return selTable;
}

static struct trackDb *showGroupTrackRowLimited(char *groupVar,
    char *groupScript, char *trackVar, char *trackScript,
	struct sqlConnection *conn, char *tableVar, char **table2Return)
/* Show group & track row of controls, limited by track type.
    Returns selected track */
{
struct trackDb *track, *trackList;
struct grp *selGroup, *groupList;
char *table2;

trackList = getLimitedTrackList(&groupList);

hPrintf("Select a group, track and table to correlate with:\n");

hPrintf("<TABLE BORDER=0>\n");
hPrintf("<TR><TD>");

selGroup = showGroupFieldLimited(groupVar, groupScript, conn, FALSE, groupList);
nbSpaces(3);
track = showTrackFieldLimited(selGroup, trackVar, trackScript, trackList);

hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
table2 = showTable2FieldLimited(track, tableVar);
hPrintf("</TD></TR>\n");

if (table2Return)
    *table2Return = table2;

hPrintf("</TABLE>\n");

return track;
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

static struct dataVector *fetchOneRegion(struct trackTable *table,
	struct region *region, struct sqlConnection *conn)
/*	fetch all the data for this track and table in the given region */
{
struct dataVector *vector;
int regionSize = region->end - region->start;
long startTime, endTime;

if (! (table->isWig || table->isBedGraph))
    return NULL;

/*	makes no sense to work on less than two numbers	*/
if (regionSize < 2)
    return NULL;



startTime = clock1000();
/*	assume entire region is going to produce numbers	*/
vector = allocDataVector(region->chrom, regionSize);

if (table->isBedGraph && !table->isCustom)
    {
    int vIndex = vector->count;	/* it is starting at zero	*/
    int rowOffset;
    char **row;
    struct sqlResult *sr = NULL;
    char fields[256];
    char *filter = NULL;

    filter = filterClause(database, table->actualTable, region->chrom);

    safef(fields,ArraySize(fields), "chromStart,chromEnd,%s",
	    table->bedGraphColumnName);

//hPrintf("<P>select: %s %s:%d-%d</P>\n", table->actualTable, region->chrom, region->start, region->end );

    /*	the TRUE indicates to order by start position, this is necessary
     *	so the walk-through positions will work properly
     */
    sr = hExtendedRangeQuery(conn, table->actualTable, region->chrom,
	region->start, region->end, filter, TRUE, fields, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	float value = sqlFloat(row[2]);
	int cStart = sqlSigned(row[0]);
	int cEnd = sqlSigned(row[1]);
	int i;
	register int bases = 0;
	/*	make sure we stay within the region specified	*/
	if (cEnd < region->start || cStart > region->end)
	    continue;
	if (cStart < region->start)
	    cStart = region->start;
	if (cEnd > region->end)
	    cEnd = region->end;
	/* watch out for overlapping bed items, get the first one started */
	if (0 == bases)
	    {
	    vector->value[vIndex] = value;
	    vector->position[vIndex] = cStart;
	    ++bases;
	    ++vIndex;
	    }
	for (i = cStart+1;
		(i < cEnd) && (vIndex < vector->maxCount); ++i)
	    {
	    /*	ensure next position is past previous	*/
	    if (i > vector->position[vIndex-1])
		{
		vector->value[vIndex] = value;
		vector->position[vIndex] = i;
		++bases;
		++vIndex;
		}
	    }
	    vector->count += bases;
	}
    vector->start = vector->position[0];
    vector->end = vector->position[(vector->count)-1] + 1; /* non-inclusive */
    sqlFreeResult(&sr);
    }
else if (table->isWig)
    {
    int span = 1;
    struct wigAsciiData *wigData = NULL;
    struct wigAsciiData *asciiData;
    struct wigAsciiData *next;
    int vIndex = vector->count;	/* it is starting at zero	*/
    register int bases = 0;

    /*	we still do not have a proper minSpan finder for custom tracks */
    /*	they should have a spanList setup during their encoding, then it
     *	would always be there.
     */
    if (table->isCustom)
	span = 1;
    else
	span = minSpan(conn, table->actualTable, region->chrom,
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
    vector->end = vector->position[(vector->count)-1] + 1; /* non-inclusive */
    }

endTime = clock1000();

vector->fetchTime += endTime - startTime;

if (vector->count > 0)
    return vector;
else
    freeDataVector(&vector);
return NULL;
}	/*	static struct dataVector *fetchOneRegion()	*/

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
if ((v1->maxCount - v1Index) > 1024000)
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
    int chromStart, int chromEnd, int bases, double min, double max,
	double sumData, double sumSquares, double r, long fetchMs, long calcMs,
		char *table1, char *table2, double a, double b)
/*	display a single line of statistics	*/
{
double mean = 0.0;
double variance = 0.0;
double stddev = 0.0;

if (bases > 0)
    {
    mean = sumData/bases;
    if (bases > 1)
	{
	variance = (sumSquares - ((sumData*sumData)/bases)) /
		(double)(bases-1);
	if (variance > 0.0)
	    stddev = sqrt(variance);
	}
    }

hPrintf("<TR>");
/*	Check for first or second data rows, 2nd has 0,0 coordinates.
 *	Summary row has 1,1 start,end	*/
if (chromStart || chromEnd)
    {
    char posBuf[64];
    safef(posBuf, ArraySize(posBuf), "%s:%d-%d", chrom, chromStart+1, chromEnd);
    hPrintf("<TD ALIGN=LEFT ROWSPAN=2>");
    /* Construct browser anchor URL with tracks we're looking at open. */
    hPrintf("<A HREF=\"%s?%s", hgTracksName(), cartSidUrlString(cart));
    hPrintf("&db=%s", database);
    hPrintf("&position=%s", posBuf);
    hPrintf("&%s=%s", table1, hTrackOpenVis(table1));
    if (table2 != NULL)
	hPrintf("&%s=%s", table2, hTrackOpenVis(table2));
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
    hPrintf("<BR>\n");
    printLongWithCommas(stdout,(long)bases);
    hPrintf("&nbsp;bases</TD>");
    }

if (chromStart || chromEnd)
    {
    hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", r);
    hPrintf("<TD ALIGN=RIGHT>%.4g</TD>", r*r);
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
hPrintf("<TR><TH>Position<BR>#&nbsp;of&nbsp;bases</TH>");
hPrintf("<TH>Correlation<BR>coefficient<BR>r</TH>");
hPrintf("<TH>r<BR>squared</TH>");
hPrintf("<TH>Track</TH>");
hPrintf("<TH>Minimum</TH><TH>Maximum</TH><TH>Mean</TH><TH>Variance</TH>");
hPrintf("<TH>Standard<BR>deviation</TH>");
hPrintf("<TH COLSPAN=2>");
hPrintf("<TABLE BGCOLOR=\"%s\" BORDER=0 WIDTH=100%>", HG_COL_INSIDE);
hPrintf("<TR WIDTH=100%><TH COLSPAN=2>Regression<BR>line<BR>");
hPrintf("y&nbsp;=&nbsp;a*x&nbsp;+&nbsp;b</TH></TR>");
hPrintf("<TR><TH>a</TH><TH>b</TH></TR></TABLE>");
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
    totalBases1 += v1->end - v1->start;
    totalBases2 += v2->end - v2->start;
    totalFetch1 += v1->fetchTime;
    totalFetch2 += v2->fetchTime;
    totalCalc1 += v1->calcTime;
    totalCalc2 += v2->calcTime;
    totalSum1 += v1->sumData;
    totalSum2 += v2->sumData;
    totalSumSquares1 += v1->sumSquares;
    totalSumSquares2 += v2->sumSquares;
    }

if (0 == rowsOutput)
    hPrintf("<TR><TD COLSPAN=11>EMPTY RESULT SET</TD></TR>\n");
else if (rowsOutput > 1)
    {
    statsRowOut("OVERALL", NULL, table1->shortLabel, 1, 1,
	totalBases1, min1, max1, totalSum1, totalSumSquares1,
	    result->r, totalFetch1, totalCalc1, table1->actualTable,
		table2->actualTable, result->a, result->b);
    statsRowOut("OVERALL", NULL, table2->shortLabel, 0, 0,
	totalBases2, min1, max1, totalSum2, totalSumSquares2,
	    result->r, totalFetch2, totalCalc2, table1->actualTable,
		table2->actualTable, 0.0, 0.0);
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
	    v1->sumSquares, v2->r, v1->fetchTime,
		v1->calcTime, table1->actualTable, table2->actualTable,
		    v1->a, v1->b);
    statsRowOut(v2->chrom, v1->name, table2->shortLabel, 0, 0,
	v2->count, v2->min, v2->max, v2->sumData,
	    v2->sumSquares, v2->r, v2->fetchTime,
		v2->calcTime, table1->actualTable, table2->actualTable,
		    0.0, 0.0);
    }

if (rowsOutput > 1)
    {
    hPrintf("<TR><TD COLSPAN=11><HR></TD></TR>\n");
    statsRowOut("OVERALL", NULL, table1->shortLabel, 1, 1,
	totalBases1, min1, max1, totalSum1, totalSumSquares1,
	    result->r, totalFetch1, totalCalc1, table1->actualTable,
		table2->actualTable, result->a, result->b);
    statsRowOut("OVERALL", NULL, table2->shortLabel, 0, 0,
	totalBases2, min1, max1, totalSum2, totalSumSquares2,
	    result->r, totalFetch2, totalCalc2, table1->actualTable,
		table2->actualTable, 0.0, 0.0);

    tableLabels();
    }

hPrintf("</TABLE>\n");
/*	and end the special container table	*/
hPrintf("</TD></TR></TABLE></P>\n");

/*	debugging when 1 region, less than 100 bases, show all values	*/
if ((1 == rowsOutput) && (totalBases1 < 100) && (totalBases2 < 100))
    {
    int start, end, i;
    int v1Index = 0;
    int v2Index = 0;
    v1 = table1->vSet;
    v2 = table2->vSet;
    start = min(v1->start,v2->start);
    end = max(v1->end,v2->end);
    hPrintf("<P><TABLE BORDER=1><TR><TH>position</TH><TH>track 1</TH><TH>track 2</TH></TR>\n");
    for (i = start; i < end; ++i)
	{
hPrintf("<TR><TD ALIGN=RIGHT>%d</TD>", i+1);
	if (i >= v1->start)
	    {
	    if (i == v1->position[v1Index])
		{
		hPrintf("<TD ALIGN=RIGHT>%g</TD>", v1->value[v1Index]);
		++v1Index;
		}
	    else if (i > v1->position[v1Index])
		{
		hPrintf("<TD ALIGN=RIGHT>N/A</TD>");
		++v1Index;
		}
	    }
	else
	    hPrintf("<TD ALIGN=RIGHT>N/A</TD>");
	if (i >= v2->start)
	    {
	    if (i == v2->position[v2Index])
		{
		hPrintf("<TD ALIGN=RIGHT>%g</TD>", v2->value[v2Index]);
		++v2Index;
		}
	    else if (i > v2->position[v2Index])
		{
		hPrintf("<TD ALIGN=RIGHT>N/A</TD>");
		++v2Index;
		}
	    }
	else
	    hPrintf("<TD ALIGN=RIGHT>N/A</TD>");
	}
    hPrintf("</TABLE></P>\n");
    }
}

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

vSet1 = NULL;	/*	initialize linked list	*/
vSet2 = NULL;	/*	initialize linked list	*/

table1 = tableList;
table2 = tableList->next;

/*	count the regions, just to get an idea of where we may be going */
for (region = regionList; region != NULL; region = region->next)
    ++regionCount;

/*	this display is debugging, just showing the region(s)	*/
if (1 == regionCount)
    {
    long bases = 0;
    region = regionList;
    bases = region->end - region->start;
    hPrintf("<P>position: %s:", region->chrom);
    printLongWithCommas(stdout,(long)(region->start+1)); /* 1-relative */
    hPrintf("-");
    printLongWithCommas(stdout,(long)region->end);
    hPrintf("&nbsp;bases:&nbsp;");
    printLongWithCommas(stdout,bases);
    hPrintf("</P>\n");
    }
else if (sameString(regionType,"genome"))
    {
    hPrintf("<P>position: full genome, %d chromosomes</P>\n", regionCount);
    }
else if (sameString(regionType,"encode"))
    hPrintf("<P>position: %d encode regions</P>\n", regionCount);

if ( (table1->isCustom && checkWigDataFilter("ct", curTable, NULL, NULL, NULL))
	|| checkWigDataFilter(database, curTable, NULL, NULL, NULL))
    {
    hPrintf("<P>data filter on %s:", table1->shortLabel);
    wigShowFilter(conn);
    hPrintf("</P>\n");
    }

#ifdef NOT
/*	some debugging stuff, do we have the tables correctly identified */
    {
    struct trackTable *table;
    hPrintf("<P><TABLE BORDER=1><TR><TH COLSPAN=8>debugging information</TH></TR><TR><TH>Track</TH><TH>Track<BR>(perhaps composite)</TH><TH>table</TH><TH>type</TH><TH>type</TH><TH>bedGraph<BR>column</TH><TH>bedGraph<BR>column</TH><TH>isCustom</TH></TR>\n");

    for (table = tableList; table != NULL; table = table->next)
	{
	hPrintf("<TR><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%d</TD><TD>%s</TD><TD>%s</TD></TR>\n",
	    table->shortLabel, table->tdb->tableName, table->tableName,
	    table->actualTdb->type,
	    table->isBedGraph ? "bedGraph" : table->isWig ? "wiggle" : "other",
	    table->bedGraphColumnNum, table->bedGraphColumnName,
	    table->isCustom ? "YES" : "NO");
	}
    hPrintf("</TABLE></P>\n");
    }
#endif

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

    if (bases > 64000000)
	{
	hPrintf("<P><B>warning:</B> This region: %s:%d-%d is too large",
		region->chrom, region->start, region->end);
	hPrintf(" to process.  This function can currenly only work on\n");
	hPrintf("regions of less than 64,000,000 bases. Perhaps future\n");
	hPrintf("improvements will raise this limit.\n");
	errAbort("");
	}
    v1 = fetchOneRegion(table1, region, conn);
    if (v1)	/*	if data there, fetch the second one	*/
	{
	v2 = fetchOneRegion(table2, region, conn);
	if (v2)	/*	if both data, construct a result set	*/
	    {
	    intersectVectors(v1, v2);
	    if (v1->count < 1)	/*	possibly no intersection	*/
		{
		freeDataVector(&v1);	/* no data in second	*/
		freeDataVector(&v2);	/* no data in second	*/
		continue;		/* next region	*/
		}
	    if ((totalBases + v1->count) >= maxLimitCount)
		break;
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

/*	returning results, the collected data	*/
table1->vSet = vSet1;
table2->vSet = vSet2;
return totalBases;
}	/*	static void collectData()	*/

static struct dataVector *runRegression(struct trackTable *tableList,
    int totalBases)
{
struct trackTable *table1, *table2;
struct dataVector *result = NULL;
struct dataVector *v1;
struct dataVector *v2;
double totalSumProduct = 0.0;
double vector1sumData = 0.0;
double vector2sumData = 0.0;
double vector1sumSquares = 0.0;
double vector2sumSquares = 0.0;
double variance1 = 0.0;
double variance2 = 0.0;
double stddev1 = 0.0;
double stddev2 = 0.0;
long startTime, endTime;

/*	expecting two tables	*/
if (NULL == tableList)
    return result;

table1 = tableList;
table2 = tableList->next;
if (NULL == table2)
    return result;

/*	find the mean, min and max	*/
for (v1 = table1->vSet; v1 != NULL; v1 = v1->next)
    calcMeanMinMax(v1);
for (v2 = table2->vSet; v2 != NULL; v2 = v2->next)
    calcMeanMinMax(v2);

result = allocDataVector("result", totalBases);
/*	calculating formula for correlation coefficient r
 *	compute the product of the two variables
 *	and keep a running sum of that product for the overall answer
 *	later
 */
/*	This loop walks through all the regions in the data vectors.
 *	Each region will have its own r calculated
 */
for (v1 = table1->vSet, v2 = table2->vSet;
	(v1 != NULL) && (v2 != NULL);
		v1 = v1->next, v2 = v2->next)
    {
    double sumProduct = 0.0;		/*	sum of products	*/
    double numeratorCoVariance = 0.0;
    double numeratorVariance = 0.0;

    startTime = clock1000();

    if ((v1->count != v2->count) || (0 == v1->count))
     errAbort("unequal sized, or zero length data vectors given to regression");

    sumProduct = v1->sumProduct;
    result->count += v1->count;
    totalSumProduct += sumProduct;
    vector1sumData += v1->sumData;
    vector2sumData += v2->sumData;
    vector1sumSquares += v1->sumSquares;
    vector2sumSquares += v2->sumSquares;
    result->fetchTime += v1->fetchTime;
    result->fetchTime += v2->fetchTime;
    result->calcTime += v1->calcTime;
    result->calcTime += v2->calcTime;
    /* Sxy - page 101, page 488, Mendenhall, Beaver, Beaver - 2003 */
    numeratorCoVariance = sumProduct - ((v1->sumData*v2->sumData)/v1->count);
    /* Sxx - page 61, page 488, Mendenhall, Beaver, Beaver - 2003 */
    numeratorVariance = v1->sumSquares - ((v1->sumData*v1->sumData)/v1->count);

    /* b = Sxy / Sxx	page 489 Mendenhall, Beaver, Beaver - 2003 */
    v1->b = numeratorCoVariance / numeratorVariance;
    /* a = mean(y) - b*mean(x) page 489 Mendenhall, Beaver, Beaver - 2003 */
    v1->a = (v2->sumData/v2->count) - (v1->b * (v1->sumData/v1->count));

    v2->r = v1->r = 0.0;
    /*	do not compute r for this region if N is < 2	*/
    if (v1->count < 2)
	{
	endTime = clock1000();
	result->calcTime += endTime - startTime;
	continue;
	}
    /*	For this region, ready to compute r, N is v1->count	*/
    variance1 = numeratorVariance / (double)(v1->count-1);
    variance2 = (v2->sumSquares - ((v2->sumData*v2->sumData)/v2->count)) /
	(double)(v2->count-1);
    stddev1 = stddev2 = 0.0;
    if (variance1 > 0.0)
	stddev1 = sqrt(variance1);
    if (variance2 > 0.0)
	stddev2 = sqrt(variance2);
    if ((stddev1 * stddev2) != 0.0)
	v2->r = v1->r = (numeratorCoVariance / (v1->count - 1)) /
			    (stddev1 * stddev2);
    else
	v2->r = v1->r = 1.0;

    endTime = clock1000();
    result->calcTime += endTime - startTime;
    }

result->r = 0.0;
if (result->count > 1)
    {
    double numeratorCoVariance = 0.0;
    double numeratorVariance = 0.0;

    numeratorCoVariance = totalSumProduct -
	((vector1sumData*vector2sumData)/result->count);
    numeratorVariance = vector1sumSquares -
	((vector1sumData*vector1sumData)/result->count);

    result->b = numeratorCoVariance / numeratorVariance;
    result->a = (vector2sumData/result->count) -
	(result->b * (vector1sumData/result->count));

    variance1 = numeratorVariance / (double)(result->count-1);
    variance2 = (vector2sumSquares -
			((vector2sumData*vector2sumData)/result->count)) /
		(double)(result->count-1);
    stddev1 = stddev2 = 0.0;
    if (variance1 > 0.0)
	stddev1 = sqrt(variance1);
    if (variance2 > 0.0)
	stddev2 = sqrt(variance2);
    if ((stddev1 * stddev2) != 0.0)
	result->r = (numeratorCoVariance / (result->count - 1)) /
			(stddev1 * stddev2);
    else
	result->r = 1.0;
    }

return result;
}	/*	struct dataVector *runRegression()	*/

void doCorrelateMore(struct sqlConnection *conn)
/* Continue working in correlate page. */
{
struct trackDb *tdb2;
char *tableName2;
char *onChange = onChangeEither();
/*char *op;*/
char *table2onEntry;
char *table2;
boolean isBedGraph1 = FALSE;
boolean isWig1 = FALSE;
boolean correlateOK1 = FALSE;
char *maxLimitCartVar;
char *maxLimitCountStr;
int maxLimitCount;
char *tmpString;
struct trackTable *tableList;
long startTime, endTime;

startTime = clock1000();

tableList = NULL;	/*	initialize the list	*/

if (curTrack)
    isBedGraph1 = startsWith("bedGraph", curTrack->type);
if (curTable && database)
    isWig1 = isWiggle(database,curTable);

/*	We are going to be TRUE here since this selection was limited on
 *	the mainPage to only these types of tables.
 */
correlateOK1 = isWig1 || isBedGraph1;

table2onEntry = cartUsualString(cart, hgtaNextCorrelateTable2, "none");

/*	add first table to the list	*/
tableList = allocTrackTable();
tableList->tdb = curTrack;
tableList->tableName = cloneString(curTable);
fillInTrackTable(tableList, conn);

if (differentWord(table2onEntry,"none") && strlen(table2onEntry))
    {
    htmlOpen("Correlate table '%s' with table '%s'", tableList->shortLabel, table2onEntry);
    }
else
    htmlOpen("Correlate table '%s'", tableList->shortLabel);

hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=GET>\n");
cartSaveSession(cart);

/* Print group, track, and table selection menus */
tdb2 = showGroupTrackRowLimited(hgtaNextCorrelateGroup, onChange, 
    hgtaNextCorrelateTrack, onChange, conn, hgtaNextCorrelateTable2, &table2);

tableName2 = tdb2->shortLabel;

if (isCustomTrack(curTable))
    maxLimitCartVar=filterFieldVarName("ct", curTable, "_",
	correlateMaxDataPoints);
else
    maxLimitCartVar=filterFieldVarName(database,curTable, "_",
	correlateMaxDataPoints);

maxLimitCountStr = cartUsualString(cart, maxLimitCartVar, maxResultsMenu[0]);

tmpString = cloneString(maxLimitCountStr);

stripChar(tmpString, ',');
maxLimitCount = sqlUnsigned(tmpString);
freeMem(tmpString);

hPrintf("<TABLE BORDER=0><TR><TD>Limit total data points in result:&nbsp\n");
cgiMakeDropList(maxLimitCartVar, maxResultsMenu, maxResultsMenuSize,
    maxLimitCountStr);
hPrintf("</TD></TR></TABLE>\n", maxLimitCount);

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
	struct dataVector *resultVector;
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
//hPrintf("<P>intersected vectors are %d bases long</P>\n", totalBases);
	if (totalBases > 0)
	    {
	    resultVector = runRegression(tableList, totalBases);
	    showThreeVectors(tableList, tableList->next, resultVector);
	    }
	freeTrackTableList(&tableList);
//	hPrintf("<P>freeTrackTableList OK</P>\n");
	freeDataVector(&resultVector);
//	hPrintf("<P>free resultVector OK</P>\n");
	}
    }

endTime = clock1000();

hPrintf("<P>total elapsed time for this page: %.3f seconds.</P>\n",
	0.001*(endTime-startTime));

hPrintf("%s", corrHelpText);

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[32];
    int varCount = ArraySize(nextVars);
    memcpy(saveVars, nextVars, varCount * sizeof(saveVars[0]));
    saveVars[varCount] = hgtaDoCorrelateMore;
    jsCreateHiddenForm(saveVars, varCount+1);
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
