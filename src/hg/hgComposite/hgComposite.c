/* hgComposite --- build a composite */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "htmshell.h"
#include "web.h"
#include "cheapcgi.h"
#include "cart.h"
#include "cartTrackDb.h"
#include "genbank.h"
#include "hgConfig.h"
#include "hgHgvs.h"
#include "hui.h"
#include "grp.h"
#include "hCommon.h"
#include "hgFind.h"
#include "hPrint.h"
#include "jsHelper.h"
#include "memalloc.h"
#include "textOut.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "twoBit.h"
#include "gpFx.h"
#include "bigGenePred.h"
#include "udc.h"
#include "knetUdc.h"
#include "cartTrackDb.h"
#include "trashDir.h"
#include "customComposite.h"

#define hgCompEditPrefix    "hgCompositeEdit_"
#define hgsAddTrack hgCompEditPrefix "addTrack"
#define hgsAddVisTrack hgCompEditPrefix "addVisTrack"
#define hgsChangeGroup hgCompEditPrefix "changeGroup"
#define hgsCurrentGroup hgCompEditPrefix "currentGroup"
#define hgsCurrentComposite hgCompEditPrefix "currentComposite"
#define hgsNewCompositeName hgCompEditPrefix "newCompositeName"
#define hgsNewCompositeShortLabel hgCompEditPrefix "newCompositeShortLabel"
#define hgsNewCompositeLongLabel hgCompEditPrefix "newCompositeLongLabel"
//#define hgCompositePrefix    "hgComposite_"
#define hgDoNewComposite      hgCompEditPrefix    "doNewComposite"

struct track
{
struct track *next;
char *name;
char *shortLabel;
char *longLabel;
};

struct composite
{
struct composite *next;
char *name;
char *shortLabel;
char *longLabel;
struct track *trackList;
};


/* Global Variables */
struct cart *cart;		/* CGI and other variables */
struct hash *oldVars = NULL;	/* The cart before new cgi stuff added. */
char *genome = NULL;		/* Name of genome - mouse, human, etc. */
char *database = NULL;		/* Current genome database - hg17, mm5, etc. */
struct grp *fullGroupList = NULL;	/* List of all groups. */

// Null terminated list of CGI Variables we don't want to save permanently:
char *excludeVars[] = {"Submit", "submit", "hgva_startQuery", hgsAddTrack,  hgsNewCompositeName, hgsNewCompositeShortLabel, hgsNewCompositeLongLabel, hgsChangeGroup, hgsAddVisTrack, NULL};

void nbSpaces(int count)
/* Print some non-breaking spaces. */
{
int i;
for (i=0; i<count; ++i)
    printf("&nbsp;");
}

void addSomeCss()
/*#*** This should go in a .css file of course. */
{
printf("<style>\n"
	"div.sectionLite { border-width: 1px; border-color: #"HG_COL_BORDER"; border-style: solid;"
	"  background-color: #FFFFFF; padding-left: 10px; padding-right: 10px; "
	"  padding-top: 8px; padding-bottom: 5px; margin-top: 5px; margin-bottom: 5px }\n"
	".sectionLiteHeader { font-weight: bold; font-size:larger; color:#000000;"
	"  text-align:left; vertical-align:bottom; white-space:nowrap; }\n"
	"div.sectionLiteHeader.noReorderRemove { padding-bottom:5px; }\n"
	"div.sourceFilter { padding-top: 5px; padding-bottom: 5px }\n"
	"</style>\n");
}


INLINE void startCollapsibleSection(char *sectionSuffix, char *title, boolean onByDefault)
// Wrap shared args to jsBeginCollapsibleSectionFontSize
{
jsBeginCollapsibleSectionFontSize(cart, "hgva", sectionSuffix, title, onByDefault, "1.1em");
}

INLINE void startSmallCollapsibleSection(char *sectionSuffix, char *title, boolean onByDefault)
// Wrap shared args to jsBeginCollapsibleSectionFontSize
{
jsBeginCollapsibleSectionFontSize(cart, "hgva", sectionSuffix, title, onByDefault, "0.9em");
}

#define endCollapsibleSection jsEndCollapsibleSection


static struct dyString *onChangeStart()
/* Start up a javascript onChange command */
{
struct dyString *dy = jsOnChangeStart();
return dy;
}


