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
#include "joiner.h"

static char const rcsid[] = "$Id: mainPage.c,v 1.52 2004/10/01 19:27:45 kent Exp $";


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
jsTextCarryOver(dy, hgtaOutFileName);
return dy;
}

static char *onChangeOrg()
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "org");
jsDropDownCarryOver(dy, hgtaTable);
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return jsOnChangeEnd(&dy);
}

static char *onChangeDb()
/* Return javascript executed when they change database. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "db");
jsDropDownCarryOver(dy, hgtaTable);
return jsOnChangeEnd(&dy);
}

static char *onChangeGroupOrTrack()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "db");
jsDropDownCarryOver(dy, "org");
dyStringPrintf(dy, " document.hiddenForm.%s.value=0;", hgtaTable);
return jsOnChangeEnd(&dy);
}

static char *onChangeTable()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "db");
jsDropDownCarryOver(dy, "org");
jsDropDownCarryOver(dy, hgtaTable);
return jsOnChangeEnd(&dy);
}


void makeRegionButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
jsMakeTrackingRadioButton(hgtaRegionType, "regionType", val, selVal);
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
addIfExists(hash, &dbList, "ultra");
addIfExists(hash, &dbList, "swissProt");
addIfExists(hash, &dbList, "proteins");
addIfExists(hash, &dbList, "hgFixed");
addIfExists(hash, &dbList, database);
return dbList;
}

char *findSelDb()
/* Find user selected database (as opposed to genome database). */
{
struct slName *dbList = getDbListForGenome();
char *selDb = cartUsualString(cart, hgtaTrack, NULL);
if (!slNameInList(dbList, selDb))
    selDb = cloneString(dbList->name);
slFreeList(&dbList);
return selDb;
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
    hPrintf("<SELECT NAME=%s %s>\n", trackVar, trackScript);
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
	    hPrintf(" <OPTION VALUE=%s%s>%s\n", track->tableName,
		(track == selTrack ? " SELECTED" : ""),
		track->shortLabel);
	}
    hPrintf("</SELECT>\n");
    }
hPrintf("\n");
return selTrack;
}

char *unsplitTableName(char *table)
/* Convert chr*_name to name */
{
if (startsWith("chr", table))
    {
    char *s = strchr(table, '_');
    if (s != NULL)
        {
	if (startsWith("_random_", s))
	    table = s+8;
	else
	    table = s+1;
	}
    }
return table;
}

struct slName *tablesForDb(char *db)
/* Find tables associated with database. */
{
boolean isGenomeDb = sameString(db, database);
struct sqlConnection *conn = sqlConnect(db);
struct slName *raw, *rawList = sqlListTables(conn);
struct slName *cooked, *cookedList = NULL;
struct hash *uniqHash = newHash(0);

sqlDisconnect(&conn);
for (raw = rawList; raw != NULL; raw = raw->next)
    {
    if (isGenomeDb)
	{
	/* Deal with tables split across chromosomes. */
	char *root = unsplitTableName(raw->name);
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
	safef(dbTable, sizeof(dbTable), "%s.%s", db, raw->name);
	cooked = slNameNew(dbTable);
	slAddHead(&cookedList, cooked);
	}
    }
hashFree(&uniqHash);
slFreeList(&rawList);
slSort(&cookedList, slNameCmp);
return cookedList;
}

char *showTableField(struct trackDb *track)
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
selTable = cartUsualString(cart, hgtaTable, nameList->name);
if (!slNameInList(nameList, selTable))
    selTable = nameList->name;
/* Print out label and drop-down list. */
hPrintf("<B>table: </B>");
hPrintf("<SELECT NAME=%s %s>\n", hgtaTable, onChangeTable());
for (name = nameList; name != NULL; name = name->next)
    {
    hPrintf("<OPTION VALUE=%s", name->name);
    if (sameString(selTable, name->name))
        hPrintf(" SELECTED");
    hPrintf(">%s\n", name->name);
    }
hPrintf("</SELECT>\n");
return selTable;
}



