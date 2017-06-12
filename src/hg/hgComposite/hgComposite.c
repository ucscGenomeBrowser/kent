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
#define hgsAddMathTrack hgCompEditPrefix "addMathTrack"
#define hgsDeleteMathTrack hgCompEditPrefix "deleteMathTrack"
#define hgsDeleteTrack hgCompEditPrefix "deleteTrack"
#define hgsAddVisTrack hgCompEditPrefix "addVisTrack"
#define hgsChangeGroup hgCompEditPrefix "changeGroup"
#define hgsCurrentGroup hgCompEditPrefix "currentGroup"
#define hgsCurrentComposite hgCompEditPrefix "currentComposite"
#define hgsMakeMathTrack hgCompEditPrefix "makeMathTrack"
#define hgsCurrentMathTrack hgCompEditPrefix "currentMathTrack"
#define hgsNewCompositeName hgCompEditPrefix "newCompositeName"
#define hgsNewMathTrackShortLabel hgCompEditPrefix "newMathTrackShortLabel"
#define hgsNewMathTrackLongLabel hgCompEditPrefix "newMathTrackLongLabel"
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
void *reserved;
};

struct composite
{
struct composite *next;
char *name;
char *shortLabel;
char *longLabel;
struct track *trackList;
};

struct mathTrack 
{
struct mathTrack *next;
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
char *excludeVars[] = {"Submit", "submit", "hgva_startQuery", hgsAddTrack,  hgsNewCompositeName, hgsNewCompositeShortLabel, hgsNewCompositeLongLabel, hgsNewMathTrackShortLabel, hgsNewMathTrackLongLabel, hgsChangeGroup, hgsAddVisTrack, hgsAddMathTrack, hgsDeleteMathTrack, "createNewComposite", "deleteComposite", "makeMathWig",  NULL};

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


static struct composite *getCompositeList(char *db, char *hubName, struct hash *nameHash, struct trackDb *wigTracks)
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
            composite->longLabel = tdb->longLabel;
            }
        else
            {
            if (composite == NULL)
                errAbort("track not in composite");
            AllocVar(track);
            track->name = tdb->track;
            track->shortLabel = tdb->shortLabel;
            track->longLabel = tdb->longLabel;
            slAddHead(&composite->trackList, track);
            if (sameString(tdb->type, "mathWig"))
                {
                struct mathTrack *mathTrack = (struct mathTrack *)track;
                char *equation = hashMustFindVal(tdb->settingsHash, "trackNames");
                char *words[100];
                int count = chopByWhite(equation, words, sizeof(words)/sizeof(char *));
                struct track *subTrack;
                int ii;
                for (ii=0; ii < count; ii++)
                    {
                    AllocVar(subTrack);
                    slAddHead(&mathTrack->trackList, subTrack);
                    subTrack->name = cloneString(words[ii]);
                    struct trackDb *tdb = findTrack(subTrack->name, wigTracks);
                    subTrack->shortLabel = tdb->shortLabel;
                    subTrack->longLabel = tdb->longLabel;
                    }
                }
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


char *getUrl(struct sqlConnection *conn, char *db, struct trackDb *wigTracks, struct track *track)
{
struct trackDb *tdb = findTrack(track->name, wigTracks);
char *bigDataUrl = trackDbSetting(tdb, "bigDataUrl");
if (bigDataUrl == NULL)
    {
    if (startsWith("bigWig", tdb->type))
        bigDataUrl = getSqlBigWig(conn, db, tdb);
    }
return bigDataUrl;
}

void outTdb(struct sqlConnection *conn, char *db, FILE *f, char *name,  struct trackDb *tdb, char *parent, unsigned int color, struct track *track, struct trackDb *wigTracks)
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
    if (sameString(hel->name, "mathDataUrl"))
        {
        fprintf(f, "\ttrackNames ");
        struct mathTrack *mt = (struct mathTrack *)track;
        struct track *tr = mt->trackList;
        for(;  tr; tr = tr->next)
            {
            fprintf(f, "%s ", tr->name);
            }
        fprintf(f, "\n");

        fprintf(f, "\tmathDataUrl ");
        tr = mt->trackList;
        for(;  tr; tr = tr->next)
            {
            fprintf(f, "%s ", getUrl(conn, db, wigTracks, tr));
            }
        fprintf(f, "\n");
        }
    else if (differentString(hel->name, "parent") && differentString(hel->name, "polished")&& differentString(hel->name, "color")&& differentString(hel->name, "track")&& differentString(hel->name, "trackNames"))
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


static void outputCompositeHub(char *db, char *hubName,  struct composite *compositeList, struct hash *nameHash, struct trackDb *wigTracks)
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

        outTdb(conn, db, f, track->name,tdb, composite->name, snakePalette2[useColor], track, wigTracks);
        useColor++;
        if (useColor == (sizeof snakePalette2 / sizeof(int)))
            useColor = 0;
        }
    }
