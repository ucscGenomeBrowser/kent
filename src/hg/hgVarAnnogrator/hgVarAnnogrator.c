/* hgVarAnnogrator - User interface for variant annotation integrator tool. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "hPrint.h"
#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "jsHelper.h"
#include "pipeline.h"
#include "textOut.h"
#include "hgFind.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "hgColors.h"
#include "hgConfig.h"
#include "udc.h"
#include "customTrack.h"
#include "grp.h"
#include "hCommon.h"
#include "trackDb.h"
#include "wikiTrack.h"
#include "hgMaf.h"
#if ((defined USE_BAM || defined USE_TABIX) && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#endif//def (USE_BAM || USE_TABIX) && KNETFILE_HOOKS

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars;	/* The cart before new cgi stuff added. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *database;		/* Current genome database - hg17, mm5, etc. */
char *freezeName;	/* Date of assembly. */
struct trackDb *forbiddenTrackList; /* List of tracks with 'tableBrowser off' setting. */
static struct pipeline *compressPipeline = (struct pipeline *)NULL;
struct grp *fullGroupList;	/* List of all groups. */
struct trackDb *fullTrackList;	/* List of all tracks in database. */
struct customTrack *theCtList = NULL;	/* List of custom tracks. */
struct slName *browserLines = NULL;	/* Browser lines in custom tracks. */

//#*** Put this in .h:
#define hgvaRange "position"
#define hgvaRegionType "hgva_regionType"
//#define hgva "hgva_"

void addSomeCss()
/* This should go in a .css file of course. */
{
hPrintf("<style>\n.sectionLiteHeader { font-weight: bold; font-size:1.1em; color:#000000; "
	"text-align:left; vertical-align:bottom; white-space:nowrap; "
	"text-indent:10px; padding-top:2px; }\n"
	".prop10 { height:10px; float:right; width:1px; }\n"
	".prop5 { height:5px; float:right; width:1px; }\n"
	".clear { clear:both; height: 1px; overflow:hidden; };\n"
	"</style>\n");
}

void printSmallVerticalSpace(struct dyString *dy)
/* Add a small vertical space, like a short <BR>.  This and the CSS properties
 * are from http://www.greywyvern.com/code/min-height-hack . */
{
char *hack = "<div class='prop10'></div><div class='clear'></div>\n";
if (dy)
    dyStringAppend(dy, hack);
else
    printf(hack);
}

void printSmallerVerticalSpace(struct dyString *dy)
/* Add an even smaller vertical space, like a short <BR>.  This and the CSS properties
 * are from http://www.greywyvern.com/code/min-height-hack . */
{
char *hack = "<div class='prop5'></div><div class='clear'></div>\n";
if (dy)
    dyStringAppend(dy, hack);
else
    printf(hack);
}

//#*** --------------- begin verbatim from hgTables.c -- libify ------------------------
static void vaHtmlOpen(char *format, va_list args)
/* Start up a page that will be in html format. */
{
puts("Content-Type:text/html\n");
cartVaWebStart(cart, database, format, args);
}

void htmlOpen(char *format, ...)
/* Start up a page that will be in html format. */
{
va_list args;
va_start(args, format);
vaHtmlOpen(format, args);
}

void htmlClose()
/* Close down html format page. */
{
cartWebEnd();
}

char *getScriptName()
/* returns script name from environment or hardcoded for command line */
{
char *script = cgiScriptName();
if (script != NULL)
    return script;
else
    //#*** make hgVarAnnogratorName()
    return "hgVarAnnogrator";
}

boolean searchPosition(char *range)
/* Try and fill in region via call to hgFind. Return FALSE
 * if it can't find a single position. */
{
struct hgPositions *hgp = NULL;
char retAddr[512];
char position[512];
char *chrom = NULL;
int start=0, end=0;
safef(retAddr, sizeof(retAddr), "%s", getScriptName());
hgp = findGenomePosWeb(database, range, &chrom, &start, &end,
	cart, TRUE, retAddr);
if (hgp != NULL && hgp->singlePos != NULL)
    {
    safef(position, sizeof(position),
	    "%s:%d-%d", chrom, start+1, end);
    cartSetString(cart, hgvaRange, position);
    return TRUE;
    }
else if (start == 0)	/* Confusing way findGenomePosWeb says pos not found. */
    {
    cartSetString(cart, hgvaRange, hDefaultPos(database));
    return FALSE;
    }
else
    return FALSE;
}

boolean lookupPosition()
/* Look up position (aka range) if need be.  Return FALSE if it puts
 * up multiple positions. */
{
char *range = cartUsualString(cart, hgvaRange, "");
boolean isSingle = TRUE;
range = trimSpaces(range);
if (range[0] != 0)
    isSingle = searchPosition(range);
else
    cartSetString(cart, hgvaRange, hDefaultPos(database));
return isSingle;
}

//#*** ------------------ end verbatim ---------------

//#*** ------------------ begin verbatim from hgTables/mainPage.c ---------------
int trackDbCmpShortLabel(const void *va, const void *vb)
/* Sort track by shortLabel. */
{
const struct trackDb *a = *((struct trackDb **)va);
const struct trackDb *b = *((struct trackDb **)vb);
return strcmp(a->shortLabel, b->shortLabel);
}

void nbSpaces(int count)
/* Print some non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    hPrintf("&nbsp;");
}
//#*** ------------------ end verbatim ---------------

static boolean gotCustomTracks()
/* Return TRUE if fullTrackList has at least one custom track */
{
struct trackDb *t;
for (t = fullTrackList;  t != NULL;  t = t->next)
    {
    if (isCustomTrack(t->table))
	return TRUE;
    }
return FALSE;
}

