/* correlate - handle correlateing beds. */

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

static char const rcsid[] = "$Id: correlate.c,v 1.4 2005/06/21 16:43:00 hiram Exp $";

static char *maxResultsMenu[] =
{
    "1,000,000",
    "10,000,000",
    "100,000,000",
    "200,000,000",
};
static int maxResultsMenuSize = ArraySize(maxResultsMenu);

/*	each track data's values will be copied into one of these structures */
struct dataVector
    {
    int count;		/*	number of data values	*/
    int maxCount;	/*	no more than this number of data values	*/
    int *position;	/*	array of chrom positions, 0-relative	*/
    float *value;	/*	array of data values at these positions	*/
    double sumData;	/*	sum of all data here	*/
    double sumSquares;	/*	sum of squares of all data here	*/
    long fetchTime;	/*	msec	*/
    long calcTime;	/*	msec	*/
    };

static struct dataVector *allocDataVector(int bases)
/*	allocate a dataVector for given number of bases	*/
{
struct dataVector *v;
AllocVar(v);
v->position = needHugeMem(sizeof(int) * bases);
v->value = needHugeMem(sizeof(float) * bases);
v->count = 0;		/*	nothing here yet	*/
v->maxCount = bases;	/*	do not run past this limit	*/
v->sumData = 0.0;	/*	starting sum is zero	*/
v->sumSquares = 0.0;	/*	starting sum is zero	*/
v->fetchTime = 0;
v->calcTime = 0;
return v;
}

static void freeDataVector(struct dataVector **v)
/*	free up space belonging to a dataVector	*/
{
if (v)
    {
    struct dataVector *dv;
    dv=*v;
    if (dv)
	{
	freeMem(dv->position);
	freeMem(dv->value);
	}
    freez(v);
    }
}

/*	complete set of dataVectors collected from all regions */
struct vectorSet
    {
    struct vectorSet *next;
    char *chrom;		/* Chromosome. */
    int start;			/* Zero-based. */
    int end;			/* Non-inclusive. */
    struct dataVector *data;	/* the data collected for this region */
    };

static struct vectorSet *allocVectorSet(struct dataVector *dV, char *chrom)
/*	allocate a vectorSet item for the given dataVector */
{
struct vectorSet *v;
AllocVar(v);
v->chrom = cloneString(chrom);
v->start = dV->position[0];		/* first position, 0-relative	*/
v->end = dV->position[(dV->count)-1] + 1; /* last position, non-inclusive */
v->data = dV;
hPrintf("<P>allocVectorSet: %s:%d-%d %d</P>\n", v->chrom, v->start, v->end, dV->count);
return v;
}

static void freeVectorSet(struct vectorSet **v)
/*	free up space belonging to a vectorSet	*/
{
if (v)
    {
    struct vectorSet *vSet;
    vSet=*v;
    if (vSet)
	{
	freeDataVector(&vSet->data);
	freeMem(vSet->chrom);
	}
    freez(v);
    }
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
    struct vectorSet *vSet;	/*	the data for this table, all regions */
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
if (t)
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
	freeVectorSet(&tTable->vSet);
	}
    freez(t);
    }
}

static void freeTrackTableList(struct trackTable **tl)
{
if (tl)
    {
    struct trackTable *table, *next;
    for (table = *tl; table != NULL; table = next)
	{
	next = table->next;
	freeTrackTable(&table);
	}
    }
}

/*	this is the end result of all the data collection and analysis */
struct correlationResult
    {
    struct trackTable *table1;
    struct trackTable *table2;
    struct vectorSet *residuals;
    };