fclose(f);
hFreeConn(&conn);
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


//#*** ------------------ end verbatim ---------------

printf("<div class='sectionLiteHeader noReorderRemove'>"
       "Select Genome Assembly</div>\n");

/* Print clade, genome and assembly line. */
hgGatewayCladeGenomeDb();

}

static void printCompositeList(struct composite *compositeList, struct composite *currentComposite)
{
if (compositeList != NULL)
    {

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
    printf("Current composite: ");
    cgiMakeDropListFull("compositeList", labels, names, count,
                        currentComposite->name,
                        "change", 
                        "var e = document.getElementById('compositeList'); \
                        var strUser = e.options[e.selectedIndex].value; \
                        document.changeCompositeForm.elements['"hgsCurrentComposite"'].value = strUser; \
                        document.changeCompositeForm.submit();");
                        //document.addTrackForm.elements['hgComp_track'] = strUser;");

    hOnClickButton("deleteComposite", "document.deleteCompositeForm.submit(); return false;", "Delete Composite");
    }
hOnClickButton("new2Composite", "document.makeNewCompositeForm.submit(); return false;", "New Composite");

}


static void printCompositeLabels(struct composite *composite)
{
if (composite == NULL)
    return;

printf("<BR>short label ");
cgiMakeTextVar(hgsNewCompositeShortLabel, composite->shortLabel, 29);
printf("<BR>long label ");
cgiMakeTextVar(hgsNewCompositeLongLabel, composite->longLabel, 29);
printf("<BR>");
hOnClickButton("selVar_MakeNewComposite",
    "var e = document.getElementById('"hgsNewCompositeLongLabel"'); \
    var strUser = e.value; \
    document.editCompositeForm.elements['"hgsNewCompositeLongLabel"'].value =  strUser;  \
    var e = document.getElementById('"hgsNewCompositeShortLabel"'); \
    var strUser = e.value; \
    document.editCompositeForm.elements['"hgsNewCompositeShortLabel"'].value =  strUser;  \
document.editCompositeForm.submit();" , "make changes");
printf("<BR>");
}

