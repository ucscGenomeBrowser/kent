/* mainPage - stuff to put up the first table browser page. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "textOut.h"
#include "jksql.h"
#include "hdb.h"
#include "web.h"
#include "jsHelper.h"
#include "hui.h"
#include "hgColors.h"
#include "trackDb.h"
#include "grp.h"
#include "hgTables.h"
#include "joiner.h"

static char const rcsid[] = "$Id: mainPage.c,v 1.130 2008/09/18 21:35:45 braney Exp $";

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
struct dyString *dy = jsOnChangeStart();
jsDropDownCarryOver(dy, hgtaTrack);
jsDropDownCarryOver(dy, hgtaGroup);
jsTrackedVarCarryOver(dy, hgtaRegionType, "regionType");
jsTextCarryOver(dy, hgtaRange);
jsDropDownCarryOver(dy, hgtaOutputType);
jsTextCarryOver(dy, hgtaOutFileName);
return dy;
}

static char *onChangeClade()
/* Return javascript executed when they change clade. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, hgtaTable);
dyStringAppend(dy, " document.hiddenForm.org.value=0;");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
dyStringAppend(dy, " document.hiddenForm.position.value='';");
return jsOnChangeEnd(&dy);
}

static char *onChangeOrg()
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "org");
jsDropDownCarryOver(dy, hgtaTable);
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
dyStringAppend(dy, " document.hiddenForm.position.value='';");
return jsOnChangeEnd(&dy);
}

static char *onChangeDb()
/* Return javascript executed when they change database. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "db");
jsDropDownCarryOver(dy, hgtaTable);
dyStringAppend(dy, " document.hiddenForm.position.value='';");
return jsOnChangeEnd(&dy);
}

static char *onChangeGroupOrTrack()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "db");
jsDropDownCarryOver(dy, "org");
dyStringPrintf(dy, " document.hiddenForm.%s.value=0;", hgtaTable);
return jsOnChangeEnd(&dy);
}

static char *onChangeTable()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
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
addIfExists(hash, &dbList, database);
addIfExists(hash, &dbList, "swissProt");
addIfExists(hash, &dbList, "proteins");
addIfExists(hash, &dbList, "uniProt");
addIfExists(hash, &dbList, "proteome");
addIfExists(hash, &dbList, "go");
addIfExists(hash, &dbList, "hgFixed");
addIfExists(hash, &dbList, "visiGene");
addIfExists(hash, &dbList, "ultra");
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

char *showTableField(struct trackDb *track, char *varName, boolean useJoiner)
/* Show table control and label. */
{
struct slName *name, *nameList = NULL;
char *selTable;

if (track == NULL)
    nameList = tablesForDb(findSelDb());
else
    nameList = tablesForTrack(track, useJoiner);

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
	if ((curTrack == NULL) ||
	    differentWord(tdb->shortLabel, curTrack->shortLabel))
	    hPrintf(">%s (%s)\n", tdb->shortLabel, name->name);
	else
	    hPrintf(">%s\n", name->name);
    else
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
hPrintf(" ");
cartMakeCheckBox(cart, "sendToGalaxy", FALSE);
hPrintf(" Send output to <A HREF=\"http://g2.bx.psu.edu\" target=_BLANK>Galaxy</A>");
hPrintf("</TD></TR>\n");
}

struct outputType otAllFields = { NULL, 
	outPrimaryTable, 
	"all fields from selected table", };
struct outputType otSelected = { NULL, 
    outSelectedFields,
    "selected fields from primary and related tables",  };
struct outputType otSequence = { NULL, 
    outSequence,
    "sequence", };
struct outputType otPal = { NULL, 
    outPalOptions,
    "CDS FASTA alignment from multiple alignment", };
struct outputType otGff = { NULL, 
    outGff,
    "GTF - gene transfer format", };
struct outputType otBed = { NULL, 
    outBed,
    "BED - browser extensible data", };
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
struct outputType otChromGraphData = { NULL, 
     outChromGraphData, 
    "data points", };


static void showOutputTypeRow(boolean isWig, boolean isBedGr,
    boolean isPositional, boolean isMaf, boolean isChromGraphCt,
    boolean isGenePred)
