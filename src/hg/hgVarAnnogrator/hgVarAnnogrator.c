/* hgVarAnnogrator - User interface for variant annotation integrator tool. */
#include "common.h"
#include "asParse.h"
#include "linefile.h"
#include "hash.h"
#include "memalloc.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "hPrint.h"
#include "hdb.h"
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
#include "genePred.h"
#include "hgMaf.h"
#if ((defined USE_BAM || defined USE_TABIX) && defined KNETFILE_HOOKS)
#include "knetUdc.h"
#endif//def (USE_BAM || USE_TABIX) && KNETFILE_HOOKS
#include "annoGratorQuery.h"
#include "annoStreamDb.h"
#include "annoStreamVcf.h"
#include "annoStreamWig.h"
#include "annoGrateWigDb.h"
#include "annoGratorGpVar.h"
#include "annoFormatTab.h"

/* Global Variables */
struct cart *cart;             /* CGI and other variables */
struct hash *oldVars;	/* The cart before new cgi stuff added. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *database;		/* Current genome database - hg17, mm5, etc. */
char *regionType;	/* genome, ENCODE pilot regions, or specific position range. */
char *position;		/* position range (if applicable) */
static struct pipeline *compressPipeline = (struct pipeline *)NULL;
struct grp *fullGroupList;	/* List of all groups. */
struct trackDb *fullTrackList;	/* List of all tracks in database. */
struct customTrack *theCtList = NULL;	/* List of custom tracks. */
struct slName *browserLines = NULL;	/* Browser lines in custom tracks. */

int maxOutRows = 10000;  //#*** make sensible, configurable limit

#define hgvaRange "position"
#define hgvaRegionType "hgva_regionType"
#define hgvaRegionTypeEncode "encode"
#define hgvaRegionTypeGenome "genome"
#define hgvaRegionTypeRange "range"
#define hgvaPositionContainer "positionContainer"

void addSomeCss()
/* This should go in a .css file of course. */
{
hPrintf("<style>\n"
	"div.sectionLite { border-width: 1px; border-color: #"HG_COL_BORDER"; border-style: solid;"
	"  background-color: #"HG_COL_INSIDE"; padding-left: 10px; padding-right: 10px; "
	"  padding-top: 8px; padding-bottom: 5px; margin-top: 5px; margin-bottom: 5px }\n"
	".sectionLiteHeader { font-weight: bold; font-size:1.1em; color:#000000;"
	"  text-align:left; vertical-align:bottom; white-space:nowrap; }\n"
	"div.sectionLiteHeader.noReorderRemove { padding-bottom:5px; }\n"
	"div.sourceFilter { padding-top: 5px; padding-bottom: 5px }\n"
	"</style>\n");
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
struct jsonElement *el = hel ? hel->val : NULL;
expectJsonType(el, jsonString, elName);
return el->val.jeString;
}

struct slRef *listFromJHash(struct hash *jHash, char *elName, boolean nullOk)
/* Look up the jsonElement with elName in hash, make sure the element's type is jsonList,
 * and return its actual list.  If nullOK, return NULL when elName is not found. */
{
struct hashEl *hel = hashLookup(jHash, elName);
if (hel == NULL && nullOk)
    return NULL;
struct jsonElement *el = hel ? hel->val : NULL;
expectJsonType(el, jsonList, elName);
return el->val.jeList;
}

struct hash *hashFromJEl(struct jsonElement *jel, char *desc, boolean nullOk)
/* Make sure jel's type is jsonObject and return its actual hash.  If nullOK, return
 * NULL when elName is not found. */
{
expectJsonType(jel, jsonObject, desc);
return jel->val.jeHash;
}

struct hash *hashFromJHash(struct hash *jHash, char *elName, boolean nullOk)
/* Look up the jsonElement with elName in jHash, make sure the element's type is jsonObject,
 * and return its actual hash.  If nullOK, return NULL when elName is not found. */
{
struct hashEl *hel = hashLookup(jHash, elName);
if (hel == NULL && nullOk)
    return NULL;
struct jsonElement *el = hel ? hel->val : NULL;
return hashFromJEl(el, elName, nullOk);
}

struct slPair *stringsWithPrefixFromJHash(struct hash *jHash, char *prefix)
/* Search jHash elements for string variables whose names start with prefix. */
{
struct slPair *varList = NULL;
struct hashCookie cookie = hashFirst(jHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (startsWith(prefix, hel->name))
	{
	struct jsonElement *el = hel->val;
	if (el->type == jsonString)
	    slAddHead(&varList, slPairNew(hel->name, el->val.jeString));
	}
    }
return varList;
}


// #*** -------------------------- end maybe libify to jsHelper ------------------------
//#*** --------------- begin verbatim from hgTables.c -- libify ------------------------

char *getScriptName()
/* returns script name from environment or hardcoded for command line */
//#*** This should be libified and used for all the places where one CGI needs another
//#*** to return to it.
{
char *script = cgiScriptName();
if (script != NULL)
    return script;
else
    return hgVarAnnogratorName();
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

void saveMiniCart()
/* Store cart variables necessary for executing queries here, so javascript can
 * include them in AJAX calls and we can retrieve them in restoreMiniCart() below. */
{
cartSaveSession(cart);
char *ctfile = cartOptionalStringDb(cart, database, "ctfile");
if (isNotEmpty(ctfile))
    cgiMakeHiddenVar("ctfile", ctfile);
struct slPair *hubVar, *hubVarList = cartVarsWithPrefix(cart, hgHubConnectHubVarPrefix);
for (hubVar = hubVarList; hubVar != NULL;  hubVar = hubVar->next)
    if (cartBoolean(cart, hubVar->name))
	cgiMakeHiddenVar(hubVar->name, hubVar->val);
char *trackHubs = cartOptionalString(cart, hubConnectTrackHubsVarName);
if (isNotEmpty(trackHubs))
    cgiMakeHiddenVar(hubConnectTrackHubsVarName, trackHubs);
}

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

//#*** perhaps this fancy onChange stuff should be done in hgVarAnnogrator.js?
//#*** and do we really need a hidden form?

static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = jsOnChangeStart();
jsTextCarryOver(dy, hgvaRegionType);
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

INLINE void printOption(char *val, char *selectedVal, char *label)
/* For rolling our own select without having to build conditional arrays/lists. */
{
printf("<OPTION VALUE='%s'%s>%s\n", val, (sameString(selectedVal, val) ? " SELECTED" : ""), label);
}

void printRegionListHtml(char *db)
/* Make a dropdown choice of region type, with position input box that appears if needed.
 * Return the selected region type. */
{
printf("<SELECT ID='"hgvaRegionType"' NAME='"hgvaRegionType"' "
       "onchange=\"hgva.changeRegion();\">\n");
struct sqlConnection *conn = hAllocConn(db);
boolean doEncode = sqlTableExists(conn, "encodeRegions");
hFreeConn(&conn);
// If regionType not allowed force it to "genome".
if (sameString(regionType, hgvaRegionTypeEncode) && !doEncode)
    regionType = hgvaRegionTypeGenome;
printOption(hgvaRegionTypeGenome, regionType, "genome");
if (doEncode)
    printOption(hgvaRegionTypeEncode, regionType, "ENCODE Pilot regions");
printOption(hgvaRegionTypeRange, regionType, "position or search term");
printf("</SELECT>");
}

void topLabelSpansStart(char *label)
{
printf("<span style='display: inline-block; padding-right: 5px;'>"
       "<span style='display: block;'>%s</span>\n"
       "<span style='display: block; padding-bottom: 5px;'>\n", label);
}

void topLabelSpansEnd()
{
printf("</span></span>");
}

char *makePositionInput()
/* Return HTML for the position input. */
{
struct dyString *dy = dyStringCreate("<INPUT TYPE=TEXT NAME=\"%s\" SIZE=%d VALUE=\"%s\""
				     " onchange=\"hgva.lookupPosition();\">",
				     hgvaRange, 26, addCommasToPos(NULL, position));
return dyStringCannibalize(&dy);
}

void hgGatewayCladeGenomeDb()
/* Make a row of labels and row of buttons like hgGateway, but not using tables. */
{
boolean gotClade = hGotClade();
if (gotClade)
    {
    topLabelSpansStart("clade");
    printCladeListHtml(genome, onChangeClade());
    topLabelSpansEnd();
    }
topLabelSpansStart("genome");
if (gotClade)
    printGenomeListForCladeHtml(database, onChangeOrg());
else
    printGenomeListHtml(database, onChangeOrg());
topLabelSpansEnd();
topLabelSpansStart("assembly");
printAssemblyListHtml(database, onChangeDb());
topLabelSpansEnd();
puts("<BR>");
topLabelSpansStart("region");
printRegionListHtml(database);
topLabelSpansEnd();
topLabelSpansStart("");
// Yet another span, for hiding/showing position input and lookup button:
printf("<span id='"hgvaPositionContainer"'%s>\n",
       differentString(regionType, hgvaRegionTypeRange) ? " style='display: none;'" : "");
puts(makePositionInput());
printf("</span>\n");
topLabelSpansEnd();
puts("<div style='padding-top: 5px; padding-bottom: 5px'>");
hOnClickButton("document.customTrackForm.submit(); return false;",
	       gotCustomTracks() ? CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL);
hPrintf(" ");
if (hubConnectTableExists())
    hOnClickButton("document.trackHubForm.submit(); return false;", "track hubs");
nbSpaces(3);
hPrintf("To reset <B>all</B> user cart settings (including custom tracks), \n"
	"<A HREF=\"/cgi-bin/cartReset?destination=%s\">click here</A>.\n",
	getScriptName());
puts("</div>");
}

void printAssemblySection()
/* Print assembly-selection stuff.
 * Redrawing the whole page when the assembly changes seems fine to me. */
{
//#*** ---------- More copied verbatim, from hgTables/mainPage.c: -----------
/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      "clade", "org", "db", hgvaRange, hgvaRegionType };
    jsCreateHiddenForm(cart, getScriptName(), saveVars, ArraySize(saveVars));
    }

hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" ID=\"mainForm\" METHOD=%s>\n",
	getScriptName(), cartUsualString(cart, "formMethod", "POST"));
//#*** ------------------ end verbatim ---------------

saveMiniCart();

printf("<div class='sectionLiteHeader noReorderRemove'>Select Genome Assembly</div>\n");

/* Print clade, genome and assembly line. */
hgGatewayCladeGenomeDb();

hPrintf("</FORM>");
}

static boolean inOldSection = TRUE;
static boolean inSectionLite = FALSE;

void webEndOldSection()
{
//#*** this statement copied from web.c's static void webEndSection... maybe just expose that in  web.h!
puts(
    "" "\n"
    "	</TD><TD WIDTH=15></TD></TR></TABLE>" "\n"
    "	</TD></TR></TABLE>" "\n"
    "	</TD></TR></TABLE>" "\n"
    "	" );
puts("</div>\n");
inOldSection = FALSE;
}

void webEndSectionLite()
{
puts("</div>\n");
inSectionLite = FALSE;
}

void webNewSectionLite(boolean canReorderRemove, char *divId, boolean show, char* format, ...)
/* create a new section on the web page -- probably should do div w/css style, but this is quicker */
{
if (inOldSection)
    webEndOldSection();
if (inSectionLite)
    webEndSectionLite();
inSectionLite = TRUE;
puts("<!-- +++++++++++++++++++++ START NEW SECTION +++++++++++++++++++ -->");
printf("<div id='%s' class='sectionLite%s'", divId, (canReorderRemove ? " hideableSection" : ""));
if (!show)
    printf(" style='display: none;'");
printf(">\n");

if (isNotEmpty(format))
    {
    if (canReorderRemove)
	printf("<div class='sectionLiteHeader'>\n"
	       "<span name='sortHandle' style='float: left;'>\n"
	       "<span class='ui-icon ui-icon-arrowthick-2-n-s' style='float: left;'></span>\n");
    else
	puts("<div class='sectionLiteHeader noReorderRemove'>");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    if (canReorderRemove)
	{
	printf("</span>\n"
	       "<span style=\"float: right;\">");
	cgiMakeButtonWithMsg("removeMe", "Remove", "remove this section");
	puts("</span>\n"
	     "<div style='clear: both; '></div>");
	}
    puts("</div>");
    }
}