static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = jsOnChangeStart();
//#*** mainPage.c saves a bunch of variables in addition to clade/org/db/position.
jsTrackedVarCarryOver(dy, hgvaRegionType, "regionType");
jsTextCarryOver(dy, hgvaRange);
return dy;
}

static char *onChangeClade()
/* Return javascript executed when they change clade. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
dyStringAppend(dy, " document.hiddenForm.org.value=0;");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
dyStringAppend(dy, " document.hiddenForm." hgvaRange ".value='';");
return jsOnChangeEnd(&dy);
}

static char *onChangeOrg()
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "org");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
dyStringAppend(dy, " document.hiddenForm." hgvaRange ".value='';");
return jsOnChangeEnd(&dy);
}

static char *onChangeDb()
/* Return javascript executed when they change database. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "db");
dyStringAppend(dy, " document.hiddenForm." hgvaRange ".value='';");
return jsOnChangeEnd(&dy);
}

void printAssemblySection()
/* Print assembly-selection stuff, pretty much identical to hgTables.
 * Redrawing the whole page when the assembly changes seems fine to me. */
{
//#*** More copied verbatim, from hgTables/mainPage.c:
/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      "clade", "org", "db", hgvaRange, hgvaRegionType, hgvaRange, };
    jsCreateHiddenForm(cart, getScriptName(), saveVars, ArraySize(saveVars));
    }

hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=%s>\n",
	getScriptName(), cartUsualString(cart, "formMethod", "POST"));
cartSaveSession(cart);
//#*** ------------------ end verbatim ---------------

// Store ctfile here so we can include it in AJAX calls.
char *ctfile = cartStringDb(cart, database, "ctfile");
if (isNotEmpty(ctfile))
    cgiMakeHiddenVar("ctfile", ctfile);

printf("<span class='sectionLiteHeader'>");
hPrintf("Select Genome Assembly\n");
printf("</span>");

printSmallVerticalSpace(NULL);
printSmallerVerticalSpace(NULL);

//#*** --------------- More copied verbatim, from hgTables/mainPage.c: ---------------
hPrintf("<TABLE BORDER=0>\n");

/* Print clade, genome and assembly line. */
{
if (hGotClade())
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
hPrintf("<BR>\n");
printSmallerVerticalSpace(NULL);
hPrintf("<div style='text-align: center;'>\n");
hOnClickButton("document.customTrackForm.submit();return false;",
	       gotCustomTracks() ? CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL);
hPrintf(" ");
if (hubConnectTableExists())
    hOnClickButton("document.trackHubForm.submit();return false;", "track hubs");
hPrintf("</div>\n");
printSmallerVerticalSpace(NULL);
hPrintf(
	"To reset <B>all</B> user cart settings (including custom tracks), \n"
	"<A HREF=\"/cgi-bin/cartReset?destination=%s\">click here</A>.\n",
	getScriptName());
hPrintf("</TD></TR>\n");
}
//#*** ------------------ end verbatim ---------------

hPrintf("</TABLE>");
hPrintf("</FORM>");
}

static boolean inSection = TRUE;  // when moved into web.c, will start out false.

void webEndHackSection()
{
printSmallerVerticalSpace(NULL);
//#*** this statement copied from web.c's static void webEndSection:
puts(
    "" "\n"
    "	</TD><TD WIDTH=15></TD></TR></TABLE>" "\n"
    "	</TD></TR></TABLE>" "\n"
    "	</TD></TR></TABLE>" "\n"
    "	" );
puts("</div>\n");
inSection = FALSE;
}

void webNewHackSection(boolean canReorderRemove, char *divId, boolean show, char* format, ...)
/* create a new section on the web page -- probably should do div w/css style, but this is quicker */
{
if (inSection)
    webEndHackSection();
inSection = TRUE;
printf("<div id='%s'", divId);
if (!show)
    printf(" style='display: none;'");
printf(">\n");

puts("<!-- +++++++++++++++++++++ START NEW SECTION +++++++++++++++++++ -->");
printSmallVerticalSpace(NULL);
puts(  // TODO: Replace nested tables with CSS (difficulty is that tables are closed elsewhere)
    "   <!--outer table is for border purposes-->\n"
    "   <TABLE WIDTH='100%' BGCOLOR='#" HG_COL_BORDER "' BORDER='0' CELLSPACING='0' CELLPADDING='1'><TR><TD>\n"
    "    <TABLE BGCOLOR='#" HG_COL_INSIDE "' WIDTH='100%'  BORDER='0' CELLSPACING='0' CELLPADDING='0'><TR><TD>\n"
    "     <TABLE BGCOLOR='#" HG_COL_INSIDE "' WIDTH='100%' CELLPADDING=0>\n"
    "     <TR><TD WIDTH=10>&nbsp;</TD><TD>\n"
);
if (isNotEmpty(format))
    {
    if (canReorderRemove)
	printf("<span class='sectionLiteHeader' style='cursor: default'>\n"
	       "<span id='sortHandle'>\n"
	       "<span class='ui-icon ui-icon-arrowthick-2-n-s' style='float: left;'></span>\n");
    else
	printf("<span class='sectionLiteHeader'>\n");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    if (canReorderRemove)
	{
	printf("</span></span>\n"
	       "<span style=\"float: right;\">");
	struct dyString *dy = dyStringCreate("$('#%s').hide(); event.sectionId = '%s'; "
					     "hgvaEventBubble(event);", divId, divId);
	cgiMakeButtonWithOnClick("removeMe", "Remove",
				 "remove this data source",
				 dy->string);
	dyStringFree(&dy);
	}
    printf("</span>");
    printSmallVerticalSpace(NULL);
    if (!canReorderRemove)
	// Compensate for the height that the Remove button would add:
	printSmallerVerticalSpace(NULL);
    }
}