#ifdef NOT_YET
static void freeCorrelationResult(struct correlationResult **cr)
/*	free up correlationResult structure	*/
{
if (cr)
    {
    struct correlationResult *cResult;
    cResult=*cr;
    freeTrackTable(&cResult->table1);
    freeTrackTable(&cResult->table2);
    freeVectorSet(&cResult->residuals);
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
/* Return TRUE if there's an correlateion to do. */
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
struct trackDb *trackList = (struct trackDb *)NULL, *tdb, *next = NULL;
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

/*	determine real tdb, and attach labels, only tdb and tableName
 *	have been set so far, these may be composites, wigMafs or
 *	something like that where the actual tdb and table is something
 *	else.	*/
static void fillInTrackTable(struct trackTable *table,
	struct sqlConnection *conn)
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
    sprintf(query, "describe %s", table->actualTable);
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
char *filter = filterClause(database, table->actualTable, region->chrom);
int regionSize = region->end - region->start;
long startTime, endTime;

if (! (table->isWig || table->isBedGraph))
    return NULL;

/*	makes no sense to work on less than two numbers	*/
if (regionSize < 2)
    return NULL;

startTime = clock1000();
/*	assume entire region is going to produce numbers	*/
vector = allocDataVector(regionSize);

if (table->isBedGraph && !table->isCustom)
    {
    int vIndex = vector->count;	/* it is starting at zero	*/
    int rowOffset;
    char **row;
    struct sqlResult *sr = NULL;
    char fields[256];
    safef(fields,ArraySize(fields), "chromStart,chromEnd,%s",
	    table->bedGraphColumnName);
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
	if (0 == vIndex)
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
    sqlFreeResult(&sr);
    }
else if (table->isWig && !table->isCustom)
    {
    int span = 1;
    struct wigAsciiData *wigData = NULL;
    struct wigAsciiData *asciiData;
    struct wigAsciiData *next;
    int vIndex = vector->count;	/* it is starting at zero	*/
    register int bases = 0;

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
    }

endTime = clock1000();

vector->fetchTime = endTime - startTime;

if (vector->count > 0)
    return vector;
else
    freeDataVector(&vector);
return NULL;
}	/*	static struct dataVector *fetchOneRegion()	*/

static void statsRowOut(char *chrom, char *shortLabel, int chromStart, int chromEnd,
    int bases, double sumData, double sumSquares,
	long fetchMs, long calcMs)
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

hPrintf("<TD ALIGN=LEFT>%s</TD>", shortLabel);
hPrintf("<TD ALIGN=LEFT>%s</TD>", chrom);
/*	browser 1-relative start coordinates	*/
if ((0 == chromStart) && (0 == chromEnd))
    {
    hPrintf("<TD COLSPAN=2>&nbsp;</TD><TD ALIGN=RIGHT>");
    }
else
    {
    hPrintf("<TD ALIGN=RIGHT>%d</TD>", chromStart+1);
    hPrintf("<TD ALIGN=RIGHT>%d</TD><TD ALIGN=RIGHT>", chromEnd);
    }
printLongWithCommas(stdout,(long)bases);
hPrintf("</TD><TD ALIGN=RIGHT>%g</TD>", mean);
hPrintf("<TD ALIGN=RIGHT>%g</TD>", variance);
hPrintf("<TD ALIGN=RIGHT>%g</TD>", stddev);
hPrintf("<TD ALIGN=RIGHT>%.3f</TD>", 0.001*fetchMs);
hPrintf("<TD ALIGN=RIGHT>%.3f</TD></TR>\n", 0.001*calcMs);
}


static void intersectVectors(struct dataVector *v1, struct dataVector *v2)
/* collapse vector data sets down to only values when positions are equal */
{
int v1Index = 0;
int v2Index = 0;
int i, j;

i = 0;
j = 0;
v1Index = 0;
v2Index = 0;

while ((i < v1->count) && (j < v2->count))
    {
    if (v1->position[i] == v2->position[j])
	{
	if (v1Index < i)
	    {
	    v1->position[v1Index] = v1->position[i];
	    v1->value[v1Index] = v1->value[i];
	    }
	if (v2Index < j)
	    {
	    v2->position[v2Index] = v2->position[j];
	    v2->value[v2Index] = v2->value[j];
	    }
	++v1Index; ++v2Index; ++i; ++j;
	}
    else if (v1->position[i] < v2->position[j])
	++i;
    else if (v1->position[i] > v2->position[j])
	++j;
    }
v1->count = v1Index;
v2->count = v2Index;
}

static void calcMean(struct dataVector *v)
{
long startTime, endTime;
register double sum = 0.0;
register double sumSquares = 0.0;
register int i;
register double value;

startTime = clock1000();
for (i = 0; i < v->count; ++i)
    {
    value = v->value[i];
    sum += value;
    sumSquares += (value * value);
    }
v->sumData = sum;
v->sumSquares = sumSquares;
endTime = clock1000();
v->calcTime = endTime - startTime;
}