/* Print output line. */
{
struct outputType *otList = NULL;

hPrintf("<TR><TD><B>output format:</B>\n");

if (isBedGr)
    {
    slAddTail(&otList, &otAllFields);
    slAddTail(&otList, &otSelected);
    slAddTail(&otList, &otWigData);
    slAddTail(&otList, &otWigBed);
    slAddTail(&otList, &otCustomTrack);
    }
else if (isWig)
    {
    slAddTail(&otList, &otWigData);
    slAddTail(&otList, &otWigBed);
    slAddTail(&otList, &otCustomTrack);
    }
else if (isMaf)
    {
    slAddTail(&otList, &otMaf);
    slAddTail(&otList, &otAllFields);
    }
else if (isChromGraphCt)
    {
    slAddTail(&otList, &otChromGraphData);
    }
else if (isPositional)
    {
    slAddTail(&otList, &otAllFields);
    slAddTail(&otList, &otSelected);
    slAddTail(&otList, &otSequence);
    slAddTail(&otList, &otGff);
#ifdef HGPAL
    if (isGenePred)
	slAddTail(&otList, &otPal);
#endif
    slAddTail(&otList, &otBed);
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
boolean isWig = FALSE, isPositional = FALSE, isMaf = FALSE, isBedGr = FALSE,
	isChromGraphCt = FALSE, isGenePred = FALSE;
boolean gotClade = hGotClade();
struct hTableInfo *hti = NULL;

hPrintf("<TABLE BORDER=0>\n");

/* Print clade, genome and assembly line. */
    {
    if (gotClade)
	{
	hPrintf("<TR><TD><B>clade:</B>\n");
	printCladeListHtml(hGenome(database), onChangeClade());
	nbSpaces(3);
	hPrintf("<B>genome:</B>\n");
	printGenomeListForCladeHtml(database, onChangeOrg());
	}
    else
	{
	hPrintf("<TR><TD><B>genome:</B>\n");
	printGenomeListHtml(database, onChangeOrg());
	}
    nbSpaces(3);
    hPrintf("<B>assembly:</B>\n");
    printAssemblyListHtml(database, onChangeDb());
    hPrintf("</TD></TR>\n");
    }

/* Print group and track line. */
    {
    hPrintf("<TR><TD>");
    selGroup = showGroupField(hgtaGroup, onChangeGroupOrTrack(), conn, allowAllTables());
    nbSpaces(3);
    curTrack = showTrackField(selGroup, hgtaTrack, onChangeGroupOrTrack());
    nbSpaces(3);
    boolean hasCustomTracks = FALSE;
    struct trackDb *t;
    for (t = fullTrackList;  t != NULL;  t = t->next)
	{
	if (isCustomTrack(t->tableName))
	    {
	    hasCustomTracks = TRUE;
	    break;
	    }
	}
    hOnClickButton("document.customTrackForm.submit();return false;",
		   hasCustomTracks ? 
                            CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL);
    
    hPrintf("</TD></TR>\n");
    }

/* Print table line. */
    {
    hPrintf("<TR><TD>");
    curTable = showTableField(curTrack, hgtaTable, TRUE);
    if (strchr(curTable, '.') == NULL)  /* In same database */
        {
	hti = getHti(database, curTable);
	isPositional = htiIsPositional(hti);
	}
    isWig = isWiggle(database, curTable);
    isMaf = isMafTable(database, curTrack, curTable);
    isBedGr = isBedGraph(curTable);
    isGenePred = isGenePredTable(curTrack, curTable);
    nbSpaces(1);
    if (isCustomTrack(curTable))
	{
	isChromGraphCt = isChromGraph(curTrack);
	}
    cgiMakeButton(hgtaDoSchema, "describe table schema");
    hPrintf("</TD></TR>\n");
    }

    if (curTrack == NULL)
	{
	struct trackDb *tdb = hTrackDbForTrack(database, curTable);
	struct trackDb *cTdb = hCompositeTrackDbForSubtrack(database, tdb);
	if (cTdb)
	    curTrack = cTdb;
	else
	    curTrack = tdb;
	isMaf = isMafTable(database, curTrack, curTable);
	}

/* Region line */
{
char *regionType = cartUsualString(cart, hgtaRegionType, hgtaRegionTypeGenome);
char *range = cartUsualString(cart, hgtaRange, "");
if (isPositional)
    {
    boolean doEncode = sqlTableExists(conn, "encodeRegions");

    hPrintf("<TR><TD><B>region:</B>\n");

    /* If regionType not allowed force it to "genome". */
    if ((sameString(regionType, hgtaRegionTypeUserRegions) &&
	 userRegionsFileName() == NULL) ||
	(sameString(regionType, hgtaRegionTypeEncode) && !doEncode))
	regionType = hgtaRegionTypeGenome;
    jsTrackingVar("regionType", regionType);
    makeRegionButton(hgtaRegionTypeGenome, regionType);
    hPrintf("&nbsp;genome&nbsp;");
    if (doEncode)
        {
	makeRegionButton(hgtaRegionTypeEncode, regionType);
	hPrintf("&nbsp;ENCODE&nbsp;");
	}
    makeRegionButton(hgtaRegionTypeRange, regionType);
    hPrintf("&nbsp;position&nbsp;");
    hPrintf("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=26 VALUE=\"%s\" onFocus=\"%s\">\n",
    	hgtaRange, range, jsRadioUpdate(hgtaRegionType, "regionType", "range"));
    cgiMakeButton(hgtaDoLookupPosition, "lookup");
    hPrintf("&nbsp;");
    if (userRegionsFileName() != NULL)
	{
	makeRegionButton(hgtaRegionTypeUserRegions, regionType);
	hPrintf("&nbsp;defined regions&nbsp;");
	cgiMakeButton(hgtaDoSetUserRegions, "change");
	hPrintf("&nbsp;");
	cgiMakeButton(hgtaDoClearUserRegions, "clear");
	}
    else
	cgiMakeButton(hgtaDoSetUserRegions, "define regions");
    hPrintf("</TD></TR>\n");
    }
else
    {
    /* Need to put at least stubs of cgi variables in for JavaScript to work. */
    jsTrackingVar("regionType", regionType);
    cgiMakeHiddenVar(hgtaRange, range);
    cgiMakeHiddenVar(hgtaRegionType, regionType);
    }

/* Select identifiers line (if applicable). */
if (!isWig && getIdField(database, curTrack, curTable, hti) != NULL)
    {
    hPrintf("<TR><TD><B>identifiers (names/accessions):</B>\n");
    cgiMakeButton(hgtaDoPasteIdentifiers, "paste list");
    hPrintf(" ");
    cgiMakeButton(hgtaDoUploadIdentifiers, "upload list");
    if (identifierFileName() != NULL)
        {
	hPrintf("&nbsp;");
	cgiMakeButton(hgtaDoClearIdentifiers, "clear list");
	}
    hPrintf("</TD></TR>\n");
    }
}

/* Filter line. */
{
hPrintf("<TR><TD><B>filter:</B>\n");
if (anyFilter())
    {
    cgiMakeButton(hgtaDoFilterPage, "edit");
    hPrintf(" ");
    cgiMakeButton(hgtaDoClearFilter, "clear");
    if (isWig || isBedGr)
	wigShowFilter(conn);
    }
else
    {
    cgiMakeButton(hgtaDoFilterPage, "create");
    }
hPrintf("</TD></TR>\n");
}

/* Composite track subtrack merge line. */
if (curTrack && tdbIsComposite(curTrack))
    {
    hPrintf("<TR><TD><B>subtrack merge:</B>\n");
    if (anySubtrackMerge(database, curTable))
	{
	cgiMakeButton(hgtaDoSubtrackMergePage, "edit");
	hPrintf(" ");
	cgiMakeButton(hgtaDoClearSubtrackMerge, "clear");
	}
    else
	{
	cgiMakeButton(hgtaDoSubtrackMergePage, "create");
	}
    hPrintf("</TD></TR>\n");
    }

/* Intersection line. */
if (isPositional)
    {
    if (anyIntersection())
        {
	hPrintf("<TR><TD><B>intersection with %s:</B>\n",
		cartString(cart, hgtaIntersectTable));
	cgiMakeButton(hgtaDoIntersectPage, "edit");
	hPrintf(" ");
	cgiMakeButton(hgtaDoClearIntersect, "clear");
	}
    else
        {
	hPrintf("<TR><TD><B>intersection:</B>\n");
	cgiMakeButton(hgtaDoIntersectPage, "create");
	}
    hPrintf("</TD></TR>\n");
    }

/* Correlation line. */
if (correlateTrackTableOK(curTrack, curTable))
    {
    char *table2 = cartUsualString(cart, hgtaCorrelateTable, "none");
    hPrintf("<TR><TD><B>correlation:</B>\n");
    if (differentWord(table2,"none") && strlen(table2))
	{
	struct grp *groupList = makeGroupList(conn, fullTrackList, TRUE);
	struct grp *selGroup = findSelectedGroup(groupList, hgtaCorrelateGroup);
	struct trackDb *tdb2 = findSelectedTrack(fullTrackList, selGroup,
		hgtaCorrelateTrack);
	if (tdbIsComposite(tdb2))
	    {
	    struct trackDb *subTdb;
	    for (subTdb=tdb2->subtracks; subTdb != NULL; subTdb=subTdb->next)
		{
		if (sameString(table2, subTdb->tableName))
		    {
		    tdb2 = subTdb;
		    break;
		    }
		}
	    }
	cgiMakeButton(hgtaDoCorrelatePage, "calculate");
	cgiMakeButton(hgtaDoClearCorrelate, "clear");
        if (tdb2 && tdb2->shortLabel)
	    hPrintf("&nbsp;(with:&nbsp;&nbsp;%s)", tdb2->shortLabel);

#ifdef NOT_YET
/* debugging 	dbg	vvvvv	*/
if (curTrack && curTrack->type)		/*	dbg	*/
    {
    hPrintf("<BR>&nbsp;(debug:&nbsp;'%s',&nbsp;'%s(%s)')", curTrack->type, tdb2->type, table2);
    }
/* debugging 	debug	^^^^^	*/
#endif

	}
    else
	cgiMakeButton(hgtaDoCorrelatePage, "create");

    hPrintf("</TD></TR>\n");
    }

/* Print output type line. */
showOutputTypeRow(isWig, isBedGr, isPositional, isMaf, isChromGraphCt,
    isGenePred);

/* Print output destination line. */
    {
    char *compressType =
	cartUsualString(cart, hgtaCompressType, textOutCompressNone);
    char *fileName = cartUsualString(cart, hgtaOutFileName, "");
    hPrintf("<TR><TD>\n");
    hPrintf("<B>output file:</B>&nbsp;");
    cgiMakeTextVar(hgtaOutFileName, fileName, 29);
    hPrintf("&nbsp;(leave blank to keep output in browser)</TD></TR>\n");
    hPrintf("<TR><TD>\n");
    hPrintf("<B>file type returned:&nbsp;</B>");
    cgiMakeRadioButton(hgtaCompressType, textOutCompressNone,
	sameWord(textOutCompressNone, compressType));
    hPrintf("&nbsp;plain text&nbsp&nbsp");
    cgiMakeRadioButton(hgtaCompressType, textOutCompressGzip,
	sameWord(textOutCompressGzip, compressType));
    hPrintf("&nbsp;gzip compressed");
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
	    name=filterFieldVarName("ct", curTable, "_", filterMaxOutputVar);
	else
	    name=filterFieldVarName(database,curTable, "_",filterMaxOutputVar);

	maxOutput = cartUsualString(cart, name, maxOutMenu[0]);

	hPrintf(
	    "<I>Note: to return more than %s lines, change the filter setting"
	    " (above). The entire data set may be available for download as"
            " a very large file that contains the original data values (not"
            " compressed into the wiggle format) -- see the Downloads page."
            "</I><BR>", maxOutput);
	}
    else
	{
	if (anySubtrackMerge(database, curTable))
	    hPrintf("<I>Note: Subtrack merge doesn't work with all fields or selected fields output.</I><BR>");
	if (anyIntersection())
	    hPrintf("<I>Note: Intersection doesn't work with all fields or selected fields output.</I><BR>");
        }
    cgiMakeButton(hgtaDoTopSubmit, "get output");
    hPrintf(" ");
    if (isPositional || isWig)
	{
	cgiMakeButton(hgtaDoSummaryStats, "summary/statistics");
	hPrintf(" ");
	}

#ifdef SOMETIMES
    hPrintf(" ");
    cgiMakeButton(hgtaDoTest, "test");
#endif /* SOMETIMES */
    }
hPrintf("<P>"
	"To reset <B>all</B> user cart settings (including custom tracks), \n"
	"<A HREF=\"/cgi-bin/cartReset?destination=%s\">click here</A>.\n",
	getScriptName());

}

