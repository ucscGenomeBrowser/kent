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

static char const rcsid[] = "$Id: mainPage.c,v 1.6 2004/07/18 01:51:35 kent Exp $";


static struct grp *makeGroupList(struct sqlConnection *conn, 
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
hashFree(&groupsInTrackList);
hashFree(&groupsInDatabase);
return groupList;
}

struct grp *findGroup(struct grp *groupList, char *name)
/* Return named group in list, or NULL if not found. */
{
struct grp *group;
for (group = groupList; group != NULL; group = group->next)
    if (sameString(name, group->name))
        return group;
return NULL;
}

struct grp *findSelectedGroup(struct grp *groupList)
/* Find user-selected group if possible.  If not then
 * go to various levels of defaults. */
{
char *defaultGroup = "genes";
char *name = cartUsualString(cart, hgtaGroup, defaultGroup);
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

void jsDropDownCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over drop-down item to dy. */
{
dyStringPrintf(dy, "document.hiddenForm.%s.value=", var);
dyStringPrintf(dy, "document.mainForm.%s.options", var);
dyStringPrintf(dy, "[document.mainForm.%s.selectedIndex].value; ", var);
}

void jsTextCarryOver(struct dyString *dy, char *var)
/* Add statement to carry-over text item to dy. */
{
dyStringPrintf(dy, 
    "document.hiddenForm.%s.value=document.mainForm.%s.value; ",
    var, var);
}

struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = dyStringNew(1024);
dyStringAppend(dy, "onChange=\"");
jsDropDownCarryOver(dy, hgtaTrack);
jsDropDownCarryOver(dy, hgtaGroup);
dyStringAppend(dy, 
	"document.hiddenForm.hgta_regionType.value=regionType; ");
jsTextCarryOver(dy, hgtaRange);
jsDropDownCarryOver(dy, hgtaOutputType);
return dy;
}

char *onChangeEnd(struct dyString **pDy)
/* Finish up javascript onChange command. */
{
dyStringAppend(*pDy, "document.hiddenForm.submit();\"");
return dyStringCannibalize(pDy);
}

char *onChangeOrg()
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "org");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return onChangeEnd(&dy);
}

char *onChangeDb()
/* Return javascript executed when they change database. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "db");
return onChangeEnd(&dy);
}

char *onChangeGroup()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "db");
jsDropDownCarryOver(dy, "org");
return onChangeEnd(&dy);
}

void makeRegionButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
hPrintf("<INPUT TYPE=RADIO NAME=\"%s\"", hgtaRegionType);
hPrintf(" VALUE=\"%s\"", val);
hPrintf(" onClick=\"regionType='%s';\"", val);
if (sameString(val, selVal))
    hPrintf(" CHECKED");
hPrintf(">");
}

void showMainControlTable(struct sqlConnection *conn)
/* Put up table with main controls for main page. */
{
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
    {
    struct grp *group, *groupList = makeGroupList(conn, fullTrackList);
    struct grp *selGroup = findSelectedGroup(groupList);
    struct trackDb *track;

    hPrintf("<TR><TD><B>group:</B>\n");
    hPrintf("<SELECT NAME=%s %s>\n", hgtaGroup, onChangeGroup());
    for (group = groupList; group != NULL; group = group->next)
        {
	hPrintf(" <OPTION VALUE=%s%s>%s\n", group->name,
	    (group == selGroup ? " SELECTED" : ""),
	    group->label);
	}
    hPrintf(" <OPTION VALUE=all%s>All Tracks\n",
	    (group == selGroup ? " SELECTED" : ""));
    hPrintf("</SELECT>\n");

    hPrintf("<B>track:</B>\n");
    if (selGroup == NULL)   /* All Tracks */
	slSort(&fullTrackList, trackDbCmpShortLabel);
    curTrack = findSelectedTrack(fullTrackList, selGroup);
    hPrintf("<SELECT NAME=%s>\n", hgtaTrack);
    for (track = fullTrackList; track != NULL; track = track->next)
        {
	if (selGroup == NULL || sameString(selGroup->name, track->grp))
	    hPrintf(" <OPTION VALUE=%s%s>%s\n", track->tableName,
		(track == curTrack ? " SELECTED" : ""),
		track->shortLabel);
	}
    hPrintf("</SELECT>\n");
    hPrintf("</TD></TR>\n");
    }

/* Region line */
    {
    char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
    boolean doEncode = sameWord(genome, "human");
    char *range = cartUsualString(cart, hgtaRange, "");

    hPrintf("<TR><TD><B>region:</B>\n");

    /* If regionType not allowed force it to "genome". */
    if (!(sameString(regionType, "genome") ||
        sameString(regionType, "range") ||
	(doEncode && sameString(regionType, "encode") ) ) )
	regionType = "genome";
    hPrintf("<SCRIPT>\n");
    hPrintf("var regionType='%s';\n", regionType);
    hPrintf("</SCRIPT>\n");
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
    {
    hPrintf("<TR><TD><B>filter:</B>\n");
    cgiMakeButton(hgtaDoFilterPage, "Create");
    hPrintf("</TD></TR>\n");
    }

/* Output line. */
    {
    int i;
    char *outputType = cartUsualString(cart, hgtaOutputType, outPrimaryTable);
    static char *symbols[] = 
        {outPrimaryTable, outSequence, 
	 outSelectedFields, outSchema,
	 outStats, outBed, 
	 outGtf, outCustomTrack };
    static char *labels[] =
        {"all fields from primary table", "sequence", 
	 "selected fields from related tables", "schema (database organization)",
	 "statistics", "BED - Browser Extensible Data", 
	 "GTF - Gene Transfer Format", "custom track"};
    hPrintf("<TR><TD><B>output:</B>\n");
    hPrintf("<SELECT NAME=%s>\n", hgtaOutputType);
    for (i=0; i<ArraySize(symbols); ++i)
	{
	hPrintf(" <OPTION VALUE=%s", symbols[i]);
	if (sameString(symbols[i], outputType))
	    hPrintf(" SELECTED");
	hPrintf(">%s\n", labels[i]);
	}
    hPrintf("</SELECT>\n");
    hPrintf("</TD></TR>\n");
    }

/* Submit buttons. */
    {
    hPrintf("<TR><TD>\n");
    cgiMakeButton(hgtaDoTopSubmit, "Submit");
    hPrintf(" ");
    cgiMakeButton(hgtaDoIntersect, "Intersect");
    hPrintf(" ");
    cgiMakeButton(hgtaDoTest, "Test");
    hPrintf("</TD></TR>\n");
    }

hPrintf("</TABLE>\n");
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
    int i;

    hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=\"GET\" NAME=\"hiddenForm\">\n");
    cartSaveSession(cart);
    for (i=0; i<ArraySize(saveVars); ++i)
	hPrintf("<input type=\"hidden\" name=\"%s\" value=\"\">\n", saveVars[i]);
    puts("</FORM>");
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