static char *onChangeClade()
/* Return javascript executed when they change clade. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
dyStringAppend(dy, " document.hiddenForm.org.value=0;");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return jsOnChangeEnd(&dy);
}

static char *onChangeOrg()
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "org");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return jsOnChangeEnd(&dy);
}

static char *onChangeDb()
/* Return javascript executed when they change database. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "db");
return jsOnChangeEnd(&dy);
}

INLINE void printOption(char *val, char *selectedVal, char *label)
/* For rolling our own select without having to build conditional arrays/lists. */
{
printf("<OPTION VALUE='%s'%s>%s\n", val, (sameString(selectedVal, val) ? " SELECTED" : ""), label);
}


static struct composite *getCompositeList(char *db, char *hubName, struct hash *nameHash)
{
struct composite *compositeList = NULL;
FILE *f;

if ((hubName != NULL) && ((f = fopen(hubName, "r")) != NULL))
    {
    fclose(f);
    // read hub to find names of composites and add them to compositeList
    struct trackDb *tdbList = trackDbFromRa(hubName, NULL);
    struct trackDb *tdb, *tdbNext;
    struct composite *composite = NULL;
    struct track *track = NULL;

    for(tdb = tdbList; tdb; tdb = tdbNext)
        {
        hashAdd(nameHash, tdb->track, tdb);
        tdbNext = tdb->next;
        trackDbFieldsFromSettings(tdb);
        if (trackDbSetting(tdb, "compositeTrack"))
            {
            AllocVar(composite);
            slAddHead(&compositeList, composite);
            composite->name = tdb->track;
            composite->shortLabel = tdb->shortLabel;
            composite->longLabel = tdb->shortLabel;
            }
        else
            {
            if (composite == NULL)
                errAbort("track not in composite");
            AllocVar(track);
            track->name = tdb->track;
            track->shortLabel = tdb->shortLabel;
            track->longLabel = tdb->shortLabel;
            slAddHead(&composite->trackList, track);
            }
        }
    }

return compositeList;
}

static char *getSqlBigWig(struct sqlConnection *conn, char *db, struct trackDb *tdb)
{
char buffer[4096];

safef(buffer, sizeof buffer, "NOSQLINJ select fileName from %s", tdb->table);
return sqlQuickString(conn, buffer);
}

static int snakePalette2[] =
{
0x1f77b4, 0xaec7e8, 0xff7f0e, 0xffbb78, 0x2ca02c, 0x98df8a, 0xd62728, 0xff9896, 0x9467bd, 0xc5b0d5, 0x8c564b, 0xc49c94, 0xe377c2, 0xf7b6d2, 0x7f7f7f, 0xc7c7c7, 0xbcbd22, 0xdbdb8d, 0x17becf, 0x9edae5
};


void outTdb(struct sqlConnection *conn, char *db, FILE *f, char *name,  struct trackDb *tdb, char *parent, unsigned int color)
{
char *dataUrl = NULL;
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");

if (bigDataUrl == NULL)
    {
    if (startsWith("bigWig", tdb->type))
        dataUrl = getSqlBigWig(conn, db, tdb);
    }
struct hashCookie cookie = hashFirst(tdb->settingsHash);
struct hashEl *hel;
fprintf(f, "\ttrack %s\n", name);
while ((hel = hashNext(&cookie)) != NULL)
    {
    if (differentString(hel->name, "parent") && differentString(hel->name, "polished")&& differentString(hel->name, "color")&& differentString(hel->name, "track"))
        fprintf(f, "\t%s %s\n", hel->name, (char *)hel->val);
    }
if (bigDataUrl == NULL)
    {
    if (dataUrl != NULL)
        fprintf(f, "\tbigDataUrl %s\n", dataUrl);
    }
fprintf(f, "\tparent %s\n",parent);
fprintf(f, "\tcolor %d,%d,%d\n", (color >> 16) & 0xff,(color >> 8) & 0xff,color & 0xff);
fprintf(f, "\n");
}

static void outComposite(FILE *f, struct composite *composite)
{
char *parent = composite->name;
char *shortLabel = composite->shortLabel;
char *longLabel = composite->longLabel;
fprintf(f,"track %s\n\
shortLabel %s\n\
compositeTrack on\n\
aggregate none\n\
longLabel %s\n\
%s on\n\
type wig \n\
visibility full\n\n", parent, shortLabel, longLabel, CUSTOM_COMPOSITE_SETTING);
}

