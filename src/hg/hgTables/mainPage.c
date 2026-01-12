/* mainPage - stuff to put up the first table browser page. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "cart.h"
#include "cartTrackDb.h"
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
#include "trackDb.h"
#include "hubConnect.h"
#include "trackHub.h"
#include "hgConfig.h"
#include "jsHelper.h"


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

static char *onChangeGroupOrTrack()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
dyStringPrintf(dy, " document.hiddenForm.%s.value=0;", hgtaTable);
return jsOnChangeEnd(&dy);
}

static char *onChangeTable()
/* Return javascript executed when they change group. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, hgtaTable);
return jsOnChangeEnd(&dy);
}

void makeRegionButtonExtraHtml(char *val, char *selVal, char *extraHtml)
/* Make region radio button including a little Javascript to save selection state
 * and optional extra html attributes. */
{
jsMakeTrackingRadioButtonExtraHtml(hgtaRegionType, "regionType", val, selVal, extraHtml);
}

void makeRegionButton(char *val, char *selVal)
/* Make region radio button including a little Javascript
 * to save selection state. */
{
makeRegionButtonExtraHtml(val, selVal, NULL);
}

struct grp *showGroupField(char *groupVar, char *event, char *groupScript,
    struct sqlConnection *conn, boolean allTablesOk)