//#*** --------------------------- verbatim from hgTables.c ---------------------------
//#*** libify into grp.h??
static struct grp *findGroup(struct grp *groupList, char *name)
/* Return named group in list, or NULL if not found. */
{
struct grp *group;
for (group = groupList; group != NULL; group = group->next)
    if (sameString(name, group->name))
        return group;
return NULL;
}
//#*** --------------------------- end verbatim from hgTables.c ---------------------------

void makeGroupDropDown(struct dyString *dy, char *selGroup)
/* Make group drop-down from fullGroupList. */
{
char *selectName = "groupSel";
dyStringPrintf(dy, "<SELECT ID='%s' NAME='%s' %s>\n", selectName, selectName, "");
if (selGroup == NULL || sameString(selGroup, "none"));
    dyStringAppend(dy, " <OPTION VALUE='none' SELECTED>\n");
struct grp *group;
for (group = fullGroupList; group != NULL; group = group->next)
    {
    dyStringPrintf(dy, " <OPTION VALUE='%s'%s>%s\n", group->name,
	    (sameOk(group->name, selGroup) ? " SELECTED" : ""), group->label);
    }
dyStringPrintf(dy, "</SELECT>\n");
}

char *makeSpecificGroupDropDown(struct dyString *dy, struct slName *groups, char *selGroup)
/* Make group drop-down with only Custom Tracks and listed groups.  If selGroup is NULL
 * but there's only one group to select from, return that group; otherwise return selGroup. */
{
char *selectName = "groupSel";
dyStringPrintf(dy, "<SELECT ID='%s' NAME='%s' %s>\n", selectName, selectName, "");
int gotCT = gotCustomTracks() ? 1 : 0;
if (!gotCT && slCount(groups) == 1)
    {
    struct grp *onlyGroup = findGroup(fullGroupList, groups->name);
    if (onlyGroup == NULL)
	errAbort("makeSpecificGroupDropDown: Can't find group '%s'", groups->name);
    dyStringPrintf(dy, " <OPTION VALUE='%s'%s>%s", onlyGroup->name,
		   sameOk(selGroup, onlyGroup->name) ? " SELECTED" : "",
		   onlyGroup->label);
    selGroup = onlyGroup->name;
    }
else
    {
    if (selGroup == NULL || !slNameInList(groups, selGroup))
	dyStringAppend(dy, " <OPTION VALUE='none' SELECTED>\n");
    if (gotCT)
	dyStringPrintf(dy, " <OPTION VALUE='user'%s>Custom Tracks\n",
		       sameOk(selGroup, "user") ? " SELECTED" : "");
    struct slName *group;
    for (group = groups;  group != NULL;  group = group->next)
	{
	struct grp *theGroup = findGroup(fullGroupList, group->name);
	// Not every db has phenDis...
	if (theGroup != NULL)
	    {
	    dyStringPrintf(dy, " <OPTION VALUE='%s'%s>%s\n", theGroup->name,
			   sameOk(selGroup, theGroup->name) ? " SELECTED" : "",
			   theGroup->label);
	    }
	}
    }
dyStringPrintf(dy, "</SELECT>\n");
return selGroup;
}

char *makeTrackDropDown(struct dyString *dy, char *selGroup, char *selTrack)
/* Make track drop-down for selGroup using fullTrackList. If selTrack is NULL,
 * return first track in selGroup; otherwise return selTrack. */
{
char *selectName = "trackSel";
dyStringPrintf(dy, "<SELECT ID='%s' NAME='%s' %s>\n", selectName, selectName, "");
boolean allTracks = sameString(selGroup, "All Tracks");
if (allTracks)
    slSort(&fullTrackList, trackDbCmpShortLabel);
struct trackDb *track = NULL;
for (track = fullTrackList; track != NULL; track = track->next)
    {
    if (allTracks || sameString(selGroup, track->grp))
	{
	dyStringPrintf(dy, " <OPTION VALUE=\"%s\"%s>%s\n", track->track,
		(sameOk(track->track, selTrack) ? " SELECTED" : ""),
		track->shortLabel);
	if (selTrack == NULL)
	    selTrack = track->track;
	}
    }
dyStringPrintf(dy, "</SELECT>\n");
return selTrack;
}

struct trackDb *findTdb(struct trackDb *tdbList, char *trackName)
/* Get trackDb for trackName. */
{
struct trackDb *tdb = NULL;
for (tdb = tdbList;  tdb != NULL;  tdb = tdb->next)
    if (sameString(tdb->track, trackName))
	return tdb;
return NULL;
}

//#*** ------------- More verbatim from hgTables.c -------------------
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
        struct consWiggle *wig, *wiggles = wigMafWiggles(database, track);
        slReverse(&wiggles);
        for (wig = wiggles; wig != NULL; wig = wig->next)
            {
            name = slNameNew(wig->table);
            slAddHead(pList, name);
            hashAdd(uniqHash, wig->table, NULL);
            }
	}
    if (track->subtracks)
        {
        struct slName *subList = NULL;
	struct slRef *tdbRefList = trackDbListGetRefsToDescendantLeaves(track->subtracks);
	slSort(&tdbRefList, trackDbRefCmp);
	struct slRef *tdbRef;
	for (tdbRef = tdbRefList; tdbRef != NULL; tdbRef = tdbRef->next)
            {
	    struct trackDb *subTdb = tdbRef->val;
	    name = slNameNew(subTdb->table);
	    slAddTail(&subList, name);
	    hashAdd(uniqHash, subTdb->table, NULL);
            }
        pList = slCat(pList, subList);
        }
    }