static struct trackDb *findTrack(char *name, struct trackDb *fullTrackList)
{
struct trackDb *tdb;
for (tdb = fullTrackList; tdb != NULL; tdb = tdb->next)
    {
    if (sameString(name, tdb->track))
        return tdb;            
    }
errAbort("cannot find track");
return NULL;
}

static void outHubHeader(FILE *f, char *db, char *hubName)
{
char *hubFile = strrchr(hubName, '/') + 1;

fprintf(f,"hub hub1\n\
shortLabel User Composite\n\
longLabel User Composite\n\
genomesFile %s\n\
email braney@soe.ucsc.edu\n\
descriptionUrl hub.html\n\n", hubFile);
fprintf(f,"genome %s\n\
trackDb %s\n\n", db, hubFile);
}

static char *getHubName(char *db)
{
struct tempName hubTn;
char buffer[4096];
safef(buffer, sizeof buffer, "%s-%s", customCompositeCartName, db);
char *hubName = cartOptionalString(cart, buffer);

if (hubName == NULL)
    {
    trashDirDateFile(&hubTn, "hgComposite", "hub", ".txt");
    hubName = cloneString(hubTn.forCgi);
    cartSetString(cart, buffer, hubName);
    FILE *f = mustOpen(hubName, "a");
    outHubHeader(f, db, hubName);
    fclose(f);
    cartSetString(cart, "hubUrl", hubName);
    cartSetString(cart, hgHubConnectRemakeTrackHub, hubName);
    }
return hubName;
}