static boolean printMakeMath(struct mathTrack *currentMathTrack)
{
if (currentMathTrack == NULL)
    {
    hOnClickButton("makeMathWig",  "document.makeMathWigForm.submit(); return FLASE;", "Add Math Track to Composite");


    return FALSE; 
    }

printf("<BR>short label ");
cgiMakeTextVar(hgsNewMathTrackShortLabel, currentMathTrack->shortLabel, 29);
printf("<BR>long label ");
cgiMakeTextVar(hgsNewMathTrackLongLabel, currentMathTrack->longLabel, 29);
struct track *track;
hOnClickButton("selVar_newMathLabels",
    "var e = document.getElementById('"hgsNewMathTrackLongLabel"'); \
    var strUser = e.value; \
    document.editMathTrackForm.elements['"hgsNewMathTrackLongLabel"'].value =  strUser;  \
    var e = document.getElementById('"hgsNewMathTrackShortLabel"'); \
    var strUser = e.value; \
    document.editMathTrackForm.elements['"hgsNewMathTrackShortLabel"'].value =  strUser;  \
document.editMathTrackForm.submit();" , "make changes");
printf("<BR>");
printf("<table id=\"sessionTable\" class=\"sessionTable stripe hover row-border compact\" borderwidth=0>\n");
printf("<thead><tr>");
printf("<TD><B>Name</B></TD><TD>Color</TD><TD>Edit</TD><TD>Delete</TD>");
printf("</tr></thead><tbody>");
if (currentMathTrack)
    for(track = currentMathTrack->trackList; track; track = track->next)
        {
        printf("<TR>\n");
        printf("<TD>%s</TD>",track->shortLabel);
        printf("<TD>COLORSELECTOR</TD>");
        printf("<TD>");
        hOnClickButton("selVar_AddAllVis", "document.addVisTrackForm.submit(); return false;", "Edit Track");
        printf("</TD>");
        printf("<TD>");
        char javaScript[4096];
        safef(javaScript, sizeof javaScript, 
            "document.deleteMathTrackForm.elements['"hgsDeleteMathTrack"'].value =  '%s'; "
            "document.deleteMathTrackForm.submit(); return false;",
            track->name);
        char buttonName[1024];
        safef(buttonName, sizeof buttonName, "delMathTrack%s", track->name);
        hOnClickButton(buttonName, javaScript,  "Delete Track");
        printf("</TD>");
        printf("</TR>\n");
        }
printf("</tbody></table>");
return TRUE;
}


static void printTrackList(struct composite *composite)
{
printf("<table id=\"sessionTable\" class=\"sessionTable stripe hover row-border compact\" borderwidth=0>\n");
printf("<thead><tr>");
printf("<TD><B>Name</B></TD><TD>Color</TD><TD>Edit</TD><TD>Delete</TD>");
printf("</tr></thead><tbody>");

struct track *track;
if (composite)
    for(track = composite->trackList; track; track = track->next)
        {
        printf("<TR>\n");
        printf("<TD>%s</TD>",track->shortLabel);
        printf("<TD>COLORSELECTOR</TD>");
printf("<TD>");
hOnClickButton("selVar_AddAllVis", "document.addVisTrackForm.submit(); return false;", "Edit Track");
printf("</TD>");
printf("<TD>");
        char javaScript[4096];
        safef(javaScript, sizeof javaScript, 
            "document.deleteTrackForm.elements['"hgsDeleteTrack"'].value =  '%s'; "
            "document.deleteTrackForm.submit(); return false;",
            track->name);
        char buttonName[1024];
        safef(buttonName, sizeof buttonName, "delTrack%s", track->name);
        hOnClickButton(buttonName, javaScript,  "Delete Track");
printf("</TD>");
        printf("</TR>\n");
        }
printf("</table>");
}


static boolean trackCanBeAdded(struct trackDb *tdb)
{
return  (tdb->subtracks == NULL) && !startsWith("wigMaf",tdb->type) &&  (startsWith("wig",tdb->type) || startsWith("bigWig",tdb->type)) ;
}

static void availableTracks(char *db, struct grp *groupList, struct trackDb *fullTrackList, char *formName, char *varName)
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
char buffer[1024];
safef(buffer, sizeof buffer, "availGroups%s", formName);
char javaScript[4096];
safef(javaScript, sizeof javaScript, 
    "var e = document.getElementById('availGroups%s'); \
    var strUser = e.options[e.selectedIndex].value; \
    document.changeGroupForm.elements['"hgsCurrentGroup"'].value =  strUser;  \
    document.changeGroupForm.submit();", formName);
cgiMakeDropListFull(buffer, labels, names, count,
                    curGroupName,
                    "change", javaScript);

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
safef(buffer, sizeof buffer, "availTracks%s", formName);
cgiMakeDropListFull(buffer, labels, names, count,
                    *names, 
                    "change", "");