static void showTwoVectors(struct trackTable *table1,
    struct trackTable *table2, struct vectorSet *vSet1,
	struct vectorSet *vSet2)
{
struct vectorSet *v1 = vSet1;
struct vectorSet *v2 = vSet2;
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

hPrintf("<P><TABLE BORDER=1><TR><TH>Track</TH><TH>Chrom</TH><TH>Data<BR>start</TH><TH>Data<BR>end</TH><TH>Bases<BR>covered</TH><TH>Mean</TH><TH>Variance</TH><TH>Standard<BR>deviation</TH><TH>Data&nbsp;fetch<BR>time&nbsp;(sec)</TH><TH>Calculation<BR>time&nbsp;(sec)</TR>\n");

for ( ; (v1 != NULL) && (v2 !=NULL); v1 = v1->next, v2=v2->next)
    {
    statsRowOut(v1->chrom, table1->shortLabel, v1->start, v1->end,
	v1->data->count, v1->data->sumData, v1->data->sumSquares,
	    v1->data->fetchTime, v1->data->calcTime);
    statsRowOut(v2->chrom, table2->shortLabel, v2->start, v2->end,
	v2->data->count, v2->data->sumData, v2->data->sumSquares,
	    v2->data->fetchTime, v2->data->calcTime);
    totalBases1 += v1->end - v1->start;
    totalBases2 += v2->end - v2->start;
    totalFetch1 += v1->data->fetchTime;
    totalFetch2 += v2->data->fetchTime;
    totalCalc1 += v1->data->calcTime;
    totalCalc2 += v2->data->calcTime;
    totalSum1 += v1->data->sumData;
    totalSum2 += v2->data->sumData;
    totalSumSquares1 += v1->data->sumSquares;
    totalSumSquares2 += v2->data->sumSquares;
    ++rowsOutput;
    }

if (0 == rowsOutput)
    hPrintf("<TR><TD COLSPAN=9>EMPTY RESULT SET</TD></TR>\n");
else
    {
    statsRowOut("OVERALL", table1->shortLabel, 0, 0,
	totalBases1, totalSum1, totalSumSquares1,
	    totalFetch1, totalCalc1);
    statsRowOut("OVERALL", table2->shortLabel, 0, 0,
	totalBases2, totalSum2, totalSumSquares2,
	    totalFetch2, totalCalc2);
    }

hPrintf("</TABLE></P>\n");

/*	debugging when 1 region, less than 100 bases, show all values	*/
if ((1 == rowsOutput) && (totalBases1 < 100) && (totalBases2 < 100))
    {
    int start, end, i;
    int v1Index = 0;
    int v2Index = 0;
    v1 = vSet1;
    v2 = vSet2;
    start = min(v1->start,v2->start);
    end = max(v1->end,v2->end);
    hPrintf("<P><TABLE BORDER=1><TR><TH>position</TH><TH>track 1</TH><TH>track 2</TH></TR>\n");
    for (i = start; i < end; ++i)
	{
hPrintf("<TR><TD ALIGN=RIGHT>%d</TD>", i+1);
	if (i >= v1->start)
	    {
	    if (i == v1->data->position[v1Index])
		{
		hPrintf("<TD ALIGN=RIGHT>%g</TD>", v1->data->value[v1Index]);
		++v1Index;
		}
	    else if (i > v1->data->position[v1Index])
		{
		hPrintf("<TD ALIGN=RIGHT>N/A</TD>");
		++v1Index;
		}
	    }
	else
	    hPrintf("<TD ALIGN=RIGHT>N/A</TD>");
	if (i >= v2->start)
	    {
	    if (i == v2->data->position[v2Index])
		{
		hPrintf("<TD ALIGN=RIGHT>%g</TD>", v2->data->value[v2Index]);
		++v2Index;
		}
	    else if (i > v2->data->position[v2Index])
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

static void collectData(struct sqlConnection *conn,
	struct trackTable *tableList, int maxLimitCount)
/*	for each track in the list, collect its raw data */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
struct region *regionList = getRegions();
struct region *region;
struct trackTable *table;
int regionCount = 0;

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
    hPrintf("<P>position: full genome, %d chromosomes</P>\n", regionCount);
else if (sameString(regionType,"encode"))
    hPrintf("<P>position: %d encode regions</P>\n", regionCount);

/*	some more debugging stuff, do we have the tables correct */
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

/*	walk through the regions and fetch each table's data
 *	after you have them both, condense them down to only those
 *	places that overlap each other
 */
    {
    struct vectorSet *vSet1;
    struct vectorSet *vSet2;
    vSet1 = NULL;
    vSet2 = NULL;
    for (region = regionList; region != NULL; region = region->next)
	{
	struct dataVector *v1;
	struct dataVector *v2;
	struct trackTable *table1;
	struct trackTable *table2;
	table1 = tableList;
	table2 = table1->next;


	v1 = fetchOneRegion(table1, region, conn);
	if (v1)	/*	if data there, fetch the second one	*/
	    {
	    v2 = fetchOneRegion(table2, region, conn);
	    if (v2)	/*	if both data, construct a result set	*/
		{
		struct vectorSet *vSet;
		calcMean(v1);
		vSet = allocVectorSet(v1,region->chrom);
		slAddTail(&vSet1, vSet);
		calcMean(v2);
		vSet = allocVectorSet(v2,region->chrom);
		slAddTail(&vSet2, vSet);
		}
	    else
		freeDataVector(&v1);	/* no data in second	*/
	    }
	}

    /*showTwoVectors(tableList, tableList->next, vSet1, vSet2);*/
    intersectVectors(vSet1->data, vSet2->data);
    vSet1->start = vSet1->data->position[0];
    vSet1->end = vSet1->data->position[(vSet1->data->count)-1] + 1;
    vSet2->start = vSet2->data->position[0];
    vSet2->end = vSet2->data->position[(vSet2->data->count)-1] + 1;
    calcMean(vSet1->data);
    calcMean(vSet2->data);
hPrintf("<P>after intersection</P>\n");
    showTwoVectors(tableList, tableList->next, vSet1, vSet2);
    freeVectorSet(&vSet1);
    freeVectorSet(&vSet2);
    hPrintf("<P>freeVectorSet 1 and 2 OK</P>\n");
    }
}

void doCorrelateMore(struct sqlConnection *conn)
/* Continue working in correlate page. */
{
struct trackDb *tdb2;
char *tableName1 = curTableLabel();
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

if (curTrack)
    isBedGraph1 = startsWith("bedGraph", curTrack->type);
if (curTable && database)
    isWig1 = isWiggle(database,curTable);

/*	We are going to be TRUE here since this selection was limited on
 *	the mainPage to only these types of tables.
 */
correlateOK1 = isWig1 || isBedGraph1;

table2onEntry = cartUsualString(cart, hgtaNextCorrelateTable2, "none");

if (differentWord(table2onEntry,"none") && strlen(table2onEntry))
    {
    htmlOpen("Correlate table %s with table %s", tableName1, table2onEntry);
    }
else
    htmlOpen("Correlate table %s", tableName1);

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
cgiMakeButton(hgtaDoClearCorrelate, "clear selections");
hPrintf("&nbsp;");
cgiMakeButton(hgtaDoMainPage, "return to table browser");
hPrintf("</FORM>\n");

hPrintf("<P>\n");

if (differentWord(table2onEntry,"none") && strlen(table2onEntry))
    {
    if (correlateOK1)
	{
	struct trackTable *tableList, *tt;

	tableList = NULL;

	/*	add first table to the list	*/
	tt = allocTrackTable();
	tt->tdb = curTrack;
	tt->tableName = cloneString(curTable);
	fillInTrackTable(tt, conn);
	slAddTail(&tableList,tt);

	/*	add second table to the list	*/
	tt = allocTrackTable();
	tt->tdb = tdb2;
	tt->tableName = cloneString(table2);
	fillInTrackTable(tt, conn);
	slAddTail(&tableList,tt);

	collectData(conn, tableList, maxLimitCount);
	freeTrackTableList(&tableList);
hPrintf("<P>freeTrackTableList OK</P>\n");
	}
    }

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

void doClearCorrelate(struct sqlConnection *conn)
/* Respond to click on clear correlateion. */
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