static void outputCompositeHub(char *db, char *hubName,  struct composite *compositeList, struct hash *nameHash)
{
chmod(hubName, 0666);
FILE *f = mustOpen(hubName, "w");

outHubHeader(f, db, hubName);
int useColor = 0;
struct composite *composite;
struct sqlConnection *conn = hAllocConn(db);
for(composite = compositeList; composite; composite = composite->next)
    {
    outComposite(f, composite);
    struct trackDb *tdb;
    struct track *track;
    for (track = composite->trackList; track; track = track->next)
        {
        tdb = hashMustFindVal(nameHash, track->name);

        outTdb(conn, db, f, track->name,tdb, composite->name, snakePalette2[useColor]);
        useColor++;
        if (useColor == (sizeof snakePalette2 / sizeof(int)))
            useColor = 0;
        }
    }
fclose(f);
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


void printCtAndHubButtons()
/* Print a div with buttons for hgCustom and hgHubConnect */
{
boolean hasCustomTracks = customTracksExist(cart, NULL);
puts("<div style='padding-top: 5px; padding-bottom: 5px'>");
hOnClickButton("prtCtHub_CtBut", "document.customTrackForm.submit(); return false;",
	       hasCustomTracks ? CT_MANAGE_BUTTON_LABEL : CT_ADD_BUTTON_LABEL);
printf(" ");
if (hubConnectTableExists())
    hOnClickButton("prtCtHub_TrkHub", "document.trackHubForm.submit(); return false;", "track hubs");
nbSpaces(3);
printf("To reset <B>all</B> user cart settings (including custom tracks), \n"
       "<A HREF=\"cartReset?destination=%s\">click here</A>.\n",
       cgiScriptName());
puts("</div>");
}

void hgGatewayCladeGenomeDb()
/* Make a row of labels and row of buttons like hgGateway, but not using tables. */
{
boolean gotClade = hGotClade();
if (gotClade)
    {
    topLabelSpansStart("clade");
    printCladeListHtml(genome, "change", onChangeClade());
    topLabelSpansEnd();
    }
topLabelSpansStart("genome");
if (gotClade)
    printGenomeListForCladeHtml(database, "change", onChangeOrg());
else
    printGenomeListHtml(database, "change", onChangeOrg());
topLabelSpansEnd();
topLabelSpansStart("assembly");
printAssemblyListHtml(database, "change", onChangeDb());
topLabelSpansEnd();
puts("<BR>");
topLabelSpansEnd();
topLabelSpansStart("");
printf("</span>\n");
topLabelSpansEnd();
puts("<BR>");
}

void printAssemblySection()
/* Print assembly-selection stuff.
 * Redrawing the whole page when the assembly changes seems fine to me. */
{
//#*** ---------- More copied verbatim, from hgTables/mainPage.c: -----------
/* Hidden form - for benefit of javascript. */
    {
    static char *saveVars[] = {
      "clade", "org", "db", };
    jsCreateHiddenForm(cart, cgiScriptName(), saveVars, ArraySize(saveVars));
    }

hPrintf("<FORM ACTION='%s' NAME='changeGroupForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsCurrentGroup, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='changeCompositeForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsCurrentComposite, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='makeNewCompositeForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsNewCompositeName, "");
cgiMakeHiddenVar(hgsNewCompositeShortLabel, "");
cgiMakeHiddenVar(hgsNewCompositeLongLabel, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='addVisTrackForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsAddVisTrack, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='addTrackForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsAddTrack, "");
hPrintf("</FORM>\n");

/* Hidden form for jumping to custom tracks CGI. */
hPrintf("<FORM ACTION='%s' NAME='customTrackForm'>", hgCustomName());
cartSaveSession(cart);
hPrintf("</FORM>\n");

/* Hidden form for jumping to track hub manager CGI. */
hPrintf("<FORM ACTION='%s' NAME='trackHubForm'>", hgHubConnectName());
cartSaveSession(cart);
hPrintf("</FORM>\n");

printf("<FORM ACTION=\"%s\" NAME=\"mainForm\" ID=\"mainForm\" METHOD=%s>\n",
	cgiScriptName(), cartUsualString(cart, "formMethod", "GET"));
cartSaveSession(cart);

//#*** ------------------ end verbatim ---------------

printf("<div class='sectionLiteHeader noReorderRemove'>"
       "Select Genome Assembly</div>\n");

/* Print clade, genome and assembly line. */
hgGatewayCladeGenomeDb();

}

static void printCompositeList(struct composite *compositeList, struct composite *currentComposite)
{
if (compositeList == NULL)
    return;

printf("<div class='sectionLiteHeader noReorderRemove'>"
       "My Composites</div>\n");

int count = slCount(compositeList);
char *labels[count];
char *names[count];
count = 0;
for(; compositeList; compositeList = compositeList->next)
    {
    labels[count] = compositeList->shortLabel;
    names[count] = compositeList->name;
    count++;
    }
cgiMakeDropListFull("compositeList", labels, names, count,
                    currentComposite->name,
                    "change", 
                    "var e = document.getElementById('compositeList'); \
                    var strUser = e.options[e.selectedIndex].value; \
                    document.changeCompositeForm.elements['"hgsCurrentComposite"'].value = strUser; \
                    document.changeCompositeForm.submit();");
                    //document.addTrackForm.elements['hgComp_track'] = strUser;");

}


static void makeAddComposite()
{
printf("<BR>");
printf("<BR>");
printf("<H3>Make New Composite</H3>");
printf("name ");
cgiMakeTextVar(hgsNewCompositeName, "", 29);
printf("<BR>short label ");
cgiMakeTextVar(hgsNewCompositeShortLabel, "", 29);
printf("<BR>long label ");
cgiMakeTextVar(hgsNewCompositeLongLabel, "", 29);
printf("<BR>");
hOnClickButton("selVar_MakeNewComposite", 
                    "var e = document.getElementById('"hgsNewCompositeName"'); \
                    document.makeNewCompositeForm.elements['"hgsNewCompositeName"'].value = e.value; \
                    var e = document.getElementById('"hgsNewCompositeShortLabel"'); \
                    document.makeNewCompositeForm.elements['"hgsNewCompositeShortLabel"'].value = e.value; \
                    var e = document.getElementById('"hgsNewCompositeLongLabel"'); \
                    document.makeNewCompositeForm.elements['"hgsNewCompositeLongLabel"'].value = e.value; \
document.makeNewCompositeForm.submit(); return false;", "submit");
printf("<BR>");
}

static void printTrackList(struct composite *composite)
{
printf("<div class='sectionLiteHeader noReorderRemove'>"
       "Tracks in Composite</div>\n");
printf("<BR>");
printf("<div style=\"max-width:1024px\">");
printf("<table id=\"sessionTable\" class=\"sessionTable stripe hover row-border compact\" borderwidth=0>\n");
printf("<thead><tr>");
printf("<TH><TD><B>Name</B></TD></TH>");
printf("</tr></thead><tbody>");

struct track *track;
if (composite)
    for(track = composite->trackList; track; track = track->next)
        {
        printf("<TR><TD>%s</TD></TR>\n",track->shortLabel);
        }
//printf("<TR><TD>track1</TD></TR>");
//printf("<TR><TD>track2</TD></TR>");
printf("</table>");
}


static boolean trackCanBeAdded(struct trackDb *tdb)
{
return  (tdb->subtracks == NULL) && !startsWith("wigMaf",tdb->type) &&  (startsWith("wig",tdb->type) || startsWith("bigWig",tdb->type)) ;
}

static void availableTracks(char *db, struct grp *groupList, struct trackDb *fullTrackList)
{
printf("<H4>Add tracks from %s</H4>", db);

char *curGroupName = cartOptionalString(cart, hgsCurrentGroup);
printf("<BR>groups: ");
char **labels, **names;
int count = slCount(groupList);
AllocArray(labels, count);
AllocArray(names, count);
count = 0;
struct grp *grp;
for(grp = groupList; grp; grp = grp->next)
    {
    labels[count] = grp->label;
    names[count] = grp->name;
    count++;
    }
cgiMakeDropListFull("availGroups", labels, names, count,
                    curGroupName,
                    "change", 
    "var e = document.getElementById('availGroups'); \
    var strUser = e.options[e.selectedIndex].value; \
    document.changeGroupForm.elements['"hgsCurrentGroup"'].value =  strUser;  \
    document.changeGroupForm.submit();");

struct grp *curGroup;
if (curGroupName == NULL)
    curGroup = groupList;
else
    {
    for(curGroup = groupList; curGroup; curGroup = curGroup->next)
        {
        if (sameString(curGroupName, curGroup->name))
            break;
        }
    if (curGroup == NULL)
        errAbort("cannot find current group");
    }

struct trackDb *tdb;
count = 0;
for(tdb = fullTrackList; tdb; tdb = tdb->next)
    {
    if (sameString(tdb->grp, curGroup->name))
        count++;
    }

//char **labels, **names;
AllocArray(labels, count);
AllocArray(names, count);
count = 0;
for(tdb = fullTrackList; tdb; tdb = tdb->next)
    {
    if (sameString(tdb->grp, curGroup->name))
        {
        labels[count] = tdb->shortLabel;
        names[count] = tdb->track;
        count++;
        }
    }

printf("track:");
cgiMakeDropListFull("availTracks", labels, names, count,
                    *names, 
                    "change", 
                    "var e = document.getElementById('availTracks'); \
                    var strUser = e.options[e.selectedIndex].value; \
                    document.addTrackForm.elements['"hgsAddTrack"'].value = strUser;");
                    //document.addTrackForm.elements['hgComp_track'] = strUser;");
hOnClickButton("addTrack", 
    "var e = document.getElementById('availTracks'); \
    var strUser = e.options[e.selectedIndex].value;  \
    document.addTrackForm.elements['"hgsAddTrack"'].value =  strUser;  \
    document.addTrackForm.submit();"
, "add track");

printf("<BR>");
printf("<BR>");
printf("<BR>");
hOnClickButton("selVar_AddAllVis", "document.addVisTrackForm.submit(); return false;", "Add All Visibile Wiggles");
}

void doMainPage(char *db, struct grp *groupList,  struct trackDb *fullTrackList, struct composite *currentComposite, struct composite *compositeList)
/* Print out initial HTML of control page. */
{
//struct composite *currentComposite = compositeList;
jsInit();
webIncludeResourceFile("jquery-ui.css");
webIncludeResourceFile("ui.dropdownchecklist.css");
boolean alreadyAgreed = cartUsualBoolean(cart, "hgva_agreedToDisclaimer", FALSE);
jsInlineF(
    "$(document).ready(function() { hgva.disclaimer.init(%s, hgva.userClickedAgree); });\n"
    , alreadyAgreed ? "true" : "false");
addSomeCss();
printAssemblySection();

puts("<BR>");
printf("<div class='sectionLiteHeader noReorderRemove'>"
       "Add Hubs and Custom Tracks </div>\n");
printCtAndHubButtons();

//struct hashEl *hel = cartFindPrefix(cart, hgCompEditPrefix);
//if (hel != NULL)
    {
 //   printf("printing EditCom\n");
  ///  printEditComposite();
    }

printf("</FORM>");
puts("<BR>");
puts("<BR>");
printCompositeList(compositeList, currentComposite);
makeAddComposite();
puts("<BR>");
printTrackList(currentComposite);
puts("<BR>");
availableTracks(db, groupList, fullTrackList);
puts("<BR>");

// Make wrapper table for collapsible sections:
//selectVariants();
//char *geneTrack = selectGenes();
//if (geneTrack != NULL)
    {
    //selectRegulatory();
    //selectAnnotations(geneTrack);
    //selectFilters();
    //selectOutput();
    //submitAndDisclaimer();
    }

printf("</FORM>");
jsReloadOnBackButton(cart);

webNewSection("Using the Composite Builder");
webIncludeHelpFileSubst("hgCompositeHelp", NULL, FALSE);
jsIncludeFile("jquery-ui.js", NULL);
jsIncludeFile("hgVarAnnogrator.js", NULL);
jsIncludeFile("ui.dropdownchecklist.js", NULL);
jsIncludeFile("ddcl.js", NULL);
}

void doUi(char *db, struct grp *groupList, struct trackDb *fullTrackList,struct composite *currentComposite, struct composite *compositeList) 
/* Set up globals and make web page */
{
cartWebStart(cart, db, "Composite Editor");
doMainPage(database, groupList, fullTrackList, currentComposite, compositeList);
cartWebEnd();
/* Save variables. */
//cartCheckout(&cart);
}

static void addWigs(struct hash *hash, struct trackDb **wigList, struct trackDb *list)
// Add tracks that are acceptable in custom composites.
{
if (list == NULL)
    return;

struct trackDb *tdb, *tdbNext;
for(tdb = list; tdb; tdb = tdbNext)
    {
    tdbNext = tdb->next;
    addWigs(hash, wigList, tdb->subtracks);

    if (trackCanBeAdded(tdb))
        {
        slAddHead(wigList, tdb);
        hashStore(hash, tdb->grp);
        }
    }

}

char *makeUnique(struct hash *nameHash, struct trackDb *tdb)
// Make the name of this track unique.
{
if (hashLookup(nameHash, tdb->track) == NULL)
    {
    hashAdd(nameHash, tdb->track, tdb);
    return tdb->track;
    }

unsigned count = 0;
char buffer[4096];

for(;; count++)
    {
    safef(buffer, sizeof buffer, "%s%d", tdb->track, count);
    if (hashLookup(nameHash, buffer) == NULL)
        {
        hashAdd(nameHash, buffer, tdb);
        return cloneString(buffer);
        }
    }

return NULL;
}

static bool subtrackEnabledInTdb(struct trackDb *subTdb)
/* Return TRUE unless the subtrack was declared with "subTrack ... off". */
{
bool enabled = TRUE;
char *words[2];
char *setting;
if ((setting = trackDbLocalSetting(subTdb, "parent")) != NULL)
    {
    if (chopLine(cloneString(setting), words) >= 2)
        if (sameString(words[1], "off"))
            enabled = FALSE;
    }
else
    return subTdb->visibility != 0;

return enabled;
}

bool isSubtrackVisible(struct trackDb *tdb)
/* Has this subtrack not been deselected in hgTrackUi or declared with
 *  * "subTrack ... off"?  -- assumes composite track is visible. */
{
boolean overrideComposite = (NULL != cartOptionalString(cart, tdb->track));
bool enabledInTdb = subtrackEnabledInTdb(tdb);
char option[1024];
safef(option, sizeof(option), "%s_sel", tdb->track);
boolean enabled = cartUsualBoolean(cart, option, enabledInTdb);
if (overrideComposite)
    enabled = TRUE;
return enabled;
}


bool isParentVisible( struct trackDb *tdb)
// Are this track's parents visible?
{
if (tdb->parent == NULL)
    return TRUE;

if (!isParentVisible(tdb->parent))
    return FALSE;

char *cartVis = cartOptionalString(cart, tdb->parent->track);
boolean vis;
if (cartVis != NULL) 
    vis =  differentString(cartVis, "hide");
else if (tdbIsSuperTrack(tdb->parent))
    vis = tdb->parent->isShow;
else
    vis = tdb->parent->visibility != tvHide;

return vis;
}


int main(int argc, char *argv[])
/* Process command line. */
{
long enteredMainTime = clock1000();
if (hIsPrivateHost())
    pushCarefulMemHandler(LIMIT_2or6GB);

cgiSpoof(&argc, argv);
htmlPushEarlyHandlers(); /* Make errors legible during initialization. */
oldVars = hashNew(10);
cart = cartAndCookie(hUserCookie(), excludeVars, oldVars);

/* Set up global variables. */
getDbAndGenome(cart, &database, &genome, oldVars);
initGenbankTableNames(database);

int timeout = cartUsualInt(cart, "udcTimeout", 300);
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

struct trackDb *fullTrackList = NULL;	/* List of all tracks in database. */
struct trackDb *wigTracks = NULL;	/* List of all wig tracks */
cartTrackDbInit(cart, &fullTrackList, &fullGroupList, TRUE);
struct hash *groupHash = newHash(5);
addWigs(groupHash, &wigTracks, fullTrackList);
struct grp *grp, *grpNext,  *groupList = NULL;

for(grp = fullGroupList; grp; grp = grpNext)
    {
    grpNext = grp->next;
    if (hashLookup(groupHash, grp->name) != NULL)
        slAddHead(&groupList, grp);
    }

slReverse(&groupList);

/*
struct grp *grpList;
struct trackDb *tdbList = hubCollectTracks(database, &grpList);
addWigs(&wigTracks, tdbList);
*/

char *hubName = getHubName(database);
struct hash *nameHash = newHash(5);
struct composite *compositeList = getCompositeList(database, hubName, nameHash);

struct composite *currentComposite = NULL;
char *currentCompositeName = cartOptionalString(cart, hgsCurrentComposite);

if (currentCompositeName != NULL)
    {
    for (currentComposite = compositeList; currentComposite; currentComposite = currentComposite->next)
        if (sameString(currentComposite->name, currentCompositeName))
            break;
    }

if (currentComposite == NULL)
    currentComposite = compositeList;

char *newCompositeName = cartOptionalString(cart, hgsNewCompositeName);
if (newCompositeName != NULL)
    {
    if (isEmpty(newCompositeName))
        warn("Composite name must not be empty string");
    else
        {
        struct composite *composite;
        AllocVar(composite);
        slAddHead(&compositeList, composite);
        currentComposite = composite;

        composite->name = cloneString(newCompositeName);
        char *newCompositeShortLabel = cartOptionalString(cart, hgsNewCompositeShortLabel);
        if (isEmpty(newCompositeShortLabel))
            composite->shortLabel = composite->name;
        else
            composite->shortLabel = cloneString(newCompositeShortLabel);
        char *newCompositeLongLabel = cartOptionalString(cart, hgsNewCompositeLongLabel);
        if (isEmpty(newCompositeLongLabel))
            composite->longLabel = composite->name;
        else
            composite->longLabel = cloneString(newCompositeLongLabel);

        }
    }

if (currentCompositeName == NULL)
    currentComposite = compositeList;

char *addAllVisible = cartOptionalString(cart, hgsAddVisTrack);
if (addAllVisible != NULL)
    {
    struct trackDb *tdb;

    for(tdb = wigTracks; tdb; tdb = tdb->next)
        {
        if (isParentVisible(tdb) &&  isSubtrackVisible(tdb))
            {
            struct track *track;
            AllocVar(track);
            track->name = makeUnique(nameHash,  tdb);
            track->shortLabel = tdb->shortLabel;
            track->longLabel = tdb->longLabel;
            slAddHead(&currentComposite->trackList, track);
            }
        }
    }

char *newTrackName = cartOptionalString(cart, hgsAddTrack);
if (newTrackName != NULL)
    {
    if (currentComposite == NULL)
        warn("cannot add track without specifying a composite");
    else
        {
        struct trackDb *tdb = findTrack(newTrackName, wigTracks);

        struct track *track;
        AllocVar(track);
        track->name = makeUnique(nameHash,  tdb);
        track->shortLabel = tdb->shortLabel;
        track->longLabel = tdb->longLabel;
        slAddHead(&currentComposite->trackList, track);
        }
    }

if (currentComposite)
    cartSetString(cart, hgsCurrentComposite, currentComposite->name);

doUi(database, groupList, wigTracks, currentComposite, compositeList);

outputCompositeHub(database, hubName,  compositeList, nameHash);
cartCheckout(&cart);
cgiExitTime("hgComposite", enteredMainTime);
return 0;
}
