/* mainPage - stuff to put up the first table browser page. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "hgColors.h"
#include "trackDb.h"
#include "grp.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: mainPage.c,v 1.25 2004/07/24 05:32:38 kent Exp $";


struct grp *makeGroupList(struct sqlConnection *conn, 
	struct trackDb *trackList)
/* Get list of groups that actually have something in them. */
{
struct sqlResult *sr;
char **row;
struct grp *groupList = NULL, *group;
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
sr = sqlGetResult(conn,
    "select * from grp order by priority");
while ((row = sqlNextRow(sr)) != NULL)
    {
    group = grpLoad(row);
    if (hashLookup(groupsInTrackList, group->name))
	{
	slAddTail(&groupList, group);
	hashAdd(groupsInDatabase, group->name, group);
	}
    else
        grpFree(&group);
    }
sqlFreeResult(&sr);

/* Do some error checking for tracks with group names that are
 * not in database.  Just warn about them. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInDatabase, track->grp))
         warn("Track %s has group %s, which isn't in grp table",
	 	track->tableName, track->grp);
    }

/* Create dummpy group for all tracks. */
AllocVar(group);
group->name = cloneString("all");
group->label = cloneString("All Tracks");
slAddTail(&groupList, group);

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

int trackDbCmpShortLabel(const void *va, const void *vb)
/* Sort track by shortLabel. */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
return strcmp(a->shortLabel, b->shortLabel);
}

static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
jsDropDownCarryOver(dy, hgtaTrack);
jsDropDownCarryOver(dy, hgtaGroup);
jsTrackedVarCarryOver(dy, hgtaRegionType, "regionType");
jsTextCarryOver(dy, hgtaRange);
jsDropDownCarryOver(dy, hgtaOutputType);
return dy;
}

static char *onChangeOrg()
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "org");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return jsOnChangeEnd(&dy);
}

static char *onChangeDb()
/* Return javascript executed when they change database. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "db");
return jsOnChangeEnd(&dy);
}

static char *onChangeGroupOrTrack()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "db");
jsDropDownCarryOver(dy, "org");
return jsOnChangeEnd(&dy);
}


void makeRegionButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
jsMakeTrackingRadioButton(hgtaRegionType, "regionType", val, selVal);
}

struct trackDb *showGroupTrackRow(char *groupVar, char *groupScript,
    char *trackVar, char *trackScript, struct sqlConnection *conn)
/* Show group & track row of controls.  Returns selected track */
{
struct grp *group, *groupList = makeGroupList(conn, fullTrackList);
struct grp *selGroup = findSelectedGroup(groupList, groupVar);
struct trackDb *track, *selTrack;

if (trackScript == NULL)
    trackScript = "";
hPrintf("<TR><TD><B>group:</B>\n");
hPrintf("<SELECT NAME=%s %s>\n", groupVar, groupScript);
for (group = groupList; group != NULL; group = group->next)
    {
    hPrintf(" <OPTION VALUE=%s%s>%s\n", group->name,
	(group == selGroup ? " SELECTED" : ""),
	group->label);
    }
hPrintf("</SELECT>\n");

hPrintf("<B>track:</B>\n");
if (selGroup != NULL && sameString(selGroup->name, "all"))
    selGroup = NULL;
if (selGroup == NULL) /* All Tracks */
    slSort(&fullTrackList, trackDbCmpShortLabel);
selTrack = findSelectedTrack(fullTrackList, selGroup, trackVar);
hPrintf("<SELECT NAME=%s %s>\n", trackVar, trackScript);
for (track = fullTrackList; track != NULL; track = track->next)
    {
    if (selGroup == NULL || sameString(selGroup->name, track->grp))
	hPrintf(" <OPTION VALUE=%s%s>%s\n", track->tableName,
	    (track == selTrack ? " SELECTED" : ""),
	    track->shortLabel);
    }
hPrintf("</SELECT>\n");
hPrintf("</TD></TR>\n");
return selTrack;
}

static void showOutDropDown(char **types, char **labels, int typeCount)
/* Display output drop-down. */
{
int i;
char *outputType = cartUsualString(cart, hgtaOutputType, outPrimaryTable);
int selIx = stringArrayIx(outputType, types, typeCount);
if (selIx < 0) selIx = 0;
hPrintf("<SELECT NAME=%s>\n", hgtaOutputType);
for (i=0; i<typeCount; ++i)
    {
    hPrintf(" <OPTION VALUE=%s", types[i]);
    if (i == selIx)
	hPrintf(" SELECTED");
    hPrintf(">%s\n", labels[i]);
    }
hPrintf("</SELECT>\n");
hPrintf("</TD></TR>\n");
}

static void showOutputTypeRow(boolean isWig)
/* Print output line. */
{
static char *usualTypes[] = 
    {outPrimaryTable, 
     outSelectedFields, 
     outSequence, 
     outGff, 
     outBed, 
     outCustomTrack ,
     outHyperlinks};
static char *usualLabels[] =
    {"all fields from primary table", 
     "selected fields from related tables", 
     "sequence", 
     "GTF - gene transfer format", 
     "BED - browser extensible data", 
     "custom track",
     "hyperlinks to Genome Browser"};
static char *wigTypes[] = 
     {
     outWigData, 
     };
static char *wigLabels[] =
    {
    "data points", 
    };

hPrintf("<TR><TD><B>output:</B>\n");
if (isWig)
    showOutDropDown(wigTypes, wigLabels, ArraySize(wigTypes));
else
    showOutDropDown(usualTypes, usualLabels, ArraySize(usualTypes));
}