struct outputType
/* Info on an output type. */
    {
    struct outputType *next;
    char *name;		/* Symbolic name of type. */
    char *label;	/* User visible label. */
    };

static void showOutDropDown(struct outputType *otList)
/* Display output drop-down. */
{
struct outputType *ot;
char *outputType = cartUsualString(cart, hgtaOutputType, otList->name);
hPrintf("<SELECT NAME=%s>\n", hgtaOutputType);
for (ot = otList; ot != NULL; ot = ot->next)
    {
    hPrintf(" <OPTION VALUE=%s", ot->name);
    if (sameString(ot->name, outputType))
	hPrintf(" SELECTED");
    hPrintf(">%s\n", ot->label);
    }
hPrintf("</SELECT>\n");
hPrintf("</TD></TR>\n");
}

struct outputType otAllFields = { NULL, 
	outPrimaryTable, 
	"all fields from primary table", };
struct outputType otSelected = { NULL, 
    outSelectedFields,
    "selected fields from primary and related tables",  };
struct outputType otSequence = { NULL, 
    outSequence,
    "sequence", };
struct outputType otGff = { NULL, 
    outGff,
    "GTF - gene transfer format", };
struct outputType otBed = { NULL, 
    outBed,
    "BED - browser extensible data", };
struct outputType otGala = { NULL, 
    outGala,
    "query results to GALA", };
struct outputType otCustomTrack = { NULL, 
    outCustomTrack,
    "custom track", };
struct outputType otHyperlinks = { NULL, 
    outHyperlinks,
    "hyperlinks to Genome Browser", };
struct outputType otWigData = { NULL, 
     outWigData, 
    "data points", };
struct outputType otWigBed = { NULL, 
     outWigBed, 
    "bed format", };
struct outputType otMaf = { NULL,
     outMaf,
     "MAF - multiple alignment format", };


static void showOutputTypeRow(boolean isWig, boolean isPositional,
	boolean isMaf)
/* Print output line. */
{
struct outputType *otList = NULL, *ot;

hPrintf("<TR><TD><B>output format:</B>\n");

if (isWig)
    {
    slAddTail(&otList, &otWigData);
    slAddTail(&otList, &otWigBed);
    if (galaAvail(database))
        slAddTail(&otList, &otGala);
    slAddTail(&otList, &otCustomTrack);
    }
else if (isMaf)
    {
    slAddTail(&otList, &otMaf);
    slAddTail(&otList, &otAllFields);
    }
else if (isPositional)
    {
    slAddTail(&otList, &otAllFields);
    slAddTail(&otList, &otSelected);
    slAddTail(&otList, &otSequence);
    slAddTail(&otList, &otGff);
    slAddTail(&otList, &otBed);
    if (galaAvail(database))
	slAddTail(&otList, &otGala);
    slAddTail(&otList, &otCustomTrack);
    slAddTail(&otList, &otHyperlinks);
    }
else
    {
    slAddTail(&otList, &otAllFields);
    slAddTail(&otList, &otSelected);
    }
showOutDropDown(otList);
}

