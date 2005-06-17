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

static char const rcsid[] = "$Id: correlate.c,v 1.1 2005/06/17 19:01:32 hiram Exp $";

/*	there can be an array of these for N number of tables to work with */
struct trackTable
    {
    struct trackDb *tdb;
    char *tableName;
    };

/*	each track data's values will be copied into one of these structures */
struct dataVector
    {
    int count;		/*	number of data values	*/
    int *position;	/*	chrom position, 0-relative	*/
    float *value;	/*	data value at this position	*/
    };

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

static int countItems(struct trackDb *tdb, struct region *regions,
    struct sqlConnection *conn, double *dataSum, double *dataSumSquares,
	int *baseCount, int *start, int *end)
{
int itemCount = 0;
struct region *region;
boolean isBedGraph = FALSE;
boolean isWig = FALSE;
char *table = NULL;
double sumData = 0.0;
double sumSquares = 0.0;
int bedGraphColumnNum = 5;         /*      default score column    */
char * bedGraphColumnName = NULL;  /*	1-relative, no bin column */
int bases = 0;
int chromStart = BIGNUM;
int chromEnd = 0;

if (tdb)
    {
    table = tdb->tableName;
    isBedGraph = startsWith("bedGraph", tdb->type);
    if (startsWith("wigMaf",tdb->type))
	{
	char *wigTable = trackDbSetting(tdb, "wiggle");
	if (wigTable && hTableExists(wigTable))
	    table = wigTable;
	}
    isWig = isWiggle(database,table);
    }
else
    return itemCount;

if (! (isWig || isBedGraph))
    return itemCount;

if (isBedGraph)
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
        bedGraphColumnNum = sqlUnsigned(words[1]);
    freez(&typeLine);
    sprintf(query, "describe %s", table);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	++colCount;
	if ((1 == colCount) && sameString(row[0], "bin"))
	    colCount = 0;
	if (colCount == bedGraphColumnNum)
	    bedGraphColumnName = cloneString(row[0]);
	}
    sqlFreeResult(&sr);
    }

for (region = regions; region != NULL; region = region->next)
    {
    char *filter = filterClause(database, table, region->chrom);

    if (isBedGraph)
	{
	int rowOffset;
	char **row;
	struct sqlResult *sr = NULL;
	char fields[256];
	sr = hExtendedRangeQuery(conn, table, region->chrom, region->start,
		region->end, filter, FALSE, "count(*)", &rowOffset);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    itemCount += sqlUnsigned(row[0]);
	    }
	sqlFreeResult(&sr);
	safef(fields,ArraySize(fields), "chromStart,chromEnd,%s",
		bedGraphColumnName);
	sr = hExtendedRangeQuery(conn, table, region->chrom, region->start,
		region->end, filter, FALSE, fields, &rowOffset);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    float value = sqlFloat(row[2]);
	    int cStart, cEnd;
	    sumData += value;
	    sumSquares += value * value;
	    cStart = sqlSigned(row[0]);
	    cEnd = sqlSigned(row[1]);
	    bases += cEnd - cStart;
	    if (cStart < chromStart) chromStart = cStart;
	    if (cEnd > chromEnd) chromEnd = cEnd;
	    }
	sqlFreeResult(&sr);
	}
    else if (isWig)
	{
        struct wigAsciiData *wigData = NULL;
        struct wigAsciiData *asciiData;
        struct wigAsciiData *next;
	wigData = getWiggleAsData(conn, table, region);
	for (asciiData = wigData; asciiData; asciiData = next)
            {
	    struct asciiDatum *data = asciiData->data;
	    int i;
            next = asciiData->next;
	    itemCount += asciiData->count;
	    bases += asciiData->span * asciiData->count;
	    for (i = 0; i < asciiData->count; ++i)
		{
		sumData += data->value;
		sumSquares += data->value * data->value;
		if (data->chromStart < chromStart)
		    chromStart = data->chromStart;
		if ((data->chromStart+asciiData->span) > chromEnd)
		    chromEnd = (data->chromStart+asciiData->span);
		++data;
		}
	    
            }
	freeWigAsciiData(&wigData);
	}
    }

if (dataSum)
    *dataSum = sumData;
if (dataSumSquares)
    *dataSumSquares = sumSquares;
if (baseCount)
    *baseCount = bases;
if (start)
    *start = chromStart;
if (end)
    *end = chromEnd;

return itemCount;
}

static void statsRowOut(char *shortLabel, int chromStart, int chromEnd,
    int bases, int itemCount, double sumData, double sumSquares,
	long timeMs)