void showMainControlTable(struct sqlConnection *conn)
/* Put up table with main controls for main page. */
{
boolean isWig;
hPrintf("<TABLE BORDER=0>\n");

/* Print genome and assembly line. */
    {
    hPrintf("<TR><TD><B>genome:</B>\n");
    printGenomeListHtml(database, onChangeOrg());
    hPrintf("<B>assembly:</B>\n");
    printAssemblyListHtml(database, onChangeDb());
    hPrintf("</TD></TR>\n");
    }

/* Print group and track line. */
curTrack = showGroupTrackRow(hgtaGroup, onChangeGroupOrTrack(), hgtaTrack, 
	onChangeGroupOrTrack(), conn);
isWig = isWiggle(database, curTrack->tableName);

/* Region line */
    {
    char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
    boolean doEncode = sqlTableExists(conn, "encodeRegions");
    char *range = cartUsualString(cart, hgtaRange, "");

    hPrintf("<TR><TD><B>region:</B>\n");

    /* If regionType not allowed force it to "genome". */
    if (!(sameString(regionType, "genome") ||
        sameString(regionType, "range") ||
	(doEncode && sameString(regionType, "encode") ) ) )
	regionType = "genome";
    jsTrackingVar("regionType", regionType);
    makeRegionButton("genome", regionType);
    hPrintf(" genome ");
    if (doEncode)
        {
	makeRegionButton("encode", regionType);
	hPrintf(" ENCODE ");
	}
    makeRegionButton("range", regionType);
    hPrintf(" range ");
    cgiMakeTextVar(hgtaRange, range, 29);
    hPrintf("</TD></TR>\n");
    }

/* Select identifiers line. */
if (!isWig)
    {
    hPrintf("<TR><TD><B>select identifiers</B> (names/accessions):\n");
    cgiMakeButton(hgtaDoPasteIdentifiers, "Paste List");
    hPrintf(" ");
    cgiMakeButton(hgtaDoUploadIdentifiers, "Upload List");
    if (identifierFileName() != NULL)
        {
	hPrintf(" ");
	cgiMakeButton(hgtaDoClearIdentifiers, "Clear List");
	}
    hPrintf("</TD></TR>\n");
    }

/* Filter line. */
if (!isWig)
    {
    hPrintf("<TR><TD><B>filter:</B>\n");
    if (anyFilter())
        {
	cgiMakeButton(hgtaDoFilterPage, "Edit");
	hPrintf(" ");
	cgiMakeButton(hgtaDoClearFilter, "Clear");
	}
    else
        {
	cgiMakeButton(hgtaDoFilterPage, "Create");
	}
    hPrintf("</TD></TR>\n");
    }

/* Intersection line. */
if (!isWig)
    {
    hPrintf("<TR><TD><B>intersection:</B>\n");
    if (anyIntersection())
        {
	cgiMakeButton(hgtaDoIntersectPage, "Edit");
	hPrintf(" ");
	cgiMakeButton(hgtaDoClearIntersect, "Clear");
	}
    else
        {
	cgiMakeButton(hgtaDoIntersectPage, "Create");
	}
    hPrintf("</TD></TR>\n");
    }

/* Print group and track line. */
showOutputTypeRow(isWig);

hPrintf("</TABLE>\n");

/* Submit buttons. */
    {
    if (isWig)
	hPrintf("<I>Note: Only up to the first 100,000 data points in region will be "
	        "output.</I><BR>");
    else
	{
	if (anyIntersection())
	    hPrintf("<I>Note: Intersection is ignored in all fields & selected fields output.</I><BR>");
        }
    cgiMakeButton(hgtaDoTopSubmit, "Get Output");
    hPrintf(" ");
    cgiMakeButton(hgtaDoSummaryStats, "Summary/Statistics");
    hPrintf(" ");
    cgiMakeButton(hgtaDoSchema, "Describe Table Schema");
#ifdef SOMETIMES
    hPrintf(" ");
    cgiMakeButton(hgtaDoTest, "Test");
#endif /* SOMETIMES */
    }
}

void mainPageAfterOpen(struct sqlConnection *conn)
/* Put up main page assuming htmlOpen()/htmlClose()
 * will happen in calling routine. */
{
hPrintf("%s", 
  "Use this program to get the data associated with a track in text "
  "format, to calculate intersections between tracks, and to fetch "
  "DNA sequence covered by a track. Please <A HREF=\"#Help\">see "
  "below</A> for a description of the controls in this form. ");

/* Main form. */
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=GET>\n");
cartSaveSession(cart);
showMainControlTable(conn);
hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      "org", "db", hgtaGroup, hgtaTrack, hgtaRegionType,
      hgtaRange, hgtaOutputType, };
    jsCreateHiddenForm(saveVars, ArraySize(saveVars));
    }

webNewSection("<A NAME=\"Help\"></A>Using the Table Browser\n");
printMainHelp();
}

void doMainPage(struct sqlConnection *conn)
/* Put up the first page user sees. */
{
htmlOpen("Table Browser");
mainPageAfterOpen(conn);
htmlClose();
}