void nbSpaces(int count)
/* Print some non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    hPrintf("&nbsp;");
}

void showMainControlTable(struct sqlConnection *conn)
/* Put up table with main controls for main page. */
{
struct grp *selGroup;
boolean isWig, isPositional = FALSE, isMaf = FALSE;
hPrintf("<TABLE BORDER=0>\n");

/* Print genome and assembly line. */
    {
    hPrintf("<TR><TD><B>genome:</B>\n");
    printGenomeListHtml(database, onChangeOrg());
    nbSpaces(3);
    hPrintf("<B>assembly:</B>\n");
    printAssemblyListHtml(database, onChangeDb());
    hPrintf("</TD></TR>\n");
    }

/* Print group and track line. */
    {
    hPrintf("<TR><TD>");
    selGroup = showGroupField(hgtaGroup, onChangeGroupOrTrack(), conn, TRUE);
    nbSpaces(3);
    curTrack = showTrackField(selGroup, hgtaTrack, onChangeGroupOrTrack());
    hPrintf("</TD></TR>\n");
    }

/* Print table line. */
    {
    hPrintf("<TR><TD>");
    curTable = showTableField(curTrack);
    if (strchr(curTable, '.') == NULL)  /* In same database */
        {
	struct hTableInfo *hti = getHti(database, curTable);
	isPositional = htiIsPositional(hti);
	}
    isWig = isWiggle(database, curTable);
    isMaf = isMafTable(database, curTrack, curTable);
    if (isWig)
	isPositional = TRUE;
    nbSpaces(1);
    cgiMakeButton(hgtaDoSchema, "Describe Table Schema");
    hPrintf("</TD></TR>\n");
    }

/* Region line */
{
char *regionType = cartUsualString(cart, hgtaRegionType, "genome");
char *range = cartUsualString(cart, hgtaRange, "");
if (isPositional)
    {
    boolean doEncode = sqlTableExists(conn, "encodeRegions");

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
    hPrintf(" position ");
    hPrintf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=26 VALUE=\"%s\" onFocus=\"%s\">\n",
    	hgtaRange, range, jsOnRangeChange(hgtaRegionType, "regionType", "range"));
    cgiMakeButton(hgtaDoLookupPosition, "Lookup");
    hPrintf("</TD></TR>\n");
    }
else
    {
    /* Need to put at least stubs of cgi variables in for JavaScript to work. */
    jsTrackingVar("regionType", regionType);
    cgiMakeHiddenVar(hgtaRange, range);
    cgiMakeHiddenVar(hgtaRegionType, regionType);
    }

/* Select identifiers line. */
if (!isWig)
    {
    hPrintf("<TR><TD><B>identifiers (names/accessions):</B>\n");
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
}

/* Filter line. */
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
if (isPositional)
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

/* Print output type line. */
showOutputTypeRow(isWig, isPositional, isMaf);

/* Print output destination line. */
    {
    char *fileName = cartUsualString(cart, hgtaOutFileName, "");
    hPrintf("<TR><TD>\n");
    hPrintf("<B>output file:</B> ");
    cgiMakeTextVar(hgtaOutFileName, fileName, 29);
    hPrintf(" (leave blank to keep output in browser)\n");
    hPrintf("</TD></TR>\n");
    }

hPrintf("</TABLE>\n");


/* Submit buttons. */
    {
    hPrintf("<BR>\n");
    if (isWig)
	{
	char *name;
	extern char *maxOutMenu[];
	char *maxOutput = maxOutMenu[0];

	if (isCustomTrack(curTable))
	    name = filterFieldVarName("ct", curTable, "", filterMaxOutputVar);
	else
	    name = filterFieldVarName(database,curTable, "",filterMaxOutputVar);

	maxOutput = cartUsualString(cart, name, maxOutMenu[0]);

	hPrintf(
	    "<I>Note: output is limited to %s lines returned.  Use the"
	    " filter setting to change this limit.</I><BR>", maxOutput);
	}
    else
	{
	if (anyIntersection())
	    hPrintf("<I>Note: Intersection is ignored in all fields & selected fields output.</I><BR>");
        }
    cgiMakeButton(hgtaDoTopSubmit, "Get Output");
    hPrintf(" ");
    if (isPositional || isWig)
	{
	cgiMakeButton(hgtaDoSummaryStats, "Summary/Statistics");
	hPrintf(" ");
	}

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
  "format, to calculate intersections between tracks, and to retrieve "
  "DNA sequence covered by a track. See <A HREF=\"#Help\">Using the Table "
  "Browser</A> for a description of the controls in this form.  The "
  "<A HREF=\"../cgi-bin/hgText\">old Table Browser Page</A> is still available "
  "for a limited period.");

/* Main form. */
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" NAME=\"mainForm\" METHOD=GET>\n");
cartSaveSession(cart);
jsWriteFunctions();
showMainControlTable(conn);
hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      "org", "db", hgtaGroup, hgtaTrack, hgtaTable, hgtaRegionType,
      hgtaRange, hgtaOutputType, hgtaOutFileName};
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