/*	display a single line of statistics	*/
{
double mean = 0.0;
double variance = 0.0;
double stddev = 0.0;

if (itemCount > 0)
    {
    mean = sumData/itemCount;
    if (itemCount > 1)
	{
	variance = (sumSquares - ((sumData*sumData)/itemCount)) /
		(double)(itemCount-1);
	if (variance > 0.0)
	    stddev = sqrt(variance);
	}
    }

hPrintf("<TD ALIGN=LEFT>%s</TD>", shortLabel);
/*	browser 1-relative start coordinates	*/
hPrintf("<TD ALIGN=RIGHT>%d</TD>", chromStart+1);
hPrintf("<TD ALIGN=RIGHT>%d</TD><TD ALIGN=RIGHT>", chromEnd);
printLongWithCommas(stdout,(long)itemCount);
hPrintf("</TD><TD ALIGN=RIGHT>");
printLongWithCommas(stdout,(long)bases);
hPrintf("</TD><TD ALIGN=RIGHT>%g</TD>", mean);
hPrintf("<TD ALIGN=RIGHT>%g</TD>", variance);
hPrintf("<TD ALIGN=RIGHT>%g</TD>", stddev);
hPrintf("<TD ALIGN=RIGHT>%.3f</TD></TR>\n", 0.001*timeMs);
}

static void showStats(struct sqlConnection *conn, struct trackTable *tableList,
	int tableCount)
/*	show the stats for the given tracks and tables	*/
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
struct region *regionList = getRegions();
struct region *region;
int i;
int regionCount = 0;

for (region = regionList; region != NULL; region = region->next)
    ++regionCount;

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

/*	some debugging stuff	*/
hPrintf("<P><TABLE BORDER=1><TR><TH COLSPAN=4>debugging information</TH></TR><TR><TH>Track</TH><TH>Track<BR>(perhaps composite)</TH><TH>table</TH><TH>type</TH></TR>\n");

for (i = 0; i < tableCount; ++i)
    {
    char *label;
    struct trackDb *tdb;
    label = tableList[i].tableName;
    if (tableList[i].tdb != NULL &&
	sameString(tableList[i].tdb->tableName, label))
	    label=tableList[i].tdb->shortLabel;
    tdb = findTdb(tableList[i].tdb,tableList[i].tableName);
    hPrintf("<TR><TD>%s</TD><TD>%s</TD><TD>%s</TD><TD>%s</TD></TR>\n",
	tdb->shortLabel, tableList[i].tdb->tableName, tableList[i].tableName,
	tdb->type);
    }
hPrintf("</TABLE></P>\n");

/*	display the stats table	*/
hPrintf("<P><TABLE BORDER=1><TR><TH>Track</TH><TH>Data<BR>start</TH><TH>Data<BR>end</TH><TH>#&nbsp;of&nbsp;Data<BR>values</TH><TH>Bases<BR>covered</TH><TH>Mean</TH><TH>Variance</TH><TH>Standard<BR>deviation</TH><TH>Calculation<BR>time&nbsp;(sec)</TR>\n");
for (i = 0; i < tableCount; ++i)
    {
    char *label;
    struct trackDb *tdb;
    int itemCount;
    double sumData;
    double sumSquares;
    int bases, start, end;
    long startTime, endTime;

    startTime = clock1000();
    label = tableList[i].tableName;
    if (tableList[i].tdb != NULL &&
	sameString(tableList[i].tdb->tableName, label))
	    label=tableList[i].tdb->shortLabel;
    tdb = findTdb(tableList[i].tdb,tableList[i].tableName);
    itemCount = countItems(tdb, regionList, conn, &sumData, &sumSquares,
	&bases, &start, &end);
    endTime = clock1000();
    statsRowOut(tdb->shortLabel, start, end, bases, itemCount,
	sumData, sumSquares, endTime-startTime);
    }
hPrintf("</TABLE></P>\n");
}

void doCorrelateMore(struct sqlConnection *conn)
/* Continue working in correlate page. */
{
struct trackDb *tdb2;
char *name = curTableLabel();
char *tableName2;
char *onChange = onChangeEither();
/*char *op;*/
char *table2onEntry;
char *table2;
boolean isBedGraph1 = FALSE;
boolean isWig1 = FALSE;
boolean correlateOK1 = FALSE;

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
    htmlOpen("Correlate table %s with table %s", name, table2onEntry);
    }
else
    htmlOpen("Correlate table %s", name);

hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=GET>\n");
cartSaveSession(cart);

/* Print group, track, and table selection menus */
tdb2 = showGroupTrackRowLimited(hgtaNextCorrelateGroup, onChange, 
    hgtaNextCorrelateTrack, onChange, conn, hgtaNextCorrelateTable2, &table2);

tableName2 = tdb2->shortLabel;

hPrintf("<P>\n");

if (differentWord(table2onEntry,"none") && strlen(table2onEntry))
    {
    if (correlateOK1)
	{
	struct trackTable tableList[2];
	tableList[0].tdb = curTrack;
	tableList[0].tableName = curTable;
	tableList[1].tdb = tdb2;
	tableList[1].tableName = table2;
	showStats(conn, tableList, 2);
	}
    }

hPrintf("<P>\n");

#ifdef NOT
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