freez(&trackDupe);
}
//#*** ------------- End verbatim from hgTables.c -------------------

struct slName *tablesForTrack(struct trackDb *track)
/* Return list of all tables associated with track. (v.similar to hgTables.c's w/o joiner) */
{
struct hash *uniqHash = newHash(8);
struct slName *name, *nameList = NULL;
char *trackTable = track->table;
hashAdd(uniqHash, trackTable, NULL);
/* suppress for parent tracks -- only the subtracks have tables */
if (track->subtracks == NULL)
    {
    name = slNameNew(trackTable);
    slAddHead(&nameList, name);
    }
addTablesAccordingToTrackType(&nameList, uniqHash, track);
hashFree(&uniqHash);
return nameList;
}

struct trackDb *findSubtrackTdb(struct trackDb *tdb, char *name)
/* //#*** Is this already in a lib? */
{
struct trackDb *subTdb = NULL;
for (subTdb = tdb->subtracks;  subTdb != NULL;  subTdb = subTdb->next)
    if (sameString(name, subTdb->table))
	return subTdb;
return NULL;
}

void makeTableDropDown(struct dyString *dy, char *selTrack, char *selTable)
/* Make table drop-down for non-NULL selTrack. */
{
char *selectName = "tableSel";
dyStringPrintf(dy, "<SELECT ID='%s' NAME='%s' %s>\n", selectName, selectName, "");
struct trackDb *track = findTdb(fullTrackList, selTrack);
struct slName *t, *tableList = tablesForTrack(track);
for (t = tableList;  t != NULL;  t = t->next)
    {
    dyStringPrintf(dy, "<OPTION VALUE=\"%s\"%s>",
	    t->name, (sameOk(t->name, selTable) ? " SELECTED" : ""));
    struct trackDb *subtrackTdb = findSubtrackTdb(track, t->name);
    if (subtrackTdb != NULL && differentString(subtrackTdb->shortLabel, track->shortLabel))
	dyStringPrintf(dy, "%s (%s)\n", t->name, subtrackTdb->shortLabel);
    else
	dyStringPrintf(dy, "%s\n", t->name);
    }
dyStringPrintf(dy, "</SELECT>\n");
}

void makeEmptySelect(struct dyString *dy, char *idName)
/* print out an empty select, to be filled in by javascript */
{
dyStringPrintf(dy, "<SELECT name='%s' id='%s'></SELECT>\n", idName, idName);
}

void dyMakeButtonWithMsg(struct dyString *dy, char *name, char *value, char *msg)
/* Like cheapcgi's function, but on a dyString.
 * Make 'submit' type button. Display msg on mouseover, if present*/
{
dyStringPrintf(dy, "<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" %s%s%s>",
	       name, value,
	       (msg ? " TITLE=\"" : ""), (msg ? msg : ""), (msg ? "\"" : "" ));
}

void initGroupsTracksTables();
// -- defined after a bunch of verbatims below

char *buildSourceContents(struct slName *groupList, char *selGroup, char *selTrack, char *selTable,
			  boolean isPrimary)
/* Return a string with the contents of a <source>Contents div:
 * group, track, table selects and empty filter. */
{
initGroupsTracksTables();
struct dyString *dy = dyStringNew(1024);
if (isPrimary)
    dyStringPrintf(dy, "<INPUT TYPE=HIDDEN NAME='%s' VALUE='%s'>\n", "isPrimary", "true");
dyStringPrintf(dy, "<B>group:</B> ");
if (groupList != NULL)
    selGroup = makeSpecificGroupDropDown(dy, groupList, selGroup);
else
    makeGroupDropDown(dy, selGroup);
dyStringPrintf(dy, "<B>track:</B> ");
if (selGroup != NULL)
    selTrack = makeTrackDropDown(dy, selGroup, selTrack);
else
    makeEmptySelect(dy, "trackSel");
dyStringPrintf(dy, "<B>table:</B> ");
if (selTrack != NULL)
    makeTableDropDown(dy, selTrack, selTable);
else
    makeEmptySelect(dy, "tableSel");
if (slNameInList(groupList, "varRep") && !gotCustomTracks())
    dyStringPrintf(dy, "<BR>\n<EM>Note: to upload your own variants, click the "
	   "&quot;"CT_ADD_BUTTON_LABEL"&quot; button above.</EM>\n");
dyStringPrintf(dy, "<BR>\n");
printSmallerVerticalSpace(dy);
dyStringPrintf(dy, "<div id='filter'>\n");
dyStringPrintf(dy, "\n</div>\n");
if (selTrack != NULL)
    dyMakeButtonWithMsg(dy, "addFilter", "Add Filter", "add constraints on data");
return dyStringCannibalize(&dy);
}

void printDataSourceSection(char *title, char *divId, struct slName *groupList, boolean show,
			    boolean isPrimary)
/* Add section with group/track/table selects, which may be restricted to a particular
 * group and may be hidden. */
{
webNewHackSection(TRUE, divId, show, title);
printf("<div id='%sContents' "
       "onchange=\"hgvaEventBubble(event);\" onsubmit=\"hgvaOnChangeBubble(event);\""
       "onclick=\"hgvaEventBubble(event);\""
       ">\n", divId);
char *contents = buildSourceContents(groupList, NULL, NULL, NULL, isPrimary);
puts(contents);
printf("</div>\n");
freeMem(contents);
}

void printAddDataSection()
/* Print a very lightweight section that just has a "Select More Data" button. */
{
webNewHackSection(FALSE , "addData", TRUE, "");
printSmallerVerticalSpace(NULL);
cgiMakeButtonWithOnClick("addData", "Select More Data",
			 "select a new track to integrate with Variants",
			 "hgvaShowNextHiddenSource();");
printSmallVerticalSpace(NULL);
}