safef(javaScript, sizeof javaScript, 
    "var e = document.getElementById('availTracks%s'); \
    var strUser = e.options[e.selectedIndex].value;  \
    document.%s.elements['%s'].value =  strUser;  \
    document.%s.submit();", formName, formName, varName, formName);
hOnClickButton(formName, javaScript, "add track");

printf("<BR>");
printf("<BR>");
printf("<BR>");
hOnClickButton("selVar_AddAllVis", "document.addVisTrackForm.submit(); return false;", "Add All Visibile Wiggles");
}

void doMainPage(char *db, struct grp *groupList,  struct trackDb *fullTrackList, struct composite *currentComposite, struct composite *compositeList, struct mathTrack *currentMathTrack)
/* Print out initial HTML of control page. */
{
jsInit();
webIncludeResourceFile("jquery-ui.css");
webIncludeResourceFile("ui.dropdownchecklist.css");
addSomeCss();
#ifdef NOTNOW
printAssemblySection();

puts("<BR>");
printf("<div class='sectionLiteHeader noReorderRemove'>"
       "Add Hubs and Custom Tracks </div>\n");
printCtAndHubButtons();
#endif

hPrintf("<FORM ACTION='%s' NAME='changeGroupForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsCurrentGroup, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='changeCompositeForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsCurrentComposite, "");
hPrintf("</FORM>\n");

if (currentComposite)
    {
    hPrintf("<FORM ACTION='%s' NAME='deleteCompositeForm'>", cgiScriptName());
    cgiMakeHiddenVar("deleteComposite", currentComposite->name);
    cartSaveSession(cart);
    hPrintf("</FORM>\n");
    }

hPrintf("<FORM ACTION='%s' NAME='makeMathWigForm'>", cgiScriptName());
cgiMakeHiddenVar("makeMathWig", "on");
cartSaveSession(cart);
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='makeNewCompositeForm'>", cgiScriptName());
cgiMakeHiddenVar("createNewComposite", "on");
cartSaveSession(cart);
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='editMathTrackForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsNewMathTrackShortLabel, "");
cgiMakeHiddenVar(hgsNewMathTrackLongLabel, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='editCompositeForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsNewCompositeShortLabel, "");
cgiMakeHiddenVar(hgsNewCompositeLongLabel, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='addVisTrackForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsAddVisTrack, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='deleteTrackForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsDeleteTrack, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='deleteMathTrackForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsDeleteMathTrack, "");
hPrintf("</FORM>\n");

hPrintf("<FORM ACTION='%s' NAME='addMathTrackForm'>", cgiScriptName());
cartSaveSession(cart);
cgiMakeHiddenVar(hgsAddMathTrack, "");
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

printf("</FORM>");
puts("<BR>");
puts("<BR>");
printCompositeList(compositeList, currentComposite);
puts("<BR>");
printCompositeLabels(currentComposite);
puts("<BR>");

printf("<div id=\"tabs\">"
       "<ul> <li><a href=\"#addTracksToComposite\">Add Tracks To Composite</a></li>"
              "<li><a href=\"#addTracksToMathWig\">Add Tracks to Math Wig</a></li> "
                     "</ul> ");

printf("<div id=\"addTracksToComposite\" class=\"hubList\"> \n");
printTrackList(currentComposite);
availableTracks(db, groupList, fullTrackList, "addTrackForm", hgsAddTrack );
puts("<BR>");
printf("</div>");

printf("<div id=\"addTracksToMathWig\" class=\"hubList\"> \n");
if (printMakeMath(currentMathTrack))
    availableTracks(db, groupList, fullTrackList, "addMathTrackForm", hgsAddMathTrack);
puts("<BR>");
printf("</div>");
printf("</div>");
printf("</FORM>");
jsReloadOnBackButton(cart);

webNewSection("Using the Composite Builder");
webIncludeHelpFileSubst("hgCompositeHelp", NULL, FALSE);
jsIncludeFile("jquery-ui.js", NULL);
jsIncludeFile("hgVarAnnogrator.js", NULL);
jsIncludeFile("ui.dropdownchecklist.js", NULL);
jsIncludeFile("ddcl.js", NULL);
}

void doUi(char *db, struct grp *groupList, struct trackDb *fullTrackList,struct composite *currentComposite, struct composite *compositeList, struct mathTrack *currentMathTrack) 
/* Set up globals and make web page */
{
cartWebStart(cart, db, "Composite Editor");
jsIncludeFile("jquery.js", NULL);
jsIncludeFile("utils.js", NULL);
jsIncludeFile("jquery-ui.js", NULL);

webIncludeResourceFile("jquery-ui.css");

jsIncludeFile("ajax.js", NULL);
jsIncludeFile("hgHubConnect.js", NULL);
jsIncludeFile("jquery.cookie.js", NULL);

doMainPage(database, groupList, fullTrackList, currentComposite, compositeList, currentMathTrack);
cartWebEnd();
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
char *skipHub = trackHubSkipHubName(tdb->track);
if (hashLookup(nameHash, skipHub) == NULL)
    {
    hashAdd(nameHash, tdb->track, tdb);
    return skipHub;
    }

unsigned count = 0;
char buffer[4096];

for(;; count++)
    {
    safef(buffer, sizeof buffer, "%s%d", skipHub, count);
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
    return subTdb->visibility != tvHide;

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


struct mathTrack *getMathTracks(struct composite *compositeList)
{
struct mathTrack *mathTrackList = NULL;

return mathTrackList;
}

static char *createNewCompositeName(struct hash *nameHash)
{
static int compositeNumber = 0;
char buffer[4096];
for(;;)
    {
    safef(buffer, sizeof buffer, "userTrack%d", compositeNumber);
    if (hashLookup(nameHash, buffer) == NULL)
        {
        hashStore(nameHash, buffer);
        return cloneString(buffer);
        }
    compositeNumber++;
    }
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

char *hubName = getHubName(database);
struct hash *nameHash = newHash(5);
struct composite *compositeList = getCompositeList(database, hubName, nameHash, wigTracks);

char *deleteCompositeName = cgiOptionalString("deleteComposite");
if (deleteCompositeName)
    {
    struct composite *composite, *prevComposite = NULL;
    for (composite = compositeList; composite; prevComposite = composite, composite = composite->next)
        {
        if (sameString(composite->name, deleteCompositeName))
            break;
        }
    if (composite)
        {
        if (composite == compositeList)
            compositeList = compositeList->next;
        else
            prevComposite->next = composite->next;
        }
    }

struct composite *currentComposite = NULL;
char *currentCompositeName = cartOptionalString(cart, hgsCurrentComposite);

if (currentCompositeName != NULL)
    {
    for (currentComposite = compositeList; currentComposite; currentComposite = currentComposite->next)
        if (sameString(currentComposite->name, currentCompositeName))
            break;
    }
if (currentCompositeName == NULL)
    currentComposite = compositeList;

if (currentComposite == NULL)
    currentComposite = compositeList;

char *createNewComposite = cartOptionalString(cart, "createNewComposite");
if (createNewComposite != NULL)
    {
    struct composite *composite;
    AllocVar(composite);
    slAddHead(&compositeList, composite);
    currentComposite = composite;

    composite->name = createNewCompositeName(nameHash);
    composite->shortLabel = cloneString("Short Label");
    composite->longLabel = cloneString("Long Label");
    }

char *newShortLabel = cgiOptionalString(hgsNewCompositeShortLabel);
if (newShortLabel != NULL)
    {
    if (currentComposite != NULL)
        {
        currentComposite->shortLabel = cloneString(newShortLabel);
        currentComposite->longLabel = cloneString(cgiOptionalString(hgsNewCompositeLongLabel));
        }
    }

struct mathTrack *currentMathTrack = NULL;

char *currentMathTrackName = cartOptionalString(cart, hgsCurrentMathTrack);
char *makeMathWig = cartOptionalString(cart, "makeMathWig");

if (makeMathWig != NULL)
    {
    AllocVar(currentMathTrack);
    if (currentComposite == NULL)
        errAbort("need currentComposite");
    slAddHead(&currentComposite->trackList, currentMathTrack);
    currentMathTrack->name = createNewCompositeName(nameHash);
    currentMathTrack->shortLabel = cloneString("MathWig Short");
    currentMathTrack->longLabel = cloneString("MathWig Long Label");
    cartSetString(cart, hgsCurrentMathTrack, currentMathTrack->name);
    struct trackDb *tdb;
    AllocVar(tdb);
    hashAdd(nameHash, currentMathTrack->name, tdb);
    tdb->track = currentMathTrack->name;
    tdb->type = "mathWig";
    tdb->settingsHash = newHash(5);
    hashAdd(tdb->settingsHash, "shortLabel",currentMathTrack->shortLabel);
    hashAdd(tdb->settingsHash, "longLabel", currentMathTrack->longLabel);
    hashAdd(tdb->settingsHash, "type", "mathWig");
    hashAdd(tdb->settingsHash, "mathDataUrl", "");
    }
else if (currentMathTrackName != NULL)
    {
    if (currentComposite == NULL)
        errAbort("need currentComposite");
    for (currentMathTrack = (struct mathTrack  *)currentComposite->trackList; currentMathTrack; currentMathTrack = currentMathTrack->next)
        if (sameString(currentMathTrack->name, currentMathTrackName))
            break;
    }

newShortLabel = cgiOptionalString(hgsNewMathTrackShortLabel);
if (newShortLabel != NULL)
    {
    if (currentMathTrack != NULL)
        {
        struct trackDb *tdb = hashMustFindVal(nameHash, currentMathTrack->name);
        hashReplace(tdb->settingsHash, "shortLabel",cloneString(newShortLabel));
        hashReplace(tdb->settingsHash, "longLabel", cloneString(cgiOptionalString(hgsNewMathTrackLongLabel)));
        }
    }

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

char *newMathTrackName = cartOptionalString(cart, hgsAddMathTrack);
if (newMathTrackName != NULL)
    {
    if (currentMathTrack == NULL)
        warn("cannot add track without specifying a mathwig");
    else
        {
        struct trackDb *tdb = findTrack(newMathTrackName, wigTracks);

        struct track *track;
        AllocVar(track);
        track->name = makeUnique(nameHash,  tdb);
        track->shortLabel = tdb->shortLabel;
        track->longLabel = tdb->longLabel;
        slAddHead(&currentMathTrack->trackList, track);
        }
    }

char *deleteTrackName = cartOptionalString(cart, hgsDeleteTrack);
if (deleteTrackName != NULL)
    {
    if (currentComposite == NULL)
        warn("cannot delete track without specifying a composite");
    else
        {
        struct track *track, *prevTrack = NULL;
        for(track = currentComposite->trackList; track; prevTrack = track,track = track->next)
            {
            if (sameString(track->name, deleteTrackName))
                break;
            }
        if (track != NULL)
            {
            if (prevTrack == NULL)
                currentComposite->trackList = track->next;
            else
                prevTrack->next = track->next;
            }
        }
    }

char *deleteMathTrackName = cartOptionalString(cart, hgsDeleteMathTrack);
if (deleteMathTrackName != NULL)
    {
    if (currentMathTrack == NULL)
        warn("cannot delete track without specifying a mathwig");
    else
        {
        struct track *track, *prevTrack = NULL;
        for(track = currentMathTrack->trackList; track; prevTrack = track,track = track->next)
            {
            if (sameString(track->name, deleteMathTrackName))
                break;
            }
        if (track != NULL)
            {
            if (prevTrack == NULL)
                currentMathTrack->trackList = track->next;
            else
                prevTrack->next = track->next;
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

doUi(database, groupList, wigTracks, currentComposite, compositeList, currentMathTrack);

outputCompositeHub(database, hubName,  compositeList, nameHash, wigTracks);
cartCheckout(&cart);
cgiExitTime("hgComposite", enteredMainTime);
return 0;
}