/* Show group control. Returns selected group. */
{
struct grp *group, *groupList = fullGroupList;
struct grp *selGroup = findSelectedGroup(groupList, groupVar);
hPrintf("<B>Group:</B>\n");
hPrintf("<SELECT NAME=%s id='%s'>\n", groupVar, groupVar);
jsOnEventById(event,groupVar,groupScript);
for (group = groupList; group != NULL; group = group->next)
    {
    if (allTablesOk || differentString(group->name, "allTables"))
        hPrintf(" <OPTION VALUE=%s%s>%s</OPTION>\n", group->name,
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

struct trackDb *showTrackField(struct grp *selGroup, char *trackVar, char *event, char *trackScript,
                               boolean disableNoGenome)
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
    hPrintf("<SELECT NAME=\"%s\" id='%s'>\n", trackVar, trackVar);
    jsOnEventById(event, trackVar, trackScript);
    for (db = dbList; db != NULL; db = db->next)
	{
	hPrintf(" <OPTION VALUE=%s%s>%s</OPTION>\n", db->name,
		(sameString(db->name, selDb) ? " SELECTED" : ""),
		db->name);
	}
    hPrintf("</SELECT>\n");
    }
else
    {
    boolean allTracks = sameString(selGroup->name, "allTracks");
    hPrintf("<B>Track:</B>\n");
    hPrintf("<SELECT NAME=\"%s\" id='%s'>\n", trackVar, trackVar);
    jsOnEventById(event, trackVar, trackScript);
    if (allTracks)
        {
	selTrack = findSelectedTrack(fullTrackList, NULL, trackVar);
	slSort(&fullTrackList, trackDbCmpShortLabel);
	}
    else
	{
	selTrack = findSelectedTrack(fullTrackList, selGroup, trackVar);
	}
    boolean selTrackIsDisabled = FALSE;
    struct trackDb *firstEnabled = NULL;
    for (track = fullTrackList; track != NULL; track = track->next)
	{
	if (allTracks || sameString(selGroup->name, track->grp))
	    {
	    hPrintf(" <OPTION VALUE=\"%s\"", track->track);
            if (cartTrackDbIsNoGenome(database, track->table))
                hPrintf(NO_GENOME_CLASS);
            if (disableNoGenome && isNoGenomeDisabled(database, track->table))
                {
                hPrintf(" DISABLED");
                if (track == selTrack)
                    selTrackIsDisabled = TRUE;
                }
            else if (firstEnabled == NULL)
                firstEnabled = track;
            if (track == selTrack && !selTrackIsDisabled)
                hPrintf(" SELECTED");
            hPrintf(">%s</OPTION>", track->shortLabel);
	    }
	}
    if (selTrackIsDisabled)
        selTrack = firstEnabled;
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



struct slName *tablesForDb(char *db)
/* Find tables associated with database. */
{
boolean isGenomeDb = sameString(db, database);
struct sqlConnection *conn = hAllocConn(db);
struct slName *raw, *rawList = sqlListTables(conn);
struct slName *cooked, *cookedList = NULL;
struct hash *uniqHash = newHash(0);

hFreeConn(&conn);
for (raw = rawList; raw != NULL; raw = raw->next)
    {
    if (cartTrackDbIsAccessDenied(db, raw->name))
        continue;
    if (isGenomeDb)
	{
	/* Deal with tables split across chromosomes. */
	char *root = unsplitTableName(raw->name);
	if (cartTrackDbIsAccessDenied(db, root))
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

char *showTableField(struct trackDb *track, char *varName, boolean useJoiner)
/* Show table control and label. */
{
struct slName *name, *nameList = NULL;
char *selTable;

if (track == NULL)
    nameList = tablesForDb(findSelDb());
else
    nameList = cartTrackDbTablesForTrack(database, track, useJoiner);

/* Get currently selected table.  If it isn't in our list
 * then revert to first in list. */
selTable = cartUsualString(cart, varName, nameList->name);
if (!slNameInListUseCase(nameList, selTable))
    selTable = nameList->name;

/* Print out label and drop-down list. */
hPrintf("<B>Table: </B>");
hPrintf("<SELECT NAME=\"%s\" id='%s'>\n", varName, varName);
jsOnEventById("change", varName, onChangeTable());
struct trackDb *selTdb = NULL;
for (name = nameList; name != NULL; name = name->next)
    {
    struct trackDb *tdb = NULL;
    if (curTrack != NULL && isHubTrack(curTrack->track))
        {
        if (sameString(curTrack->track, name->name))
            tdb = curTrack;
        else if (curTrack->subtracks != NULL)
            {
            // maybe a subtrack is what we're looking for
            struct trackDb *sub = subTdbFind(curTrack, name->name);
            if (sub)
                tdb = sub;
            }
        }
    else
        {
        if (track != NULL)
            tdb = findTdbForTable(database,track,name->name, ctLookupName);
        }
    hPrintf("<OPTION VALUE=\"%s\"", name->name);
    // Disable options for related tables that are noGenome -- if a non-positional table
    // is selected then we output its entire contents.
    if (cartTrackDbIsNoGenome(database, name->name) && fullGenomeRegion() &&
        (track == NULL || differentString(track->table, name->name)))
        hPrintf(" DISABLED"NO_GENOME_CLASS);
    else if (sameString(selTable, name->name))
        {
        hPrintf(" SELECTED");
        selTdb = tdb;
        }
    if (tdb != NULL)
	if ((curTrack == NULL) || differentWord(tdb->shortLabel, curTrack->shortLabel))
	    hPrintf(">%s (%s)", tdb->shortLabel, name->name);
	else
	    hPrintf(">%s", name->name);
    else
	hPrintf(">%s", name->name);
    hPrintf("</OPTION>\n");
    }
hPrintf("</SELECT>\n");
if (!trackHubDatabase(database))
    {
    char *restrictDate = encodeRestrictionDateDisplay(database,selTdb);
    if (restrictDate)
	{
	hPrintf("<A HREF=\'%s\' TARGET=BLANK>restricted until:</A>&nbsp;%s",
		    ENCODE_DATA_RELEASE_POLICY, restrictDate);
	freeMem(restrictDate);
	}
    }
return selTable;
}



struct outputType
/* Info on an output type. */
    {
    struct outputType *next;
    char *name;		/* Symbolic name of type. */
    char *label;	/* User visible label. */
    };

static void showOutDropDown(struct outputType *otList, struct outputType *otDefault)
/* Display output drop-down. */
{
struct outputType *ot;
char *outputType = cartUsualString(cart, hgtaOutputType, otList->name);
if (otDefault != NULL && otDefault != otList)
    {
    boolean otInOtList = FALSE;
    for (ot = otList; ot != NULL; ot = ot->next)
	if (sameString(ot->name, outputType))
	    {
	    otInOtList = TRUE;
	    break;
	    }
    if (! otInOtList)
	outputType = otDefault->name;
    }
hPrintf("<SELECT id='outputTypeDropdown' NAME=\"%s\">", hgtaOutputType);
for (ot = otList; ot != NULL; ot = ot->next)
    {
    hPrintf(" <OPTION VALUE=%s", ot->name);
    if (sameString(ot->name, outputType))
	hPrintf(" SELECTED");
    if (sameString(ot->name, outBed) || sameString(ot->name, outWigBed))
        hPrintf(" id=\"outBed\"");
    hPrintf(">%s</OPTION>\n", ot->label);
    }
hPrintf("</SELECT>\n");
hPrintf(" ");

hPrintf("<DIV style='display:none; opacity:0.9; border: 1px solid #EEE; margin: 2px; padding: 4px' id='gffNote'>"
        "<b>Note:</b> Table Browser GTF files contain transcripts, but no gene identifiers or symbols.<br> "
        "If you are looking for fully formatted "
        "gene model files for use in genome analysis pipelines,<br>check the "
        "<a href='https://hgdownload.soe.ucsc.edu/goldenPath/%s/bigZips/genes'>bigZips/genes</a> "
        "directory on our download server.</DIV>", trackHubSkipHubName(database));
hPrintf("<DIV style='display:none; opacity:0.9; border: 1px solid #EEE; margin: 2px; padding: 4px' id='wigNote'>"
        "<b>Signal data points format:</b> The Table Browser outputs signal track data in "
        "<a href='../goldenPath/help/wiggle.html' target=_blank>wiggle</a> format by default. You can also use the "
        "Table Browser <b>filter</b> feature to designate a value threshold and then output that as BED format. The "
        "Table Browser will then create a BED file with an entry for every value that passes the filter. Our command line tool "
        "<a href=\"../goldenPath/help/bigWig.html#Extract\" target=_blank>bigWigToBedGraph</a> can also be used "
        "to convert wig files directly. Contact us at genome@soe.ucsc.edu for help with data extraction or conversion.</div>");
hPrintf(" ");

// we should make an hgTables.js one day, this is ugly
jsInline("function checkOutputNotes(event) {\n"
    "var outType=document.getElementById('outputTypeDropdown').value;\n"
    "if (outType==='gff')\n"
    "    document.getElementById('gffNote').style.display='';\n"
    "else if (outType==='wigData')\n"
    "    document.getElementById('wigNote').style.display='';\n"
    "else\n"
    "    $('.outputNote').hide();\n" // a lot shorter with Jquery than without
    "}\n"
    "$(document).ready(checkOutputNotes);\n"
);
jsAddEventForId("change", "outputTypeDropdown", "checkOutputNotes");

jsInline("function checkSnpTablesNote(event) {\n"  
    "var trackName = document.getElementById('hgta_track').value;\n"
    "if (trackName.startsWith('dbSnp') || trackName.startsWith('snp') || trackName.startsWith('gnomad'))\n"
    "    document.getElementById('snpTablesNote').style.display='';\n"
    "else\n"
    "    document.getElementById('snpTablesNote').style.display='none';\n"
    "}\n"
    "$(document).ready(checkSnpTablesNote);\n"
);
jsAddEventForId("change", "outputTypeDropdown", "checkSnpTablesNote");

jsInlineF("function checkForCsv(event) {\n"
    "var outputType = document.getElementById('outputTypeDropdown').value;\n"
    "if (outputType === 'primaryTable' || outputType === 'selectedFields') {\n"
    "   document.getElementById('%s').parentElement.style.display='';\n"
    "   document.getElementById('excelOutNote').style.display='';\n"
    "} else {\n"
    "   document.getElementById('%s').parentElement.style.display='none';\n"
    "   document.getElementById('excelOutNote').style.display='none';\n"
    "}\n"
    "}\n"
    "$(document).ready(checkForCsv);\n"
    , hgtaOutSep, hgtaOutSep);
jsAddEventForId("change", "outputTypeDropdown", "checkForCsv");

/*
 * Code to add in the interactive tutorial*/
if (cfgOptionBooleanDefault("showTutorial", TRUE))
    {
    jsIncludeFile("shepherd.min.js", NULL);
    webIncludeResourceFile("shepherd.css");
    jsIncludeFile("jquery.js", NULL);
    jsIncludeFile("jquery-ui.js", NULL);
    webIncludeResourceFile("jquery-ui.css");

    jsIncludeFile("tutorialPopup.js", NULL);
    jsIncludeFile("tableBrowserTutorial.js",NULL);
    if (sameOk(cgiOptionalString("startTutorial"), "true"))
        {
        jsInline("var startTableBrowserOnLoad = true;");
        jsInline("tableBrowserTour.start();");
        }
    }



if (!cfgOptionBooleanDefault("hgta.disableSendOutput", FALSE))
    {
    hPrintf(" Send output to ");
    struct dyString *dy = dyStringNew(256);
    dyStringAppend(dy, 
	"document.getElementById('checkboxGreat').checked=false;");
    if (isGenomeSpaceEnabled())
	dyStringAppend(dy, 
	      	  "document.getElementById('checkboxGenomeSpace').checked=false;");
    dyStringAppend(dy, 
	      	  "return true;");
    cgiMakeCheckBoxWithId("sendToGalaxy", doGalaxy(), "checkboxGalaxy");
    jsOnEventById("click", "checkboxGalaxy", dy->string);
    hPrintf("<A HREF=\""GALAXY_URL_BASE"\" target=_BLANK>Galaxy</A>\n");
    nbSpaces(2);
    cgiMakeCheckBoxWithId("sendToGreat", doGreat(), "checkboxGreat");
    jsOnEventById("click", "checkboxGreat", "return onSelectGreat();");
    hPrintf(" <A HREF=\"http://great.stanford.edu\" target=_BLANK>GREAT</A>");
    if (isGenomeSpaceEnabled())
	{
	nbSpaces(2);
	cgiMakeCheckBoxWithId("sendToGenomeSpace", doGenomeSpace(), "checkboxGenomeSpace");
	jsOnEventById("click", "checkboxGenomeSpace", 
	    "document.getElementById('checkboxGreat').checked=false;"
	    "document.getElementById('checkboxGalaxy').checked=false; return true;");
	hPrintf(" <A HREF=\"http://www.genomespace.org\" target=_BLANK>GenomeSpace</A>");
	}
    }

hPrintf("</DIV></TD></TR>\n");
}

struct outputType otAllFields = { NULL,	outPrimaryTable,"All fields from selected table", };
struct outputType otSelected =  { NULL, outSelectedFields,
                                  "Selected fields from primary and related tables",  };
struct outputType otSequence =  { NULL, outSequence,    "Sequence", };
struct outputType otPal =       { NULL, outPalOptions,
                                  "CDS FASTA alignment from multiple alignment", };
struct outputType otGff =         { NULL, outGff,         "GTF - gene transfer format &#40;limited&#41;", };
struct outputType otBed =         { NULL, outBed,         "BED - browser extensible data", };
struct outputType otCustomTrack = { NULL, outCustomTrack, "Custom track", };
struct outputType otHyperlinks =  { NULL, outHyperlinks,  "Hyperlinks to Genome Browser", };
struct outputType otWigData =     { NULL, outWigData,     "Data points", };
struct outputType otWigBed =      { NULL, outWigBed,      "Bed format", };
struct outputType otMaf =         { NULL, outMaf,         "MAF - multiple alignment format", };
struct outputType otChromGraphData =      { NULL, outChromGraphData,       "Data points", };
struct outputType otMicroarrayNames =     { NULL, outMicroarrayNames,     "Microarray names", };
struct outputType otMicroarrayGroupings = { NULL, outMicroarrayGroupings, "Microarray groupings", };

static void showOutputTypeRow(boolean isWig, boolean isBedGr,
    boolean isPositional, boolean isMaf, boolean isChromGraphCt,
    boolean isPal, boolean isMicroarray, boolean isHalSnake)
/* Print output line. */
{
struct outputType *otList = NULL, *otDefault = NULL;
boolean bedifiedOnly = (anySubtrackMerge(database, curTable) || anyIntersection());

hPrintf("<TR><TD><DIV ID=\"output-select\"><B>Output format:</B>\n");

if (isBedGr)
    {
    if (! bedifiedOnly)
	{
	slAddTail(&otList, &otAllFields);
	slAddTail(&otList, &otSelected);
	}
    slAddTail(&otList, &otWigData);
    slAddTail(&otList, &otWigBed);
    slAddTail(&otList, &otCustomTrack);
    slAddTail(&otList, &otHyperlinks);
    }
else if (isWig)
    {
    slAddTail(&otList, &otWigData);
    slAddTail(&otList, &otWigBed);
    slAddTail(&otList, &otCustomTrack);
    // hyperlinks output works for db-wiggle but not for bigWig
    }
else if (isHalSnake)
    {
    slAddTail(&otList, &otMaf);
    }
else if (isMaf)
    {
    slAddTail(&otList, &otMaf);
    if (! bedifiedOnly)
	slAddTail(&otList, &otAllFields);
    }
else if (isChromGraphCt)
    {
    slAddTail(&otList, &otChromGraphData);
    }
else if (isMicroarray)
    {
    slAddTail(&otList, &otMicroarrayNames);
    slAddTail(&otList, &otAllFields);
    slAddTail(&otList, &otSelected);
    slAddTail(&otList, &otHyperlinks);
    }
else if (isPositional)
    {
    if (! bedifiedOnly)
	{
	slAddTail(&otList, &otAllFields);
	slAddTail(&otList, &otSelected);
	}
    else
	otDefault = &otBed;
    slAddTail(&otList, &otSequence);
    slAddTail(&otList, &otGff);
    if (isPal)
	slAddTail(&otList, &otPal);
    slAddTail(&otList, &otBed);
    slAddTail(&otList, &otCustomTrack);
    slAddTail(&otList, &otHyperlinks);
    }
else
    {
    slAddTail(&otList, &otAllFields);
    slAddTail(&otList, &otSelected);
    }
showOutDropDown(otList, otDefault);
}

void nbSpaces(int count)
/* Print some non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    hPrintf("&nbsp;");
}

/* Stepwise instructions to guide users */

#define STEP_MAX        4       // 1-based
#define HELP_LABEL      "Help"

static char *stepLabels[] = 
{
"Select dataset",
"Define region of interest",
"Optional: Subset, combine, compare with another track",
"Retrieve and display data"
};

static char *stepHelp[] = 
{
"Specify the genome, track and data table to be used as the data source.",
"Specify whole genome or restrict to a single or set of genomic regions, "
        "defined by coordinates or identifiers.",
"Press 'create' button and select parameters for optional operations.",
"Specify output options and press the 'get output' button."
};

static char *stepHelpLinks[] =
{
"https://genome.ucsc.edu/goldenPath/help/hgTablesHelp.html#GettingStarted",
"https://genome.ucsc.edu/goldenPath/help/hgTablesHelp.html#GettingStarted",
"https://genome.ucsc.edu/goldenPath/help/hgTablesHelp.html#Filter",
"https://genome.ucsc.edu/goldenPath/help/hgTablesHelp.html#OutputFormats"
};

static void printStep(int num)
/* Print user guidance via steps */
{
if (num > STEP_MAX)
    errAbort("Internal error: table browser help problem");
hPrintf("<tr height='16px'><td></td></tr>");
hPrintf("<tr><td>");
hPrintf(" <div class='tbTooltip'>");
hPrintf("<span class='tbTooltipLabel'><b>%s</b>",
            stepLabels[num-1]);

hPrintf("&nbsp;");
printInfoIconSvg();
hPrintf("</span>");

hPrintf("<span class='tbTooltiptext'>%s <a target='_blank' href='%s'>%s</a></span>\n",
            stepHelp[num-1], stepHelpLinks[num-1], HELP_LABEL);
hPrintf("</div>");
hPrintf("</td></tr>");
hPrintf("<tr height='6px'><td></td></tr>");
}

void printNoGenomeWarning(struct trackDb *curTrack) {
    /* print a message box that explains why a track is not downloadable */
    hPrintf("<DIV style='background-color: #faf2bb; opacity:0.9; border: 1px solid #EEE; margin: 2px; padding: 4px'>");
    char *noGenomeNote = trackDbSettingClosestToHome(curTrack, "noGenomeReason");
    hPrintf("<b>Note:</b> This track is unavailable for genome-wide download. ");
    if (noGenomeNote)
        hPrintf("Reason: %s", noGenomeNote);
    else
        hPrintf("Usually, this is due to distribution restrictions of the source database or the size of the track. Please see the track documentation for more details. Contact us if you are still unable to access the data. ");
    hPrintf("</DIV>");
}

void showMainControlTable(struct sqlConnection *conn)
/* Put up table with main controls for main page. */
{
struct grp *selGroup;
boolean isWig = FALSE, isPositional = FALSE, isMaf = FALSE, isBedGr = FALSE,
        isChromGraphCt = FALSE, isPal = FALSE, isArray = FALSE, isBam = FALSE, isVcf = FALSE, isHalSnake = FALSE,
        isLongTabix = FALSE, isHic = FALSE;
//boolean gotClade = hGotClade();
struct hTableInfo *hti = NULL;

hPrintf("<TABLE BORDER=0>\n");

int stepNumber = 1;
printStep(stepNumber++);

/* Print genome search bar line. */
    {
    hPrintf("<tr style=\"display: block\"><td class='searchCell'>\n");
    char *searchPlaceholder = "Search any species, genome or assembly name";
    char *searchBarId = "genomeSearch";
    jsIncludeAutoCompleteLibs();
    printGenomeSearchBar(searchBarId, searchPlaceholder, NULL, TRUE, "Genome:", "tbSearchLabel");
    jsInlineF(
        "function hgTablesSelect(selectEle, item) {\n"
        "   if (item.disabled || !item.genome) return;\n"
        "   selectEle.innerHTML = item.label;\n"
        "   document.hiddenForm.db.value = item.genome;\n"
        "   document.hiddenForm.hgta_regionType.value = regionType;\n"
        "   document.hiddenForm.hgta_outputType.value = document.getElementById('outputTypeDropdown').value;\n"
        "   document.hiddenForm.submit();\n"
        "}\n\n"
        "function onSearchError(jqXHR, textStatus, errorThrown, term) {\n"
        "    return [{label: 'No genomes found', value: '', genome: '', disabled: true}];\n"
        "}\n\n"
        "document.addEventListener(\"DOMContentLoaded\", () => {\n"
        "    // bind the actual <select> to the function hgTablesSelect, that way\n"
        "    // initSpeciesAutoCompleteDropdown can call the function\n"
        "    // for hgTables specifically, we need to set the hiddenForm.db value so\n"
        "    // we can force a form submit when the user chooses a new species\n"
        "    document.hiddenForm.db.value = \"%s\";\n"
        "    let selectEle = document.getElementById(\"genomeLabel\").childNodes[1];\n"
        "    let boundSelect = hgTablesSelect.bind(null, selectEle);\n"
        "    initSpeciesAutoCompleteDropdown('%s', boundSelect, null, null, null, onSearchError);\n"
        "    // make the search button trigger the autocomplete manually\n"
        "    let btn = document.getElementById(\"%sButton\");\n"
        "    btn.addEventListener(\"click\", () => {\n"
        "        let val = document.getElementById(\"%s\").value;\n"
        "        $(\"[id=\'%s\']\").autocompleteCat(\"search\", val);\n"
        "    });\n"
        "});\n"
        , database, searchBarId, searchBarId, searchBarId, searchBarId
    );
    hPrintf("</td>\n");
    char *selectedLabel = getCurrentGenomeLabel(database);
    hPrintf("<td class='searchCell' id='genomeLabel'><b>Assembly:</b><span style=\"padding-left: 2px\">%s</span></td>\n", selectedLabel);
    hPrintf("</tr>\n");
    }

/* Print group and track line. */
    {
    hPrintf("<TR><TD><DIV ID=\"track-select\">");
    selGroup = showGroupField(hgtaGroup, "change", onChangeGroupOrTrack(), conn, hAllowAllTables());
    nbSpaces(3);
    curTrack = showTrackField(selGroup, hgtaTrack, "change", onChangeGroupOrTrack(), FALSE);
    nbSpaces(3);
    hPrintf("</DIV></TD></TR>\n");
    }

/* Print table line. */
    {
    hPrintf("<TR><TD><DIV ID=\"table-select\">");
    curTable = showTableField(curTrack, hgtaTable, TRUE);
    // note that fullTableToTdbHash track hash is missing many tables including knownCanonical so cannot use it. 
    if (isHubTrack(curTable) || (strchr(curTable, '.') == NULL))  /* In same database */
        {
        hti = getHti(database, curTable, conn);
        isPositional = htiIsPositional(hti);
        }
    isLongTabix = isLongTabixTable( curTable);
    isBam = isBamTable(curTable);
    isHic = isHicTable(curTable);
    isVcf = isVcfTable(curTable, NULL);
    isWig = isWiggle(database, curTable);
    if (isBigWigTable(curTable))
        {
        isPositional = TRUE;
        isWig = TRUE;
        }
    isHalSnake = isHalTable( curTable);
    isMaf = isMafTable(database, curTrack, curTable);
    isBedGr = isBedGraph(curTable);
    isArray = isMicroarray(curTrack, curTable);
    struct trackDb *tdb = findTdbForTable(database, curTrack, curTable, ctLookupName);
    isPal = isPalCompatible(conn, tdb, curTable);
    nbSpaces(1);
    if (isCustomTrack(curTable))
        {
        isChromGraphCt = isChromGraph(tdb);
        }
    cgiMakeButton(hgtaDoSchema, "Data format description");
    hPrintf("</DIV></TD></TR>\n");
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

/* Table-specific options */
if (isHicTable(curTable))
    {
    // Seems like there should be a better way to do this, but I'm not clear what it is at the moment.
    // TrackDb processing could probably use some refactoring.
    struct trackDb *tdb = hTrackDbForTrack(database,curTable);
    if (tdb == NULL)
        tdb = findTdbForTable(database, curTrack, curTable, ctLookupName);
    if (tdb == NULL)
        errAbort("Unable to find trackDb structure for track; something is wrong");
    hicMainPageConfig(cart, tdb);
    }

hPrintf("<tr><td><DIV style='background-color: #faf2bb; display:none; opacity:0.9; border: 1px solid #EEE; margin: 2px; padding: 4px' id='snpTablesNote'>"
        "<b>Note:</b> Most dbSNP and gnomAD variant tables are huge. Trying to download them through the Table Browser "
        "usually leads to a timeout.<br> "
        "Please see our <a href='../FAQ/FAQdownloads.html#snp'>Data Access FAQ</a> "
        "for information on how to download dbSNP data, or refer to the Data Access section of the dbSNP or gnomAD track documentation pages.</DIV></td></tr>");

/* Region line */
{
printStep(stepNumber++);

char *regionType;
if (cartVarExists(cart, "hgFind.matches")) // coming back from a search
    regionType = cartUsualString(cart, hgtaRegionType, hgtaRegionTypeRange);
else
    regionType = cartUsualString(cart, hgtaRegionType, hgtaRegionTypeGenome);

char *range = cartUsualString(cart, hgtaRange, "");

if (isPositional)
    {
    boolean doEncode = FALSE; 

    if (!trackHubDatabase(database))
	doEncode = sqlTableExists(conn, "encodeRegions");

    hPrintf("<TR><TD><DIV ID=\"position-controls\"><B>Region:</B>\n");

    /* If regionType not allowed force it to "genome". */
    if ((sameString(regionType, hgtaRegionTypeUserRegions) &&
	 userRegionsFileName() == NULL) ||
	(sameString(regionType, hgtaRegionTypeEncode) && !doEncode))
	regionType = hgtaRegionTypeGenome;
    // Is "genome" is not allowed because of tdb 'tableBrowser noGenome'?
    boolean disableGenome = ((curTrack && cartTrackDbIsNoGenome(database, curTrack->table)) ||
                             (curTable && cartTrackDbIsNoGenome(database, curTable)));
    // If "genome" is selected but not allowed, force it to "range":
    if (sameString(regionType, hgtaRegionTypeGenome) && disableGenome)
        regionType = hgtaRegionTypeRange;
    jsTrackingVar("regionType", regionType);
    if (disableGenome)
        {
        makeRegionButtonExtraHtml(hgtaRegionTypeGenome, regionType, "DISABLED");
        hPrintf("&nbsp;<span"NO_GENOME_CLASS">genome (unavailable for selected track)</span>"
                "&nbsp;");
        }
    else
        {
        makeRegionButton(hgtaRegionTypeGenome, regionType);
        hPrintf("&nbsp;Genome&nbsp;");
        }
    if (doEncode)
        {
	makeRegionButton(hgtaRegionTypeEncode, regionType);
	hPrintf("&nbsp;ENCODE Pilot regions&nbsp;");
	}
    makeRegionButton(hgtaRegionTypeRange, regionType);
    hPrintf("&nbsp;Position&nbsp;");
    hPrintf("<INPUT TYPE=TEXT NAME=\"%s\" id='%s' SIZE=26 VALUE=\"%s\">\n",
    	hgtaRange, hgtaRange, range);
    jsOnEventById("focus", hgtaRange, 
	jsRadioUpdate(hgtaRegionType, "regionType", "range"));
    cgiMakeButton(hgtaDoLookupPosition, "Lookup");
    hPrintf("&nbsp;");
    if (userRegionsFileName() != NULL)
	{
	makeRegionButton(hgtaRegionTypeUserRegions, regionType);
	hPrintf("&nbsp;Defined regions&nbsp;");
	cgiMakeButton(hgtaDoSetUserRegions, "Change");
	hPrintf("&nbsp;");
	cgiMakeButton(hgtaDoClearUserRegions, "Clear");
	}
    else
	cgiMakeButton(hgtaDoSetUserRegions, "Define regions");
    hPrintf("</DIV>");
    hPrintf("</TD></TR>\n");

    if (disableGenome) { // no need to check curTrack for NULL, disableGenome can only be set if curTable is set
        hPrintf("<tr><td>");
        printNoGenomeWarning(curTrack);
        hPrintf("</td></tr>");
    }

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
    hPrintf("<TR><TD><DIV ID=\"identifiers-controls\"><B>Identifiers (names/accessions):</B>\n");
    cgiMakeButton(hgtaDoPasteIdentifiers, "Paste list");
    hPrintf(" ");
    cgiMakeButton(hgtaDoUploadIdentifiers, "Upload list");
    if (identifierFileName() != NULL)
        {
	hPrintf("&nbsp;");
	cgiMakeButton(hgtaDoClearIdentifiers, "Clear list");
	}
    hPrintf("</DIV></TD></TR>\n");
    }
}

/*   button for option page here (median/log-ratio, etc)  */

printStep(stepNumber++);

/* Filter line. */
{
hPrintf("<TR><TD><B>Filter:</B>\n");
if (anyFilter())
    {
    cgiMakeButton(hgtaDoFilterPage, "Edit");
    hPrintf(" ");
    cgiMakeButton(hgtaDoClearFilter, "Clear");
    if (isWig || isBedGr)
	wigShowFilter(conn);
    }
else
    {
    cgiMakeButton(hgtaDoFilterPage, "Create");
    }
hPrintf("</TD></TR>\n");
}

/* Composite track subtrack merge line. */
boolean canSubtrackMerge = (curTrack && tdbIsComposite(curTrack) && !isBam && !isVcf && !isLongTabix && !isHic);
if (canSubtrackMerge)
    {
    hPrintf("<TR><TD><B>Subtrack merge:</B>\n");
    if (anySubtrackMerge(database, curTable))
	{
	cgiMakeButton(hgtaDoSubtrackMergePage, "Edit");
	hPrintf(" ");
	cgiMakeButton(hgtaDoClearSubtrackMerge, "Clear");
	}
    else
	{
	cgiMakeButton(hgtaDoSubtrackMergePage, "Create");
	}
    hPrintf("</TD></TR>\n");
    }

/* Intersection line. */
if (isPositional)
    {
    if (anyIntersection())
        {
	hPrintf("<TR><TD><B>Intersection with %s:</B>\n",
		cartString(cart, hgtaIntersectTable));
	cgiMakeButton(hgtaDoIntersectPage, "Edit");
	hPrintf(" ");
	cgiMakeButton(hgtaDoClearIntersect, "Clear");
        hPrintf("</TD></TR>\n");
	}
    else if (canIntersect(database, curTable))
        {
	hPrintf("<TR><TD><B>Intersection:</B>\n");
	cgiMakeButton(hgtaDoIntersectPage, "Create");
        hPrintf("<small style='margin-left: 12px'>Need to intersect on position?</small>");
        printInfoIcon("Want to intersect two tables on their chrom-start-end fields and keep all other fields from both tables? For example, if you want to select one track with SNPs and want to overlap against enhancers and need to output both the SNP and the enhancer information, then our <a href='hgIntegrator'>Data Annotation Integrator</a> is the right tool for you");
        hPrintf("</TD></TR>\n");
	}
    }

/* Correlation line. */
struct trackDb *tdb = findTdbForTable(database, curTrack, curTable, ctLookupName);
if (correlateTrackTableOK(tdb, curTable))
    {
    char *table2 = cartUsualString(cart, hgtaCorrelateTable, "none");
    hPrintf("<TR><TD><B>Correlation:</B>\n");
    if (differentWord(table2, "none") && strlen(table2) && ! isNoGenomeDisabled(database, table2))
        {
        struct grp *groupList = fullGroupList;
        struct grp *selGroup = findSelectedGroup(groupList, hgtaCorrelateGroup);
        struct trackDb *tdb2 = findSelectedTrack(fullTrackList, selGroup,hgtaCorrelateTrack);
        if (tdbIsComposite(tdb2))
            {
	    struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(tdb2->subtracks);
	    struct slRef *tdbRef;
	    for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
                {
		struct trackDb *subTdb = tdbRef->val;
                if (sameString(table2, subTdb->table))
                    {
                    tdb2 = subTdb;
                    break;
                    }
                }
	    slFreeList(&tdbRefList);
            }
        cgiMakeButton(hgtaDoCorrelatePage, "Calculate");
        cgiMakeButton(hgtaDoClearCorrelate, "Clear");
        if (tdb2 && tdb2->shortLabel)
            hPrintf("&nbsp;(with:&nbsp;&nbsp;%s)", tdb2->shortLabel);

#ifdef NOT_YET
        /* debugging 	dbg	vvvvv	*/
        if (curTrack && curTrack->type)		/*	dbg	*/
            {
            hPrintf("<BR>&nbsp;(debug:&nbsp;'%s',&nbsp;'%s(%s)')",
                    curTrack->type, tdb2->type, table2);
            }
        /* debugging 	debug	^^^^^	*/
#endif

        }
    else
        cgiMakeButton(hgtaDoCorrelatePage, "Create");

    hPrintf("</TD></TR>\n");
    }

/* Print output type line. */

printStep(stepNumber++);
showOutputTypeRow(isWig, isBedGr, isPositional, isMaf, isChromGraphCt, isPal, isArray, isHalSnake);

/* Print output destination line. */
    {
    char *compressType = cartUsualString(cart, hgtaCompressType, textOutCompressNone);
    char *fieldSep = cartUsualString(cart, hgtaOutSep, outTab);
    char *fileName = cartUsualString(cart, hgtaOutFileName, "");
    hPrintf("<TR><TD>\n");
    hPrintf("<DIV ID=\"filename-select\"><B>Output filename:</B>&nbsp;");
    cgiMakeTextVar(hgtaOutFileName, fileName, 29);
    hPrintf("&nbsp;(<span id='excelOutNote' style='display:none'>add .csv extension if opening in Excel, </span>leave blank to keep output in browser)</DIV></TD></TR>\n");
    hPrintf("<TR><TD>\n");
    hPrintf("<B>Output field separator:&nbsp;</B>");

    // tab or csv output
    cgiMakeRadioButton(hgtaOutSep, outTab, sameWord(outTab, fieldSep));
    hPrintf("&nbsp;tsv (tab-separated)&nbsp&nbsp;");

    cgiMakeRadioButton(hgtaOutSep, outCsv, sameWord(outCsv, fieldSep));
    hPrintf("&nbsp;csv (for excel)&nbsp;");

    hPrintf("</TD></TR>\n");
    hPrintf("<TR><TD>\n");
    hPrintf("<B>File type returned:&nbsp;</B>");
    cgiMakeRadioButton(hgtaCompressType, textOutCompressNone,
        sameWord(textOutCompressNone, compressType));
    hPrintf("&nbsp;Plain text&nbsp;");
    cgiMakeRadioButton(hgtaCompressType, textOutCompressGzip,
        sameWord(textOutCompressGzip, compressType));
    hPrintf("&nbsp;Gzip compressed");
    hPrintf("</TD></TR>\n");
    }

hPrintf("</TABLE>\n");


/* Submit buttons. */
    {
    hPrintf("<BR><DIV ID=\"submit-select\">\n");
    if (isWig || isBam || isVcf || isLongTabix || isHic)
	{
	char *name;
	extern char *maxOutMenu[];
	char *maxOutput = maxOutMenu[0];

	if (isCustomTrack(curTable))
	    name=filterFieldVarName("ct", curTable, "_", filterMaxOutputVar);
	else
	    name=filterFieldVarName(database,curTable, "_",filterMaxOutputVar);

	maxOutput = cartUsualString(cart, name, maxOutMenu[0]);

	if (isWig)
	    hPrintf(
		"<I>Note: to return more than %s lines, change the filter setting"
		" (above). The entire data set may be available for download as"
		" a very large file that contains the original data values (not"
		" compressed into the wiggle format) -- see the Downloads page."
		"</I><BR>", maxOutput);
	else if (isBam || isVcf || isLongTabix || isHic)
	    hPrintf(
		"<I>Note: to return more than %s lines, change the filter setting"
		" (above). Please consider downloading the entire data from our Download pages."
		"</I><BR>", maxOutput);
	}
    else if (anySubtrackMerge(database, curTable) || anyIntersection())
	{
	hPrintf("<I>Note: The all fields and selected fields output formats "
		"are not available when a%s has been specified.</I><BR>",
		canSubtrackMerge ? " subtrack merge or intersection" : "n intersection");
	}
    cgiMakeButton(hgtaDoTopSubmit, "Get output");
    hPrintf(" ");
    if (isPositional || isWig)
	{
	cgiMakeButton(hgtaDoSummaryStats, "Summary/statistics");
	hPrintf(" ");
	}
    hPrintf("</DIV>");

#ifdef SOMETIMES
    hPrintf(" ");
    cgiMakeButton(hgtaDoTest, "test");
#endif /* SOMETIMES */
    }
hPrintf("<P></P>");
}

static char *getGenomeSpaceText()
/* fetch GenomeSpace text if enabled */
{
if (isGenomeSpaceEnabled())
    {
    return 
  "Send data to "
  "<A HREF=\"http://www.genomespace.org\" target=_BLANK>GenomeSpace</A> for use with diverse computational tools. ";
    }
else
    {
    return "";
    }
}

void mainPageAfterOpen(struct sqlConnection *conn)
/* Put up main page assuming htmlOpen()/htmlClose()
 * will happen in calling routine. */
{
hPrintf(
  "Use this tool to retrieve and export data from the Genome Browser annotation track database. "
  "You can limit retrieval based on data attributes and intersect or merge with data from "
  "another track, or retrieve DNA sequence covered by a track."
);
hPrintf(" <span id='tbHelpMore' class='blueLink'>More...</span>");
hPrintf(
  "<span id='tbHelp' style='display:none'>"
  "For a description of the controls below, "
  "see <A HREF=\"#Help\">Using the Table Browser</A> (below). "
  "General information and sample queries are available from the "
  "<A HREF=\"../goldenPath/help/hgTablesHelp.html\">Table Browser User's Guide</A>. "
  "For more complex queries, we recommend "
  "<A HREF=\""GALAXY_URL_BASE"\" target=_BLANK>Galaxy</A> or "
  "our <A HREF=\"../goldenPath/help/mysql.html\">public "
  "MySQL server</A>. "
  "To examine the biological function of your set through annotation "
  "enrichments, send the data to "
  "<A HREF=\"http://great.stanford.edu\" target=_BLANK>GREAT</A>. "
  "%s"
  "Refer to the "
  "<A HREF=\"../goldenPath/credits.html\">Credits</A> page for the list of "
  "contributors and usage restrictions associated with these data. "
  "Bulk download of track data is available from the "
  "<A HREF=\"http%s://hgdownload.soe.ucsc.edu/downloads.html\""
  ">Sequence and Annotation Downloads</A> page.</span>"
   , getGenomeSpaceText(), cgiAppendSForHttps()
);
hPrintf(" <span id='tbHelpLess' class='blueLink' style='display:none'>Less...</span>");

// Show more or less intro text
char jsText[1024];
safef(jsText, sizeof jsText,
        "$('#tbHelpMore').hide();"
        "$('#tbHelp').show();"
        "$('#tbHelpLess').show();"
        );
jsOnEventById("click", "tbHelpMore", jsText);
safef(jsText, sizeof jsText,
        "$('#tbHelpMore').show();"
        "$('#tbHelp').hide();"
        "$('#tbHelpLess').hide();"
        );
jsOnEventById("click", "tbHelpLess", jsText);


// When GREAT is selected, disable the other checkboxes and force output to BED
jsInline(
    "function onSelectGreat() {\n"
    " document.getElementById('checkboxGalaxy').checked=false;\n");
if (isGenomeSpaceEnabled())
    jsInline(
    " document.getElementById('checkboxGenomeSpace').checked=false;\n");
jsInline(
    " document.getElementById('outBed').selected=true;\n"
    " return true;\n"
    "}\n");

// Disable/enable noGenome tracks depending on whether region is genome.
jsInline(
    "function maybeDisableNoGenome() {\n"
    "   var regionTypeSelected = $('input[name=\"hgta_regionType\"]:checked').val();\n"
    "   var regionIsGenome = (regionTypeSelected === 'genome');\n"
    "   var $noGenomeOptions = $('select[name=\"hgta_track\"] option.hgtaNoGenome,select[name=\"hgta_table\"] option.hgtaNoGenome');\n"
    "   $noGenomeOptions.attr('disabled', regionIsGenome)\n"
    "                   .css('color', regionIsGenome ? '' : 'black');\n"
    "}\n"
    "$(document).ready(function() {\n"
    // once when the page loads, and every time the user changes the region type:
    "    maybeDisableNoGenome();\n"
    "    $('input[name=\"hgta_regionType\"]').change(maybeDisableNoGenome);\n"
    "});\n");

jsInline(
  "function createTutorialLink() {\n"
  "  // allow the user to bring the tutorials popup via a new help menu button\n"
  "  var tutorialLinks = document.createElement('li');\n"
  "  tutorialLinks.id = 'hgGatewayHelpTutorialLinks';\n"
  "  tutorialLinks.innerHTML = \"<a id='hgGatewayHelpTutorialLinks' href='#showTutorialPopup'>Interactive Tutorials</a>\";\n"
  "  $(\"#help > ul\")[0].appendChild(tutorialLinks);\n"
  "  $(\"#hgGatewayHelpTutorialLinks\").on(\"click\", function () {\n"
  "    // Check to see if the tutorial popup has been generated already\n"
  "    var tutorialPopupExists = document.getElementById(\"tutorialContainer\");\n"
  "    if (!tutorialPopupExists) {\n"
  "      // Create the tutorial popup if it doesn't exist\n"
  "      createTutorialPopup();\n"
  "    } else {\n"
  "      // Otherwise use jQuery UI to open the popup\n"
  "      $(\"#tutorialContainer\").dialog(\"open\");\n"
  "    }\n"
  "  });\n"
  "}\n"
  "$(document).ready(createTutorialLink);\n"
);

/* Main form. */
hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=%s>\n",
	getScriptName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
jsInit();
showMainControlTable(conn);
hPrintf("</FORM>\n");

/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      "db", hgtaGroup, hgtaTrack, hgtaTable, hgtaRegionType,
      hgtaRange, hgtaOutputType, hgtaOutFileName};
    jsCreateHiddenForm(cart, getScriptName(), saveVars, ArraySize(saveVars));
    }

webNewSection("<A NAME=\"Help\"></A>Using the Table Browser\n");
printMainHelp();
cartFlushHubWarnings();
}

void doMainPage(struct sqlConnection *conn)
/* Put up the first page user sees. */
{
htmlOpen("Table Browser");
//webIncludeResourceFile("jquery-ui.css");
mainPageAfterOpen(conn);
htmlClose();
}