void makeOutputFormatDropDown(char *selectName)
/* placeholder for now... the real choices will depend on inputs */
{
char *menu[] = { "Tab-separated text", "Something else" };
char *values[] = { "tabSep", "other" };
cgiMakeDropListWithVals(selectName, menu, values, ArraySize(menu), "");
}

void printOutputSection()
/* Print an output format section that can't be removed like data source sections can. */
{
webNewHackSection(FALSE, "outFormat", TRUE, "Select Output Format");
printf("<div id='outFormatContents' "
       "onchange=\"hgvaEventBubble(event);\" onclick=\"hgvaEventBubble(event);\">\n");
makeOutputFormatDropDown("outFormat");
printf("<BR>\n");
printf("<div id='outOptions'>\n");
printSmallerVerticalSpace(NULL);
cgiMakeButtonWithMsg("outSelectFields", "Select fields",
		     "select particular fields from inputs");
printf("</div>\n"); // options
printf("</div>\n"); // contents
}

void printSubmitSection()
/* Print an output format section that can't be removed like data source sections can. */
{
webNewHackSection(FALSE, "submitSection", TRUE, "Get Results");
cgiMakeButtonWithOnClick("startQuery", "Go!",
			 "get the results of your query",
			 "hgvaExecuteQuery();");
}

//#*** --------------------- verbatim from hgTables/wikiTrack.c -------------------------
//#*** libify to hg/lib/wikiTrack.c
//#*** should it sort trackDb, or leave that to the caller?
void wikiTrackDb(struct trackDb **list)
/* create a trackDb entry for the wiki track */
{
struct trackDb *tdb;

AllocVar(tdb);
tdb->track = WIKI_TRACK_TABLE;
tdb->table = WIKI_TRACK_TABLE;
tdb->shortLabel = WIKI_TRACK_LABEL;
tdb->longLabel = WIKI_TRACK_LONGLABEL;
tdb->visibility = tvFull;
tdb->priority = WIKI_TRACK_PRIORITY;

tdb->html = hFileContentsOrWarning(hHelpFile(tdb->track));
tdb->type = "none";
tdb->grp = "map";
tdb->canPack = FALSE;

slAddHead(list, tdb);
slSort(list, trackDbCmp);
}
//#*** --------------------- end verbatim from hgTables/wikiTrack.c -------------------------


//#*** --------------------- verbatim from hgTables/custom.c (+ globals) -------------------------
struct customTrack *getCustomTracks()
/* Get custom track list. */
{
//fprintf(stdout,"database %s in cart %s", database, cartString(cart, "db"));
cartSetString(cart, "db", database);
if (theCtList == NULL)
    theCtList = customTracksParseCart(database, cart, &browserLines, NULL);
return(theCtList);
}
//#*** --------------------- end verbatim from hgTables/custom.c -------------------------


//#*** --------------------- verbatim from hgTables.c (+ globals) -------------------------
struct grp *grpFromHub(struct hubConnectStatus *hub)
/* Make up a grp structur from hub */
{
struct grp *grp;
AllocVar(grp);
char name[16];
safef(name, sizeof(name), "hub_%d", hub->id);
grp->name = cloneString(name);
grp->label = cloneString(hub->trackHub->shortLabel);
return grp;
}

static struct trackDb *getFullTrackList(struct hubConnectStatus *hubList, struct grp **pHubGroups)
/* Get all tracks including custom tracks if any. */
{
struct trackDb *list = hTrackDb(database);
struct customTrack *ctList, *ct;

/* exclude any track with a 'tableBrowser off' setting */
struct trackDb *tdb, *nextTdb, *newList = NULL;
for (tdb = list;  tdb != NULL;  tdb = nextTdb)
    {
    nextTdb = tdb->next;
    if (tdbIsDownloadsOnly(tdb) || tdb->table == NULL)
        {
        //freeMem(tdb);  // should not free tdb's.
        // While hdb.c should and says it does cache the tdbList, it doesn't.
        // The most notable reason that the tdbs are not cached is this hgTables CGI !!!
        // It needs to be rewritten to make tdbRef structures for the lists it creates here!
        continue;
        }

    char *tbOff = trackDbSetting(tdb, "tableBrowser");
    if (tbOff != NULL && startsWithWord("off", tbOff))
	slAddHead(&forbiddenTrackList, tdb);
    else
	slAddHead(&newList, tdb);
    }
slReverse(&newList);
list = newList;

/* add wikiTrack if enabled */
if (wikiTrackEnabled(database, NULL))
    wikiTrackDb(&list);

/* Add hub tracks. */
struct hubConnectStatus *hubStatus;
for (hubStatus = hubList; hubStatus != NULL; hubStatus = hubStatus->next)
    {
    /* Load trackDb.ra file and make it into proper trackDb tree */
    char hubName[64];
    safef(hubName, sizeof(hubName), "hub_%d", hubStatus->id);

    struct trackHub *hub = hubStatus->trackHub;
    if (hub != NULL)
	{
	hub->name = cloneString(hubName);
	struct trackHubGenome *hubGenome = trackHubFindGenome(hub, database);
	if (hubGenome != NULL)
	    {
	    struct trackDb *tdbList = trackHubTracksForGenome(hub, hubGenome);
	    tdbList = trackDbLinkUpGenerations(tdbList);
	    tdbList = trackDbPolishAfterLinkup(tdbList, database);
	    trackDbPrioritizeContainerItems(tdbList);
	    if (tdbList != NULL)
		{
		list = slCat(list, tdbList);
		struct grp *grp = grpFromHub(hubStatus);
		slAddHead(pHubGroups, grp);
		}
	    }

	// clear this so it isn't free'd later
	hubStatus->trackHub = NULL;
	}
    }
slReverse(pHubGroups);

/* Create dummy group for custom tracks if any. Add custom tracks to list */
ctList = getCustomTracks();
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    slAddHead(&list, ct->tdb);
    }