struct slName *variantsGroupList()
/* Return the restricted group list for the Variants section. */
{
static struct slName *list = NULL;
if (list == NULL)
    {
    list = slNameNew("phenDis");
    slAddHead(&list, slNameNew("phenoAllele"));
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

//-----------------------------------------------------------------------------
// Configuration data structures parsed from JSON sent by UI and/or stored in cart

struct sourceConfig
/* User settings for input data, from a div/section of the UI */
    {
    struct sourceConfig *next;
    char *name;			// Name of section div, e.g. "variants", "genes", "source0"
    struct slName *groupList;	// If not NULL, restricted list of track groups for dropdown
    char *selGroup;		// Currently selected track group
    char *selTrack;		// Currently selected track
    char *selTable;		// Currently selected table
    char *selIntersect;		// Currently selected intersection mode
    struct annoFilter *filterList;	// Active (i.e. configured by user) filters
    boolean isPrimary;		// True only for first source in list
    boolean thinksItsPrimary;	// UI state
    };

struct formatterConfig
/* User settings for output data, from output div/section of the UI */
    {
    struct formatterConfig *next;
    char *outFormat;		// Name of output format, e.g. "tabSep"
    };

struct queryConfig
/* Complete description of query, built from UI elements */
    {
    struct sourceConfig *sources;	// Inputs
    struct formatterConfig *formatters;	// Outputs
    };

void *parseNumbers(enum asTypes type, char *numStr1, char *numStr2)
/* Given one or two numbers from UI, parse & store them in an array according to type. */
{
void *ret = NULL;
if (asTypesIsFloating(type))
    {
    double *retDouble = needMem(2 * sizeof(double *));
    retDouble[0] = atof(numStr1);
    if (numStr2 != NULL)
	retDouble[1] = atof(numStr2);
    ret = retDouble;
    }
else if (asTypesIsInt(type))
    {
    long long *retLl = needMem(2 * sizeof(long long));
    retLl[0] = atoll(numStr1);
    if (numStr2 != NULL)
	retLl[1] = atoll(numStr2);
    ret = retLl;
    }
return ret;
}

struct annoFilter *parseFilters(struct slRef *filterRefList)
/* Translate a hash of JSON elements that describe a single data source's filter settings
 * into a partial annoFilter. */
{
struct annoFilter *afList = NULL;
struct slRef *ref;
for (ref = filterRefList;  ref != NULL;  ref = ref->next)
    {
    struct hash *fHash = hashFromJEl(ref->val, "filter", FALSE);
    struct annoFilter *newFilter;
    AllocVar(newFilter);
    newFilter->label = stringFromJHash(fHash, "name", FALSE);
    newFilter->columnIx = -1;  // we don't have or use filterFunc or columnIx in UI
    newFilter->op = afOpFromString(stringFromJHash(fHash, "opSel", FALSE));
    enum asTypes type = atoi(stringFromJHash(fHash, "asType", FALSE)); //#*** should use enum<-->strings
    newFilter->type = type;
    if (newFilter->op == afInRange)
	{
	char *numStr1 = stringFromJHash(fHash, "num1", FALSE);
	char *numStr2 = stringFromJHash(fHash, "num2", FALSE);
	newFilter->values = parseNumbers(type, numStr1, numStr2);
	}
    else if (newFilter->op == afMatch || newFilter->op == afNotMatch)
	newFilter->values = stringFromJHash(fHash, "matchPattern", FALSE);
    else if (newFilter->op != afNoFilter)
	newFilter->values = parseNumbers(type, stringFromJHash(fHash, "num1", FALSE), NULL);
    slAddHead(&afList, newFilter);
    }
slReverse(&afList);
return afList;
}

struct sourceConfig *parseSourceConfig(struct hash *srcHash, boolean isPrimary)
/* Translate a hash of JSON elements that describe a single data source. */
{
struct sourceConfig *newSrc;
AllocVar(newSrc);
newSrc->name = stringFromJHash(srcHash, "id", FALSE);
newSrc->groupList = groupListForSource(newSrc->name);
newSrc->selGroup = stringFromJHash(srcHash, "groupSel", FALSE);
newSrc->selTrack = stringFromJHash(srcHash, "trackSel", FALSE);
newSrc->selTable = stringFromJHash(srcHash, "tableSel", FALSE);
newSrc->selIntersect = stringFromJHash(srcHash, "intersectSel", TRUE);
newSrc->filterList = parseFilters(listFromJHash(srcHash, "filters", TRUE));
newSrc->isPrimary = isPrimary;
if (sameOk(stringFromJHash(srcHash, "isPrimary", TRUE), "1"))
    newSrc->thinksItsPrimary = TRUE;
return newSrc;
}

struct queryConfig *parseQueryConfig(struct hash *qcHash)
/* Translate a hash of JSON elements into a proper queryConfig. */
{
struct queryConfig *queryConfig = NULL;
AllocVar(queryConfig);
struct slRef *srcRef, *sources = listFromJHash(qcHash, "sources", FALSE);
for (srcRef = sources;  srcRef != NULL;  srcRef = srcRef->next)
    {
    struct hash *srcHash = hashFromJEl(srcRef->val, "source object", FALSE);
    struct sourceConfig *newSrc = parseSourceConfig(srcHash, (srcRef == sources));
    slAddHead(&(queryConfig->sources), newSrc);
    }
slReverse(&(queryConfig->sources));
// Only one formatter:
AllocVar(queryConfig->formatters);
struct hash *outSpec = hashFromJHash(qcHash, "output", FALSE);
queryConfig->formatters->outFormat = stringFromJHash(outSpec, "outFormat", FALSE);
return queryConfig;
}

struct sourceConfig *emptyVariantsConfig()
/* Return a sourceConfig for the variants section but otherwise empty. Caller do not modify! */
{
static struct sourceConfig emptyVariantsConfig =
    { NULL, "variantsContents", NULL, NULL, NULL, NULL, NULL, NULL, FALSE, FALSE};
if (emptyVariantsConfig.groupList == NULL)
    emptyVariantsConfig.groupList = variantsGroupList();
return &emptyVariantsConfig;
}

struct sourceConfig *emptyGenesConfig()
/* Return a sourceConfig for the genes section but otherwise empty. Caller do not modify! */
{
static struct sourceConfig emptyGenesConfig =
    { NULL, "genesContents", NULL, NULL, NULL, NULL, NULL, NULL, FALSE, FALSE};
if (emptyGenesConfig.groupList == NULL)
    emptyGenesConfig.groupList = genesGroupList();
return &emptyGenesConfig;
}

struct sourceConfig *emptySourceConfig(unsigned short i)
/* Return an empty sourceConfig with numeric name from i, for immediate use only.
 * Caller do not modify! */
{
#define EMPTY_SOURCE_NAME_SIZE 32
static struct sourceConfig emptySourceConfig =
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, FALSE, FALSE};
if (emptySourceConfig.name == NULL)
    emptySourceConfig.name = needMem(EMPTY_SOURCE_NAME_SIZE);
safef(emptySourceConfig.name, EMPTY_SOURCE_NAME_SIZE, "source%dContents", i);
return &emptySourceConfig;
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
dyStringAppend(dy, "<SELECT NAME='groupSel'>\n");
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
dyStringAppend(dy, "<SELECT NAME='groupSel'>\n");
int gotCT = gotCustomTracks() ? 1 : 0;
//#*** should probably offer hub tracks here too.
// Count the number of groups that actually exist in this database:
int groupCount = 0;
struct slName *group;
for (group = groups;  group != NULL;  group = group->next)
    if (findGroup(fullGroupList, group->name))
	groupCount++;
if (!gotCT && groupCount == 1)
    {
    struct grp *onlyGroup = findGroup(fullGroupList, groups->name);
    if (onlyGroup == NULL)
	errAbort("makeSpecificGroupDropDown: Can't find group '%s'", groups->name);
    dyStringPrintf(dy, " <OPTION VALUE='%s' SELECTED>%s", onlyGroup->name, onlyGroup->label);
    selGroup = onlyGroup->name;
    }
else
    {
    if (selGroup == NULL || !findGroup(fullGroupList, selGroup))
	dyStringAppend(dy, " <OPTION VALUE='none' SELECTED>\n");
    if (gotCT)
	dyStringPrintf(dy, " <OPTION VALUE='user'%s>Custom Tracks\n",
		       sameOk(selGroup, "user") ? " SELECTED" : "");
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

char *makeTrackDropDown(struct dyString *dy, char *selGroup, char *selTrack, char **retSelTable)
/* Make track drop-down for selGroup using fullTrackList. If selTrack is NULL
 * or not in this assembly, return first track in selGroup; otherwise return selTrack. */
{
dyStringAppend(dy, "<SELECT NAME='trackSel'>\n");
boolean allTracks = sameString(selGroup, "All Tracks");
if (allTracks)
    slSort(&fullTrackList, trackDbCmpShortLabel);
struct trackDb *tdb = NULL;
boolean foundSelTrack = FALSE;
char *firstTrack = NULL;
for (tdb = fullTrackList; tdb != NULL; tdb = tdb->next)
    {
    if (allTracks || sameString(selGroup, tdb->grp))
	{
	boolean thisIsSelTrack = sameOk(tdb->track, selTrack);
	if (thisIsSelTrack)
	    foundSelTrack = TRUE;
	dyStringPrintf(dy, " <OPTION VALUE=\"%s\"%s>%s\n", tdb->track,
		(thisIsSelTrack ? " SELECTED" : ""),
		tdb->shortLabel);
	if (firstTrack == NULL)
	    firstTrack = tdb->track;
	}
    }
if (! foundSelTrack)
    {
    selTrack = firstTrack;
    *retSelTable = NULL;
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

char *makeTableDropDown(struct dyString *dy, char *selTrack, char *selTable)
/* Make table drop-down for non-NULL selTrack. */
{
dyStringAppend(dy, "<SELECT NAME='tableSel'>\n");
struct trackDb *track = findTdb(fullTrackList, selTrack);
if (track == NULL)
    selTable = NULL;
else
    {
    struct slName *t, *tableList = tablesForTrack(track);
    if (tableList != NULL &&
	(isEmpty(selTable) || !slNameInList(tableList, selTable)))
	selTable = tableList->name;
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
    }
dyStringPrintf(dy, "</SELECT>\n");
return selTable;
}

void makeEmptySelect(struct dyString *dy, char *name)
/* print out an empty select, to be filled in by javascript */
{
dyStringPrintf(dy, "<SELECT NAME='%s'></SELECT>\n", name);
}

void dyMakeButtonWithMsg(struct dyString *dy, char *name, char *value, char *msg)
/* Like cheapcgi's function, but on a dyString.
 * Make 'submit' type button. Display msg on mouseover, if present*/
{
dyStringPrintf(dy, "<INPUT TYPE=SUBMIT NAME=\"%s\" VALUE=\"%s\" %s%s%s>",
	       name, value,
	       (msg ? " TITLE=\"" : ""), (msg ? msg : ""), (msg ? "\"" : "" ));
}

INLINE void addOption(struct dyString *dy, char *value, char *selected)
{
dyStringPrintf(dy, "<OPTION VALUE='%s'%s>", value,
	       sameOk(selected, value) ? "SELECTED" : "");
}

void makeIntersectDropDown(struct dyString *dy, struct sourceConfig *src, char *primaryTrack)
/* Give the user the option of keeping/dropping primary items based on overlap with this track. */
{
struct trackDb *tdb = findTdb(fullTrackList, src->selTrack);
if (tdb == NULL)
    return;
if (isEmpty(primaryTrack))
    primaryTrack = "primary track";
dyStringPrintf(dy, "<SELECT NAME='intersectSel'%s>\n",
	       src->isPrimary ? " style='display: none'" : "");
addOption(dy, "keepAll", src->selIntersect);
dyStringPrintf(dy, "Keep all %s items whether or not they overlap %s items</OPTION>\n",
	       primaryTrack, tdb->shortLabel);
addOption(dy, "mustOverlap", src->selIntersect);
dyStringPrintf(dy, "Keep %s items only if they overlap %s items</OPTION>\n",
	       primaryTrack, tdb->shortLabel);
addOption(dy, "mustNotOverlap", src->selIntersect);
dyStringPrintf(dy, "Keep %s items only if they do not overlap %s items</OPTION>\n",
	       primaryTrack, tdb->shortLabel);
dyStringPrintf(dy, "</SELECT>\n");
}

void dyStartJsonHashEl(struct dyString *dy, boolean isFirst, char *name)
/* Add '"name": ' to dy, prepending "{ " or ", " as necessary. */
{
if (isFirst)
    dyStringAppend(dy, "{ ");
else
    dyStringAppend(dy, ", ");
dyStringAppendC(dy, '"');
dyStringAppend(dy, name);
dyStringAppend(dy, "\": ");
}

void dyAddQuotedString(struct dyString *dy, char *string)
/* Add double-quotes around string and escape them inside string. */
{
dyStringAppendC(dy, '"');
dyStringAppendEscapeQuotes(dy, string, '"', '\\');
dyStringAppendC(dy, '"');
}

void dyAddNumericValue(struct dyString *dy, enum asTypes type, void *values)
/* Cast the first element of values to the appropriate type and add to dy. */
{
if (asTypesIsFloating(type))
    dyStringPrintf(dy, "%lf", *(double *)values);
else if (asTypesIsInt(type))
    dyStringPrintf(dy, "%lld", *(long long *)values);
else
    errAbort("dyAddNumericValue: unrecognized type %d for numeric op", type);
}

void dyAddAnnoFilterValues(struct dyString *dy, struct annoFilter *filter)
/* Deduce the type and number of values depending on op, and JSONify accordingly. */
{
void *values = filter->values;
enum annoFilterOp op = filter->op;
if (values == NULL || op == afNoFilter)
    {
    dyStringAppend(dy, "null");
    return;
    }
else if (op == afMatch || op == afNotMatch)
    dyAddQuotedString(dy, (char *)values);
else if (op == afLT || op == afLTE || op == afEqual || op == afNotEqual ||
	 op == afGTE || op == afGT || op == afInRange)
    {
    if (op == afInRange)
	dyStringAppend(dy, "[ ");
    dyAddNumericValue(dy, filter->type, values);
    if (op == afInRange)
	{
	dyStringAppend(dy, ", ");
	dyAddNumericValue(dy, filter->type, values+1);
	dyStringAppend(dy, " ]");
	}
    }
else
    errAbort("dyAddAnnoFilterValues: unrecognized filter->op %d", filter->op);
}

void dyAddAnnoFilterMinMax(struct dyString *dy, struct annoFilter *filter)
/* If filter has a min and max, deduce their type and JSONify accordingly. */
{
dyStartJsonHashEl(dy, FALSE, "hasMinMax");
dyStringPrintf(dy, "%d", filter->hasMinMax);
boolean isDouble = asTypesIsFloating(filter->type);
dyStartJsonHashEl(dy, FALSE, "min");
if (isDouble)
    dyStringPrintf(dy, "%Lf", filter->min.aDouble);
else
    dyStringPrintf(dy, "%lld", filter->min.anInt);
dyStartJsonHashEl(dy, FALSE, "max");
if (isDouble)
    dyStringPrintf(dy, "%Lf", filter->max.aDouble);
else
    dyStringPrintf(dy, "%lld", filter->max.anInt);
}

void dyAddAnnoFilter(struct dyString *dy, struct annoFilter *filter)
/* Encode an annoFilter as a JSON hash (object). */
{
dyStartJsonHashEl(dy, TRUE, "label");
dyAddQuotedString(dy, filter->label);
dyStartJsonHashEl(dy, FALSE, "type");
dyStringPrintf(dy, "%d", filter->type);
dyStartJsonHashEl(dy, FALSE, "op");
dyStringPrintf(dy, "\"%s\"", stringFromAfOp(filter->op));
dyStartJsonHashEl(dy, FALSE, "values");
dyAddAnnoFilterValues(dy, filter);
dyStartJsonHashEl(dy, FALSE, "isExclude");
dyStringPrintf(dy, "%d", filter->isExclude);
dyStartJsonHashEl(dy, FALSE, "rightJoin");
dyStringPrintf(dy, "%d", filter->rightJoin);
dyAddAnnoFilterMinMax(dy, filter);
dyStringAppend(dy, " }");
}

struct annoFilter *findFilterByLabel(struct annoFilter *filterList, char *label)
/* Return annoFilter with name, or NULL. */
{
struct annoFilter *af;
for (af = filterList;  af != NULL;  af = af->next)
    if (sameString(label, af->label))
	return af;
return NULL;
}

struct annoStreamer *streamerFromSource(char *db, char *table, struct trackDb *tdb, char *chrom);
//#*** Move it up here?  Or move out to a lib!!

void makeFilterSection(struct dyString *dy, struct sourceConfig *src)
/* Add a filter controls section to dyString: build up two javascript variables for UI,
 * one for active filters (i.e. those that the user has already configured) and
 * one for available filters. */
{
struct trackDb *tdb = findTdb(fullTrackList, src->selTrack);
if (tdb == NULL)
    return;
dyStringPrintf(dy, "<div name='filter' class='sourceFilter'>\n");
dyStringPrintf(dy, "<script>activeFilterList['%s'] = [ ", src->name);
struct dyString *availDy = dyStringCreate("availableFilterList['%s'] = [ ", src->name);
boolean gotActive = FALSE, gotAvail = FALSE;
struct annoStreamer *streamer = streamerFromSource(database, src->selTable, tdb, NULL);
struct annoFilter *af, *afList = streamer->getFilters(streamer);
for (af = afList;  af != NULL;  af = af->next)
    {
    struct annoFilter *cf = findFilterByLabel(src->filterList, af->label);
    if (cf != NULL)
	{
	if (!gotActive)
	    gotActive = TRUE;
	else
	    dyStringAppend(dy, ", ");
	dyAddAnnoFilter(dy, cf);
	}
    else
	{
	if (!gotAvail)
	    gotAvail = TRUE;
	else
	    dyStringAppend(availDy, ", ");
	dyAddAnnoFilter(availDy, af);
	}
    }
dyStringPrintf(dy, " ];\n");
dyStringAppend(dy, availDy->string);
dyStringAppend(dy, " ];\n</script>\n</div>\n");
dyStringFree(&availDy);
}

// Defined after a bunch of verbatims below:
void initGroupsTracksTables();

char *buildSourceContents(struct sourceConfig *src, char *primaryTrack)
/* Return a string with the contents of a <source>Contents div:
 * group, track, table selects and empty filter. */
{
initGroupsTracksTables();
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "<INPUT TYPE=HIDDEN NAME='%s' VALUE='%s'>\n", "isPrimary",
	       src->isPrimary ? "1" : "0");
dyStringPrintf(dy, "<B>group:</B> ");
if (src->groupList != NULL)
    src->selGroup = makeSpecificGroupDropDown(dy, src->groupList, src->selGroup);
else
    makeGroupDropDown(dy, src->selGroup);
dyStringPrintf(dy, "<B>&nbsp;track:</B> ");
if (isNotEmpty(src->selGroup))
    src->selTrack = makeTrackDropDown(dy, src->selGroup, src->selTrack, &(src->selTable));
else
    makeEmptySelect(dy, "trackSel");
dyStringPrintf(dy, "<B>&nbsp;table:</B> ");
if (isNotEmpty(src->selTrack))
    src->selTable = makeTableDropDown(dy, src->selTrack, src->selTable);
else
    makeEmptySelect(dy, "tableSel");
if (slNameInList(src->groupList, "varRep") && !gotCustomTracks())
    dyStringPrintf(dy, "<BR>\n<EM>Note: to upload your own variants, click the "
	   "&quot;"CT_ADD_BUTTON_LABEL"&quot; button above.</EM>\n");
dyStringPrintf(dy, "<BR>\n");
if (isNotEmpty(src->selTable))
    {
    makeIntersectDropDown(dy, src, primaryTrack);
    makeFilterSection(dy, src);
    dyMakeButtonWithMsg(dy, "addFilter", "Add Filter", "add constraints on data");
    }
return dyStringCannibalize(&dy);
}

void printDataSourceSection(struct sourceConfig *src, char *primaryTrack, boolean show)
/* Add section with group/track/table selects, which may be restricted to a particular
 * group and may be hidden. */
{
char *title = "Select Data";
if (sameString(src->name, "variantsContents"))
    title = "Select Variants";
else if (sameString(src->name, "genesContents"))
    title = "Select Genes";
char *divId = cloneString(src->name);
char *p = strstr(divId, "Contents");
if (p != NULL)
    *p = '\0';
webNewSectionLite(TRUE, divId, show, title);
printf("<div id='%s' class='sourceSection'>\n", src->name);
if (isNotEmpty(src->selTrack))
    {
    struct trackDb *tdb = findTdb(fullTrackList, src->selTrack);
    if (tdb != NULL)
	src->selGroup = tdb->grp;
    }
char *contents = buildSourceContents(src, primaryTrack);
puts(contents);
printf("</div>\n");
freeMem(contents);
}

void printAddDataSection()
/* Print a very lightweight section that just has a "Select More Data" button. */
{
webNewSectionLite(FALSE , "addData", TRUE, "");
cgiMakeButtonWithOnClick("addData", "Select More Data",
			 "select a new track to integrate with Variants",
			 "hgva.showNextHiddenSource();");
}

void makeOutputFormatDropDown(char *selectName, char *selected)
/* placeholder for now... the real choices will depend on inputs */
{
char *menu[] = { "Tab-separated text", "Something else" };
char *values[] = { "tabSep", "other" };
cgiMakeDropListWithVals(selectName, menu, values, ArraySize(menu), selected);
}

void printOutputSection(struct queryConfig *queryConfig)
/* Print an output format section that can't be removed like data source sections can. */
{
webNewSectionLite(FALSE, "outFormat", TRUE, "Select Output Format");
printf("<div id='outFormatContents' class='outputSection'>\n");
char *selOutput = "";
if (queryConfig != NULL)
    selOutput = queryConfig->formatters->outFormat;
makeOutputFormatDropDown("outFormat", selOutput);
printf("<BR>\n");
printf("<div id='outOptions'>\n");
cgiMakeButtonWithMsg("outSelectFields", "Select fields",
		     "select particular fields from inputs");
printf("</div>\n"); // options
printf("</div>\n"); // contents
}

void printSubmitSection()
/* Print an output format section that can't be removed like data source sections can. */
{
webNewSectionLite(FALSE, "submitSection", TRUE, "Get Results");
cgiMakeButtonWithOnClick("startQuery", "Go!",
			 "get the results of your query",
			 "hgva.executeQuery();");
printf("&nbsp;<img id='loadingImg' src='../images/loading.gif' />\n");
printf("<span id='loadingMsg'></span>\n");
puts("<div class='warn-note' style='border: 2px solid #9e5900; padding: 5px 20px; background-color: #ffe9cc;'>");
puts("<p><span style='font-weight: bold; color: #c70000;'>NOTE:</span><br>");
puts("This tool is for research use only. While this tool is open to the "
     "public, users seeking information about a personal medical or genetic "
     "condition are urged to consult with a qualified physician for "
     "diagnosis and for answers to personal questions.");
puts("</p></div>");
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
//#*** hgTracks needs a full track list too... can it all be libified?
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
    if (tbOff == NULL || !startsWithWord("off", tbOff))
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

char *primaryTrackName(struct queryConfig *queryConfig)
/* Return name of queryConfig's primary source track, checking for NULL. */
{
if (queryConfig == NULL || queryConfig->sources == NULL)
    return NULL;
initGroupsTracksTables();
struct trackDb *tdb = findTdb(fullTrackList, queryConfig->sources->selTrack);
if (tdb == NULL)
    return NULL;
return tdb->shortLabel;
}

void printExtraSource(unsigned short i, struct sourceConfig *src, char *primaryTrack, boolean show)
/* Print a source section with unrestricted group list and numeric-suffix id. */
{
if (src == NULL || primaryTrack == NULL)
    printDataSourceSection(emptySourceConfig(i), primaryTrack, show);
else
    {
    char *tmpName = src->name;
    char sourceI[32];
    safef(sourceI, sizeof(sourceI), "source%dContents", i);
    src->name = sourceI;
    printDataSourceSection(src, primaryTrack, show);
    src->name = tmpName;
    }
}

#define MAX_EXTRA_SOURCES 5

void printDefaultSources()
/* When cart doesn't yet have a querySpec, show variants & genes; make several hidden sources. */
{
printDataSourceSection(emptyVariantsConfig(), NULL, TRUE);
printDataSourceSection(emptyGenesConfig(), NULL, TRUE);
unsigned short i;
for (i = 0;  i < MAX_EXTRA_SOURCES;  i++)
    printExtraSource(i, NULL, NULL, FALSE);
}

void printSourcesFromQueryConfig(struct queryConfig *queryConfig)
/* Show sources with the same order and settings as queryConfig. */
{
char *primaryTrack = primaryTrackName(queryConfig);

struct sourceConfig *src;
unsigned short i;
for (i = 0, src = queryConfig->sources;  src != NULL;  src = src->next)
    {
    if (sameString(src->name, "variantsContents") || sameString(src->name, "genesContents"))
	printDataSourceSection(src, primaryTrack, TRUE);
    else
	printExtraSource(i++, src, primaryTrack, TRUE);
    }
// If the max number of extra sources hasn't been reached, add hidden extra sources:
for (;  i < MAX_EXTRA_SOURCES;  i++)
    printExtraSource(i, NULL, primaryTrack, FALSE);
}

void doMainPage()
/* Print out initial HTML of control page. */
{
printAssemblySection();
webEndOldSection();
initGroupsTracksTables();
struct queryConfig *queryConfig = NULL;
char *queryStr = cartOptionalString(cart, "querySpec");
if (queryStr != NULL)
    {
    struct jsonElement *querySpecJson = jsonParse(queryStr);
    struct hash *querySpec = hashFromJEl(querySpecJson, "querySpec from cart", FALSE);
    queryConfig = parseQueryConfig(querySpec);
    }
printf("<div id='sourceContainerPlus'>\n");
printf("<div id='sourceContainer'>\n");
// Each data source section will add to these variables:
printf("<script>var activeFilterList = {}; var availableFilterList = {};</script>\n");
if (queryConfig == NULL)
    printDefaultSources();
else
    {
    printSourcesFromQueryConfig(queryConfig);
    }
webEndSectionLite();
printf("</div>\n"); // sourceContainer
printAddDataSection();
webEndSectionLite();
printf("</div>\n"); // sourceContainerPlus (extend down a bit so sections can be dragged to bottom)
printOutputSection(queryConfig);
printSubmitSection();

// __detectback trick from http://siphon9.net/loune/2009/07/detecting-the-back-or-refresh-button-click/
printf("<script>\n"
       "document.write(\"<form style='display: none'><input name='__detectback' id='__detectback' "
       "value=''></form>\");\n"
       "function checkPageBackOrRefresh() {\n"
       "  if (document.getElementById('__detectback').value) {\n"
       "    return true;\n"
       "  } else {\n"
       "    document.getElementById('__detectback').value = 'been here';\n"
       "    return false;\n"
       "  }\n"
       "}\n"
       "window.onload = function() { "
       "  if (checkPageBackOrRefresh()) { window.location.replace('%s?%s'); } };\n"
       "</script>\n", getScriptName(), cartSidUrlString(cart));

//#*** ------------------ more verbatim from mainPage.c ---------------
/* Hidden form for jumping to custom tracks CGI. */
hPrintf("<FORM ACTION='%s' NAME='customTrackForm'>", hgCustomName());
cartSaveSession(cart);
hPrintf("</FORM>\n");

/* Hidden form for jumping to track hub manager CGI. */
hPrintf("<FORM ACTION='%s' NAME='trackHubForm'>", hgHubConnectName());
//#*** well, almost verbatim... we should have an hgThisCgiName().
cgiMakeHiddenVar(hgHubConnectCgiDestUrl, hgVarAnnogratorName());
cartSaveSession(cart);
hPrintf("</FORM>\n");
//#*** ------------------ end verbatim ---------------

webNewSection("Using the Variant Annotation Integrator");
printf("Documentation goes here :)\n");
}

/* Null terminated list of CGI Variables we don't want to save
 * permanently. */
char *excludeVars[] = {"Submit", "submit", "updatePage", "executeQuery", NULL,};

void hgVarAnnogrator()
/* UI main (as opposed to AJAX or query execution):
 * Initialize state, dispatch command and clean up. */
{
oldVars = hashNew(10);
cart = cartAndCookie(hUserCookie(), excludeVars, oldVars);

/* Set up global variables. */
getDbAndGenome(cart, &database, &genome, oldVars);
regionType = cartUsualString(cart, hgvaRegionType, hgvaRegionTypeRange);
position = cloneString(cartOptionalString(cart, hgvaRange));
if (isEmpty(position))
    position = hDefaultPos(database);
cartSetString(cart, hgvaRange, position);
position = cloneString(position);

setUdcCacheDir();
int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
#if ((defined USE_BAM || defined USE_TABIX) && defined KNETFILE_HOOKS)
knetUdcInstall();
#endif//def (USE_BAM || USE_TABIX) && KNETFILE_HOOKS

cartWebStart(cart, database, "Variant Annotation Integrator");
jsInit();
jsIncludeFile("jquery-ui.js", NULL);
webIncludeResourceFile("jquery-ui.css");
jsIncludeFile("hgVarAnnogrator.js", NULL);
addSomeCss();
if (lookupPosition())
    doMainPage();
else if (webGotWarnings())
    {
    // We land here when lookupPosition pops up a warning box.
    // Reset the problematic position and show the main page.
    position = cloneString(cartUsualString(cart, "lastPosition", hDefaultPos(database)));
    cartSetString(cart, hgvaRange, position);
    doMainPage();
    }
// If lookupPosition returned FALSE and didn't report warnings,
// then it wrote HTML showing multiple position matches & links.
cartWebEnd();
/* Save variables. */
cartCheckout(&cart);
textOutClose(&compressPipeline);
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

void changeGroupTrackTable(char *divId, struct queryConfig *queryConfig,
			   char *primaryTrack, enum gtt groupTrackOrTable)
/* Send new contents for the section: same group select, updated track & table selects,
 * reset filter section. */
{
printf("{ \"updates\": [ { \"id\": \"#%s\", \"contents\": \"", divId);
struct sourceConfig *src;
for (src = queryConfig->sources;  src != NULL;  src = src->next)
    {
    if (sameString(src->name, divId))
	break;
    }
if (src == NULL)
    errAbort("Can't find source '%s'", divId);
if (groupTrackOrTable == gttGroup)
    src->selTrack = src->selTable = NULL;
else if (groupTrackOrTable == gttTrack)
    src->selTable = NULL;
else if (groupTrackOrTable != gttTable)
    errAbort("Unexpected groupTrackOrTable enum val %d", groupTrackOrTable);
char *newContents = buildSourceContents(src, primaryTrack);
printf("%s\" } ]", escapeStringForJson(newContents));
printf(" }\n");
}

void showFilter(char *divId, struct queryConfig *queryConfig)
/* Send new contents for div's filter section. */
{
//#*** Make filter settings from the appropriate src in queryConfig
printf("{ \"updates\": [ { \"id\": \"#%s #filter\", \"append\": true, "
       "\"contents\": \"", divId);
printf("a new filter!<BR>");
puts("\" } ] }");
}

void showOutSelectFields(char *divId, struct queryConfig *queryConfig)
/* Send new contents for divId's field selection section. */
{
printf("{ \"updates\": [ { \"id\": \"#%s\", \"contents\": \"", divId);
//#*** for tabSep, iterate through sources and show fields for each one (expand/collapse each)
printf("a whole bunch of checkboxes<BR>");
puts("\" } ] }");
}

void updateSourcesAndOutput(char *removedSource, struct queryConfig *queryConfig,
			    char *primaryTrack)
/* The number and/or order of sources has changed.  If primary source has changed,
 * then filter settings of the former and new primaries need to be adjusted.
 * If the number of sources has changed, we might end up with more or fewer
 * choices of output format.  Send JSON to server with new HTML for changed sections. */
{
struct dyString *dy = dyStringNew(512);
boolean gotUpdate = FALSE;
if (isNotEmpty(removedSource))
    {
    gotUpdate = TRUE;
    printf("{ \"updates\": [ { \"id\": \"#%s\", \"contents\": \"", removedSource);
    char *newContents = buildSourceContents(emptySourceConfig(0), primaryTrack);
    printf("%s\" }", escapeStringForJson(newContents));
    }
if (gotUpdate)
    printf(" ], ");
else
    printf("{ ");
dyStringAppend(dy, "Still need to compute possible output format choices from input types.");
printf("\"serverSays\": \"%s\" }\n", dy->string);
}

boolean gotSinglePosition(char *spec)
/* This duplicates some logic from hgFind.c::genomePos(), so we can determine whether
 * we can send a little ajax update for the position, or whether we need to resubmit to
 * get the printf'd HTML showing multiple results or warning that term is not found. */
{
struct hgPositions *hgp = NULL;
char *terms[16];
int termCount = chopByChar(cloneString(spec), ';', terms, ArraySize(terms));
boolean multiTerm = (termCount > 1);
char *chrom = NULL;
int start = BIGNUM;
int end = 0;
int i;
for (i = 0;  i < termCount;  i++)
    {
    trimSpaces(terms[i]);
    if (isEmpty(terms[i]))
	continue;
    hgp = hgPositionsFind(database, terms[i], "", getScriptName(), cart, multiTerm);
    if (hgp != NULL && hgp->posCount > 0 && hgp->singlePos != NULL)
	{
	if (chrom != NULL && !sameString(chrom, hgp->singlePos->chrom))
	    return FALSE;
	chrom = hgp->singlePos->chrom;
	if (hgp->singlePos->chromStart < start)
	    start = hgp->singlePos->chromStart;
	if (hgp->singlePos->chromEnd > end)
	    end = hgp->singlePos->chromEnd;
	}
    else
	return FALSE;
    }
if (chrom != NULL)
    {
    char posBuf[128];
    safef(posBuf, sizeof(posBuf), "%s:%d-%d", chrom, start+1, end);
    position = cloneString(posBuf);
    }
else
    position = hDefaultPos(database);
cartSetString(cart, hgvaRange, position);
return TRUE;
}

void updatePosition(struct hash *topHash)
/* Look up the position value in case it's a search term. If there's an unambiguous result,
 * update the page with the new value.  If there are multiple results, show them so the
 * user can select one. */
{
if (gotSinglePosition(position))
    {
    printf("{ \"values\": [ { \"id\": \"[name='"hgvaRange"']\", \"value\": \"%s\" } ] }",
	   addCommasToPos(NULL, position));
    }
else
    {
    printf("{ \"resubmit\": \"#mainForm\" }");
    }
}

boolean columnsMatch(struct asObject *asObj, struct sqlFieldInfo *fieldList)
/* Return TRUE if asObj's column names match the given SQL fields. */
{
if (asObj == NULL)
    return FALSE;
struct sqlFieldInfo *firstRealField = fieldList;
if (sameString("bin", fieldList->field) && differentString("bin", asObj->columnList->name))
    firstRealField = fieldList->next;
boolean columnsMatch = TRUE;
struct sqlFieldInfo *field = firstRealField;
struct asColumn *asCol = asObj->columnList;
for (;  field != NULL && asCol != NULL;  field = field->next, asCol = asCol->next)
    {
    if (!sameString(field->field, asCol->name))
	{
	columnsMatch = FALSE;
	break;
	}
    }
if (field != NULL || asCol != NULL)
    columnsMatch = FALSE;
return columnsMatch;
}

struct asObject *asObjectFromFields(char *name, struct sqlFieldInfo *fieldList)
/* Make autoSql text from SQL fields and pass it to asParse. */
{
struct dyString *dy = dyStringCreate("table %s\n"
				     "\"Column names grabbed from mysql\"\n"
				     "    (\n", name);
struct sqlFieldInfo *field;
for (field = fieldList;  field != NULL;  field = field->next)
    {
    char *sqlType = field->type;
    // hg19.wgEncodeOpenChromSynthGm12878Pk.pValue has sql type "float unsigned",
    // and I'd rather pretend it's just a float than work unsigned floats into autoSql.
    if (sameString(sqlType, "float unsigned"))
	sqlType = "float";
    char *asType = asTypeNameFromSqlType(sqlType);
    if (asType == NULL)
	errAbort("No asTypeInfo for sql type '%s'!", field->type);
    dyStringPrintf(dy, "    %s %s;\t\"\"\n", asType, field->field);
    }
dyStringAppend(dy, "    )\n");
return asParseText(dy->string);
}

struct asObject *getAutoSqlForTable(char *db, char *dataDb, char *dbTable, struct trackDb *tdb)
/* Get autoSql for dataDb.dbTable from tdb and/or db.tableDescriptions;
 * if it doesn't match columns, make one up from dataDb.table sql fields. */
{
struct sqlConnection *connDataDb = hAllocConn(dataDb);
struct sqlFieldInfo *fieldList = sqlFieldInfoGet(connDataDb, dbTable);
hFreeConn(&connDataDb);
struct asObject *asObj = NULL;
if (tdb != NULL)
    {
    struct sqlConnection *connDb = hAllocConn(db);
    asObj = asForTdb(connDb, tdb);
    hFreeConn(&connDb);
    }
if (columnsMatch(asObj, fieldList))
    return asObj;
else
    return asObjectFromFields(dbTable, fieldList);
}

struct annoAssembly *getAnnoAssembly(char *db)
/* Make annoAssembly for db. */
{
char *nibOrTwoBitDir = hDbDbNibPath(db);
char twoBitPath[HDB_MAX_PATH_STRING];
safef(twoBitPath, sizeof(twoBitPath), "%s/%s.2bit", nibOrTwoBitDir, db);
return annoAssemblyNew(db, twoBitPath);
}

struct annoStreamer *streamerFromSource(char *db, char *table, struct trackDb *tdb, char *chrom)
/* Figure out the source and type of data and make an annoStreamer. */
{
struct annoStreamer *streamer = NULL;
char *dataDb = db, *dbTable = table;
if (chrom == NULL)
    chrom = hDefaultChrom(db);
if (isCustomTrack(table))
    {
    dataDb = CUSTOM_TRASH;
    dbTable = trackDbSetting(tdb, "dbTableName");
    if (dbTable == NULL)
	errAbort("Can't find dbTableName for custom track %s", table);
    }

if (isHubTrack(table))
    {
    printf("\nSorry! I don't do hub tracks yet -- I probably don't support the format of %s.\n",
	   trackDbSettingOrDefault(tdb, "bigDataUrl", "-- doh, no bigDataUrl!"));
    errAbort("hgVarAnnogrator can't do hub track with type '%s'", tdb->type);
    }
struct annoAssembly *assembly = getAnnoAssembly(db);
if (startsWith("wig", tdb->type))
    streamer = annoStreamWigDbNew(dataDb, dbTable, assembly, maxOutRows);
else if (sameString("vcfTabix", tdb->type))
    {
    struct sqlConnection *conn = hAllocConn(db);
    char *fileOrUrl = bbiNameFromSettingOrTableChrom(tdb, conn, table, chrom);
    hFreeConn(&conn);
    streamer = annoStreamVcfNew(fileOrUrl, TRUE, assembly, maxOutRows);
    }
else
    {
    struct sqlConnection *conn = hAllocConn(db);
    char maybeSplitTable[1024];
    if (sqlTableExists(conn, dbTable))
	safecpy(maybeSplitTable, sizeof(maybeSplitTable), dbTable);
    else
	safef(maybeSplitTable, sizeof(maybeSplitTable), "%s_%s", chrom, dbTable);
    struct asObject *asObj = getAutoSqlForTable(db, dataDb, maybeSplitTable, tdb);
    streamer = annoStreamDbNew(dataDb, maybeSplitTable, assembly, asObj);
    }
return streamer;
}

struct annoGrator *gratorFromSource(char *db, struct sourceConfig *src, struct trackDb *tdb,
				    char *chrom, struct annoStreamer *primary)
/* Figure out the source and type of data, make an annoStreamer & wrap in annoGrator. */
{
struct annoGrator *grator = NULL;
char *dataDb = db, *dbTable = src->selTable;
if (isCustomTrack(src->selTable))
    {
    dataDb = CUSTOM_TRASH;
    dbTable = trackDbSetting(tdb, "dbTableName");
    if (dbTable == NULL)
	errAbort("Can't find dbTableName for custom track %s", src->selTable);
    }
if (startsWith("wig", tdb->type))
    {
    struct annoAssembly *assembly = getAnnoAssembly(db);
    grator = annoGrateWigDbNew(dataDb, dbTable, assembly, maxOutRows);
    }
else
    {
    struct annoStreamer *streamer = streamerFromSource(dataDb, dbTable, tdb, chrom);
    if (asObjectsMatch(primary->asObj, pgSnpAsObj()) &&
	asObjectsMatchFirstN(streamer->asObj, genePredAsObj(), 10))
	grator = annoGratorGpVarNew(streamer, FALSE);
    else
	grator = annoGratorNew(streamer);
    }
if (sameString(src->selIntersect, "mustOverlap"))
    grator->setOverlapRule(grator, agoMustOverlap);
else if (sameString(src->selIntersect, "mustNotOverlap"))
    grator->setOverlapRule(grator, agoMustNotOverlap);
return grator;
}

struct annoFormatter *formatterFromOutput(struct queryConfig *queryConfig)
/* Build up formatter given output format and options. */
{
struct formatterConfig *fConfig = queryConfig->formatters;
struct annoFormatter *formatter = NULL;
if (sameString(fConfig->outFormat, "tabSep"))
    formatter = annoFormatTabNew("stdout");
else
    errAbort("Sorry, no support for output format '%s' yet.", fConfig->outFormat);
return formatter;
}

void executeQuery(char *db, struct queryConfig *queryConfig)
/* Build up annoGrator objects from queryConfig, create an annoGratorQuery and execute it. */
{
webStartText();
initGroupsTracksTables();
struct annoStreamer *primary = NULL;
struct annoGrator *gratorList = NULL;
struct sourceConfig *src;
char *chrom = NULL;
uint start = 0, end = 0;
if (sameString(regionType, hgvaRegionTypeRange))
    {
    if (! parsePosition(position, &chrom, &start, &end))
	errAbort("Expected position to be chrom:start-end but got '%s'", position);
    }
for (src = queryConfig->sources;  src != NULL;  src = src->next)
    {
    struct trackDb *tdb = tdbForTrack(db, src->selTable, &fullTrackList);
    if (tdb == NULL)
	errAbort("Unable to find table '%s' in trackDb for track '%s' or its subtracks",
		 src->selTable, src->selTrack);
    if (src->isPrimary)
	primary = streamerFromSource(db, src->selTable, tdb, chrom);
    else
	slAddHead(&gratorList, gratorFromSource(db, src, tdb, chrom, primary));
    }
slReverse(&gratorList);
struct annoFormatter *formatter = formatterFromOutput(queryConfig);
struct annoAssembly *assembly = getAnnoAssembly(db);
struct annoGratorQuery *agq = annoGratorQueryNew(assembly, primary, gratorList, formatter);
if (sameString(regionType, hgvaRegionTypeRange))
    annoGratorQuerySetRegion(agq, chrom, start, end);
annoGratorQueryExecute(agq);
annoGratorQueryFree(&agq);
}

void restoreMiniCart(struct hash *topHash)
/* Retrieve cart variables stored by saveMiniCart() above and sent via AJAX. */
{
database = stringFromJHash(topHash, "db", FALSE);
regionType = stringFromJHash(topHash, hgvaRegionType, TRUE);
position = stringFromJHash(topHash, hgvaRange, TRUE);
cart = cartOfNothing();
char *hgsid = stringFromJHash(topHash, "hgsid", TRUE);
if (isNotEmpty(hgsid))
    cartSetString(cart, "hgsid", hgsid);
char *ctfile = stringFromJHash(topHash, "ctfile", TRUE);
if (isNotEmpty(ctfile))
    cartSetStringDb(cart, database, "ctfile", ctfile);
struct slPair *hubVar, *hubVarList = stringsWithPrefixFromJHash(topHash, hgHubConnectHubVarPrefix);
for (hubVar = hubVarList; hubVar != NULL;  hubVar = hubVar->next)
    cartSetString(cart, hubVar->name, hubVar->val);
char *trackHubs = stringFromJHash(topHash, hubConnectTrackHubsVarName, TRUE);
if (isNotEmpty(trackHubs))
    cartSetString(cart, hubConnectTrackHubsVarName, trackHubs);
}

void doAjax(char *jsonText)
/* Pick apart JSON request, send back HTML for page sections that need to change. */
{
// Undo the htmlPushEarlyHandlers() because after this point they make ugly text:
popWarnHandler();
popAbortHandler();

// Parse jsonText and make sure that it is a hash:
struct jsonElement *request = jsonParse(jsonText);
expectJsonType(request, jsonObject, "top-level request");
struct hash *topHash = request->val.jeHash;
// Every request must include a querySpec:
struct hash *querySpec = hashFromJHash(topHash, "querySpec", FALSE);
struct queryConfig *queryConfig = parseQueryConfig(querySpec);
restoreMiniCart(topHash);

char *action = stringFromJHash(topHash, "action", FALSE);
if (sameString(action, "execute"))
    puts("Content-Type:text/plain\n");
else
    puts("Content-Type:text/javascript\n");

if (sameString(action, "reorderSources"))
    updateSourcesAndOutput(NULL, queryConfig, primaryTrackName(queryConfig));
else if (sameString(action, "lookupPosition"))
    updatePosition(topHash);
else if (sameString(action, "event"))
    {
    // Determine where this event originated:
    char *parentId = stringFromJHash(topHash, "parentId", TRUE);
    char *id = stringFromJHash(topHash, "id", TRUE);
    char *name = stringFromJHash(topHash, "name", TRUE);
    if (isEmpty(id))
	id = name;
    if (isEmpty(id))
	errAbort("request must contain id and/or name, but has neither.");
    char *primaryTrack = primaryTrackName(queryConfig);
    if (sameString(id, "removeMe"))
	updateSourcesAndOutput(parentId, queryConfig, primaryTrack);
    else if (sameString(id, "groupSel"))
	changeGroupTrackTable(parentId, queryConfig, primaryTrack, gttGroup);
     else if (sameString(id, "trackSel"))
	 changeGroupTrackTable(parentId, queryConfig, primaryTrack, gttTrack);
    else if (sameString(id, "tableSel"))
	changeGroupTrackTable(parentId, queryConfig, primaryTrack, gttTable);
    else if (sameString(id, "addFilter"))
	showFilter(parentId, queryConfig);
    else if (sameString(id, "outSelectFields"))
	showOutSelectFields(parentId, queryConfig);
    else if (endsWith(parentId, "Contents"))
	printf("{ \"serverSays\": \"Some new input '%s' I need to handle in %s\" }\n",
	       id, parentId);
    else
	printf("{ \"serverSays\": \"What is '%s' from %s?\" }\n", id, parentId);
    }
else if (sameString(action, "execute"))
    executeQuery(database, queryConfig);
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
if (jsonIn == NULL)
    jsonIn = cgiUsualString("executeQuery", NULL);
if (jsonIn != NULL)
    doAjax(jsonIn);
else
    hgVarAnnogrator();
return 0;
}