void mainPageAfterOpen(struct sqlConnection *conn)
/* Put up main page assuming htmlOpen()/htmlClose()
 * will happen in calling routine. */
{
hPrintf("%s", 
  "Use this program to retrieve the data associated with a track in text "
  "format, to calculate intersections between tracks, and to retrieve "
  "DNA sequence covered by a track. For help in using this application "
  "see <A HREF=\"#Help\">Using the Table Browser</A> for a description "
  "of the controls in this form, the "
  "<A HREF=\"../goldenPath/help/hgTablesHelp.html\">User's Guide</A> for "
  "general information and sample queries, and the OpenHelix Table Browser "
  "<A HREF=\"http://www.openhelix.com/downloads/ucsc/ucsc_home.shtml\" "
  "TARGET=_blank>tutorial</A> for a narrated presentation of the software "
  "features and usage. "
  "For more complex queries, you may want to use "
  "<A HREF=\"http://main.g2.bx.psu.edu\" target=_BLANK>Galaxy</A> or "
  "our <A HREF=\"http://genome.ucsc.edu/FAQ/FAQdownloads#download29\">public "
  "MySQL server</A>. Refer to the "
  "<A HREF=\"../goldenPath/credits.html\">Credits</A> page for the list of "
  "contributors and usage restrictions associated with these data.");

/* Main form. */
hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=%s>\n",
	getScriptName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
jsWriteFunctions();
showMainControlTable(conn);
hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      "clade", "org", "db", hgtaGroup, hgtaTrack, hgtaTable, hgtaRegionType,
      hgtaRange, hgtaOutputType, hgtaOutFileName};
    jsCreateHiddenForm(cart, getScriptName(), saveVars, ArraySize(saveVars));
    }

/* Hidden form for jumping to custom tracks CGI. */
hPrintf("<FORM ACTION='%s' NAME='customTrackForm'>", hgCustomName());
cartSaveSession(cart);
hPrintf("</FORM>\n");

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