return list;
}

struct grp *makeGroupList(struct trackDb *trackList, struct grp **pHubGrpList, boolean allTablesOk)
/* Get list of groups that actually have something in them. */
{
struct grp *groupsAll, *groupList = NULL, *group;
struct hash *groupsInTrackList = newHash(0);
struct hash *groupsInDatabase = newHash(0);
struct trackDb *track;

/* Stream through track list building up hash of active groups. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInTrackList,track->grp))
        hashAdd(groupsInTrackList, track->grp, NULL);
    }

/* Scan through group table, putting in ones where we have data. */
groupsAll = hLoadGrps(database);
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

/* if we have custom tracks, we want to add the track hubs
 * after that group */
struct grp *addAfter = NULL;
if (sameString(groupList->name, "user"))
    addAfter = groupList;

/* Add in groups from hubs. */
for (group = slPopHead(pHubGrpList); group != NULL; group = slPopHead(pHubGrpList))
    {
    /* check to see if we're inserting hubs rather than
     * adding them to the front of the list */
    if (addAfter != NULL)
	{
	group->next = addAfter->next;
	addAfter->next = group;
	}
    else
	slAddHead(&groupList, group);
    hashAdd(groupsInDatabase, group->name, group);
    }

/* Do some error checking for tracks with group names that are
 * not in database.  Just warn about them. */
for (track = trackList; track != NULL; track = track->next)
    {
    if (!hashLookup(groupsInDatabase, track->grp))
         warn("Track %s has group %s, which isn't in grp table",
	      track->table, track->grp);
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
//#*** --------------------- end verbatim from hgTables.c -------------------------

void initGroupsTracksTables()
/* Get list of groups that actually have something in them, prepare hashes
 * containing all tracks and all tables. Set global variables that correspond
 * to the group, track, and table specified in the cart. */
{
static boolean inited = FALSE;
if (! inited)
    {
    struct hubConnectStatus *hubList = hubConnectStatusListFromCart(cart);
    struct grp *hubGrpList = NULL;
    fullTrackList = getFullTrackList(hubList, &hubGrpList);
    fullGroupList = makeGroupList(fullTrackList, &hubGrpList, FALSE);
    inited = TRUE;
    }
}

struct slName *variantsGroupList()
/* Return the restricted group list for the Variants section. */
{
static struct slName *list = NULL;
if (list == NULL)
    {
    list = slNameNew("phenDis");
    slAddHead(&list, slNameNew("varRep"));
    }
return list;
}

struct slName *genesGroupList()
/* Return the restricted group list for the Genes section. */
{
static struct slName *list = NULL;
if (list == NULL)
    list = slNameNew("genes");
return list;
}

void doMainPage()
/* Print out initial HTML of control page. */
{
htmlOpen("Variant Annotation Integrator");
jsInit();
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);
webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("utils.js", NULL);
jsIncludeFile("ajax.js", NULL);
jsIncludeFile("hgVarAnnogrator.js", NULL);
addSomeCss();
printAssemblySection();
webEndHackSection();
printf("<div id='sourceContainerPlus'>\n");
printf("<div id='sourceContainer'>\n");
printDataSourceSection("Select Variants", "variants", variantsGroupList(), TRUE, TRUE);
printDataSourceSection("Select Genes", "genes", genesGroupList(), TRUE, FALSE);
int maxGrators = 5, i;
for (i = 0;  i < maxGrators;  i++)
    {
    char id[32];
    safef(id, sizeof(id), "source%d", i);
    printDataSourceSection("Select Data", id, NULL, FALSE, FALSE);
    }
webEndHackSection();
printf("</div>\n"); // sourceContainer
printAddDataSection();
webEndHackSection();
printf("</div>\n"); // sourceContainerPlus (extend down a bit so sections can be dragged to bottom)
printOutputSection();
printSubmitSection();

printf("<script>\n"
       "$(function() { "
       "$('#sourceContainer' ).sortable({ "
       "  containment: '#sourceContainerPlus',"
       "  handle: '#sortHandle',"
       "  update: hgvaSourceSortUpdate"
       "}); });\n"
       "</script>\n");

//#*** ------------------ more verbatim from mainPage.c ---------------
/* Hidden form for jumping to custom tracks CGI. */
hPrintf("<FORM ACTION='%s' NAME='customTrackForm'>", hgCustomName());
cartSaveSession(cart);
hPrintf("</FORM>\n");

/* Hidden form for jumping to track hub manager CGI. */
hPrintf("<FORM ACTION='%s' NAME='trackHubForm'>", hgHubConnectName());
cgiMakeHiddenVar(hgHubConnectCgiDestUrl, "../cgi-bin/hgTables");
cartSaveSession(cart);
hPrintf("</FORM>\n");
//#*** ------------------ end verbatim ---------------

webNewSection("Using the Variant Annotation Integrator");
printf("Documentation goes here :)\n");

htmlClose();
}

void dispatch()
/* Do something. */
{
doMainPage();
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "updatePage", NULL,};

void hgVarAnnogrator()
/* Initialize state, dispatch command and clean up. */
{
char *clade = NULL;

oldVars = hashNew(10);

/* Sometimes we output HTML and sometimes plain text; let each outputter
 * take care of headers instead of using a fixed cart*Shell(). */
cart = cartAndCookieNoContent(hUserCookie(), excludeVars, oldVars);

/* Set up global variables. */
getDbGenomeClade(cart, &database, &genome, &clade, oldVars);
freezeName = hFreezeFromDb(database);

setUdcCacheDir();
int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
#if ((defined USE_BAM || defined USE_TABIX) && defined KNETFILE_HOOKS)
knetUdcInstall();
#endif//def (USE_BAM || USE_TABIX) && KNETFILE_HOOKS

if (lookupPosition())
    {
    dispatch();
    }
/* Save variables. */
cartCheckout(&cart);
textOutClose(&compressPipeline);
}

// #*** -------------------------- libify to jsHelper ? ------------------------

void expectJsonType(struct jsonElement *el, jsonElementType type, char *desc)
/* Die if el is NULL or its type is not as expected. */
{
if (el == NULL)
    errAbort("expected %s (type %d) but got NULL", desc, type);
if (el->type != type)
    errAbort("expected %s to have type %d but got type %d", desc, type, el->type);
}

char *stringFromJHash(struct hash *jHash, char *elName, boolean nullOk)
/* Look up the jsonElement with elName in hash, make sure the element's type is jsonString,
 * and return its actual string.  If nullOK, return NULL when elName is not found. */
{
struct hashEl *hel = hashLookup(jHash, elName);
if (hel == NULL && nullOk)
    return NULL;
expectJsonType((struct jsonElement *)hel->val, jsonString, elName);
return ((struct jsonStringElement *)hel->val)->str;
}

struct slRef *listFromJHash(struct hash *jHash, char *elName, boolean nullOk)
/* Look up the jsonElement with elName in hash, make sure the element's type is jsonList,
 * and return its actual list.  If nullOK, return NULL when elName is not found. */
{
struct hashEl *hel = hashLookup(jHash, elName);
if (hel == NULL && nullOk)
    return NULL;
expectJsonType((struct jsonElement *)hel->val, jsonList, elName);
return ((struct jsonListElement *)hel->val)->list;
}

struct hash *hashFromJHash(struct hash *jHash, char *elName, boolean nullOk)
/* Look up the jsonElement with elName in jHash, make sure the element's type is jsonHash,
 * and return its actual hash.  If nullOK, return NULL when elName is not found. */
{
struct hashEl *hel = hashLookup(jHash, elName);
if (hel == NULL && nullOk)
    return NULL;
expectJsonType((struct jsonElement *)hel->val, jsonHash, elName);
return ((struct jsonHashElement *)hel->val)->hash;
}

// #*** -------------------------- end maybe libify to jsHelper ------------------------

struct slName *groupListForSource(char *divId)
/* If applicable, return the restricted group list for the given section. */
{
struct slName *groupList = NULL;
if (sameString(divId, "variantsContents"))
    groupList = variantsGroupList();
else if (sameString(divId, "genesContents"))
    groupList = genesGroupList();
return groupList;
}

char *escapeStringForJson(char *input)
/* \-escape newlines and double-quotes in string to be passed via JSON. */
{
char *escNewlines = replaceChars(input, "\n", "\\n");
char *output = replaceChars(escNewlines, "\"", "\\\"");
freeMem(escNewlines);
return output;
}


// what changed: group, track or table?
enum gtt { gttGroup, gttTrack, gttTable};

void changeGroupTrackTable(char *divId, struct hash *querySpec, enum gtt groupTrackOrTable)
/* Send new contents for the section: same group select, updated track & table selects,
 * reset filter section. */
{
boolean isPrimary = FALSE;
printf("{ \"updates\": [ { \"id\": \"#%s\", \"contents\": \"", divId);
struct slRef *srcRef, *sources = listFromJHash(querySpec, "sources", FALSE);
char *selGroup = NULL, *selTrack = NULL, *selTable = NULL;
for (srcRef = sources;  srcRef != NULL;  srcRef = srcRef->next)
    {
    struct jsonElement *srcJson = srcRef->val;
    expectJsonType(srcJson, jsonHash, "source object");
    struct hash *srcHash = ((struct jsonHashElement *)srcJson)->hash;
    char *srcId = stringFromJHash(srcHash, "id", FALSE);
    if (sameString(srcId, divId))
	{
	selGroup = stringFromJHash(srcHash, "groupSel", FALSE);
	selTrack = stringFromJHash(srcHash, "trackSel", FALSE);
	selTable = stringFromJHash(srcHash, "tableSel", FALSE);
	if (srcRef == sources)
	    isPrimary = TRUE;
	break;
	}
    }
if (selGroup == NULL)
    errAbort("Can't find source '%s'", divId);
if (groupTrackOrTable == gttGroup)
    selTrack = selTable = NULL;
else if (groupTrackOrTable == gttTrack)
    selTable = NULL;
else if (groupTrackOrTable != gttTable)
    errAbort("Unexpected groupTrackOrTable enum val %d", groupTrackOrTable);
struct slName *groupList = groupListForSource(divId);
char *newContents = buildSourceContents(groupList, selGroup, selTrack, NULL, isPrimary);
printf("%s\" } ]", escapeStringForJson(newContents));
printf(" }\n");
}

void showFilter(char *divId, struct hash *querySpec)
/* Send new contents for div's filter section. */
{
printf("{ \"updates\": [ { \"id\": \"#%s #filter\", \"append\": true, "
       "\"contents\": \"", divId);
printf("a new filter!<BR>");
puts("\" } ] }");
}

void showOutSelectFields(char *divId, struct hash *querySpec)
/* Send new contents for divId's field selection section. */
{
printf("{ \"updates\": [ { \"id\": \"#%s #outOptions\", \"contents\": \"", divId);
printf("a whole bunch of checkboxes<BR>");
puts("\" } ] }");
}

void resetRemovedSource(char *removedSource)
/* Reset the contents of the newly "removed" (hidden) source, so if we bring it back
 * it will look new. */
{
if (isNotEmpty(removedSource))
    {
    printf("{ \"updates\": [ { \"id\": \"#%s\", \"contents\": \"", removedSource);
    struct slName *groupList = groupListForSource(removedSource);
    char *newContents = buildSourceContents(groupList, NULL, NULL, NULL, FALSE);
    printf("%s\" } ]", escapeStringForJson(newContents));
    printf(" }\n");
    }
}

void updateSourcesAndOutput(struct hash *querySpec)
/* The number and/or order of sources has changed.  If primary source has changed,
 * then filter settings of the former and new primaries need to be adjusted.
 * If the number of sources has changed, we might end up with more or fewer
 * choices of output format.  Send JSON to server with new HTML for changed sections. */
{
struct dyString *dy = dyStringNew(512);
struct slRef *srcRef, *sources = listFromJHash(querySpec, "sources", FALSE);
boolean gotUpdate = FALSE;
for (srcRef = sources;  srcRef != NULL;  srcRef = srcRef->next)
    {
    struct jsonElement *srcJson = srcRef->val;
    expectJsonType(srcJson, jsonHash, "source object");
    struct hash *srcHash = ((struct jsonHashElement *)srcJson)->hash;
    char *srcId = stringFromJHash(srcHash, "id", FALSE);
    boolean isPrimary = (srcRef == sources);
    boolean srcThinksItsPrimary = isNotEmpty(stringFromJHash(srcHash, "isPrimary", TRUE));
//#*** this is necessary only if it has filters... and we could simply update the filter div!
    if (srcThinksItsPrimary ^ isPrimary)
	{
	if (!gotUpdate)
	    printf("{ \"updates\": [ ");
	else
	    printf(", ");
	gotUpdate = TRUE;
	printf("{ \"id\": \"#%s\", \"contents\": \"", srcId);
	struct slName *groupList = groupListForSource(srcId);
	char *newContents = buildSourceContents(groupList, NULL, NULL, NULL, isPrimary);
	printf("%s\" }", escapeStringForJson(newContents));
	dyStringPrintf(dy, "Telling %s that it is%s primary.  ", srcId, isPrimary ? "" : " not");
	}
    }
if (gotUpdate)
    printf(" ], ");
else
    printf("{ ");
dyStringAppend(dy, "Still need to compute possible output format choices from input types.");
printf("\"serverSays\": \"%s\" }\n", dy->string);
}

void doAjax(char *jsonText)
/* Pick apart JSON request, send back HTML for page sections that need to change. */
{
puts("Content-Type:text/javascript\n");

// Parse jsonText and make sure that it is a hash:
struct jsonElement *request = jsonParse(jsonText);
expectJsonType(request, jsonHash, "top-level request");
struct hash *topHash = ((struct jsonHashElement *)request)->hash;
// Every request must include a querySpec:
struct hash *querySpec = hashFromJHash(topHash, "querySpec", FALSE);

// topHash must contain db and may contain other variables useful for making
// a minimal cart (so initGroupsTracksTables can see user's custom tracks):
database = stringFromJHash(topHash, "db", FALSE);
cart = cartOfNothing();
char *hgsid = stringFromJHash(topHash, "hgsid", TRUE);
if (isNotEmpty(hgsid))
    cartSetString(cart, "hgsid", hgsid);
char *ctfile = stringFromJHash(topHash, "ctfile", TRUE);
if (isNotEmpty(ctfile))
    cartSetStringDb(cart, database, "ctfile", ctfile);
//#*** need to do the same for hub cart vars

char *action = stringFromJHash(topHash, "action", FALSE);
if (sameString(action, "reorderSources"))
    updateSourcesAndOutput(querySpec);
else if (sameString(action, "event"))
    {
    // Determine where this event originated:
    char *ancestor = stringFromJHash(topHash, "ancestor", TRUE);
    char *id = stringFromJHash(topHash, "id", TRUE);
    char *name = stringFromJHash(topHash, "name", TRUE);
    if (isEmpty(id))
	id = name;
    if (isEmpty(id))
	errAbort("request must contain id and/or name, but has neither.");
    if (sameString(id, "removeMe"))
	resetRemovedSource(ancestor);
    else if (sameString(id, "groupSel"))
	changeGroupTrackTable(ancestor, querySpec, gttGroup);
     else if (sameString(id, "trackSel"))
	 changeGroupTrackTable(ancestor, querySpec, gttTrack);
    else if (sameString(id, "tableSel"))
	changeGroupTrackTable(ancestor, querySpec, gttTable);
    else if (sameString(id, "addFilter"))
	showFilter(ancestor, querySpec);
    else if (sameString(id, "outSelectFields"))
	showOutSelectFields(ancestor, querySpec);
    else if (endsWith(ancestor, "Contents"))
	printf("{ \"serverSays\": \"Some new input '%s' I need to handle in %s\" }\n",
	       id, ancestor);
    else
	printf("{ \"serverSays\": \"What is '%s' from %s?\" }\n", id, ancestor);
    }
else if (sameString(action, "execute"))
    printf("{ \"serverSays\": \"Execute should really be a submit event not AJAX, right??\" }\n");
else
    printf("{ \"serverSays\": \"Unrecognized action '%s'\" }\n", action);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(LIMIT_2or6GB);
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
cgiSpoof(&argc, argv);
char *jsonIn = cgiUsualString("updatePage", NULL);
if (jsonIn != NULL)
    doAjax(jsonIn);
else
    hgVarAnnogrator();
return 0;
}
