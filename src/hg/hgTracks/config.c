/* config - put up track and display configuration page. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "dystring.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hCommon.h"
#include "cart.h"
#include "web.h"
#include "customTrack.h"
#include "hgTracks.h"
#include "trashDir.h"
#include "hgConfig.h"
#include "jsHelper.h"
#include "imageV2.h"
#include "search.h"
#include "hubConnect.h"
#include "fileUi.h"
#include "trackHub.h"
#include "versionInfo.h"
#include "jsonWrite.h"
#include "cartTrackDb.h"
#include "errCatch.h"
#include "bigBed.h"
#include "chromAlias.h"
#include "multiRegionSpace.h"

static void themeMenuEntry(char *cfgName, char **retLabel, char **retValue)
/* Split an hg.conf browser.theme.* key into the menu label and the option value.
 * The value is everything after "browser.theme.", which is what setThemeFromCart
 * looks the theme up by, so it must survive the round trip through the form
 * untouched.  The label is only the part after the last '.', with underscores as
 * spaces, so a sort prefix like browser.theme.2.Sans_Serif shows as "Sans Serif".
 * The first letter is upper cased, which the plain hDropList used to do for us. */
{
char *value = cloneString(cfgName + strlen("browser.theme."));
char *label = cloneString(findTail(value, '.'));
replaceChar(label, '_', ' ');
label[0] = toupper((unsigned char)label[0]);
*retLabel = label;
*retValue = value;
}

static void themeDropDown(struct cart* cart)
/* Create drop down for UI themes.
 * specified in hg.conf like this
 * browser.theme.modern=theme-modern.css
 * optionally with a sort prefix
 * browser.theme.3.Sans_Serif=theme-modern.css
 * */
{
struct slName* themes = cfgNamesWithPrefix("browser.theme.");
if (themes==NULL)
    return;

slNameSort(&themes);
hPrintf("<TR><TD>website style:");
hPrintf("<TD style=\"text-align: right\">");

char *labels[50];
char *values[50];
struct slName* el;
int i = 0;
for (el = themes; el != NULL && i<50; el = el->next)
    {
    themeMenuEntry(el->name, &labels[i], &values[i]);
    i++;
    }

char* currentTheme = cartOptionalString(cart, "theme"); 
cgiMakeDropListWithVals("theme", labels, values, i, currentTheme);
slFreeList(themes);
hPrintf("</TD>");
}

/* HOW TO ADD A NEW FONT
 *
 * The browser draws track text with either the old bitmap engine or, when
 * freeType is on (hg.conf "freeType=on", the default in a FreeType build), the
 * FreeType engine.  The fonts the FreeType engine offers are the freeTypeFonts[]
 * table below.  To add one:
 *
 *   1. Put the font file where the engine can find it.  At run time the file is
 *      looked up under freeTypeDir, which defaults to "../htdocs/urw-fonts".
 *      That path is relative to the CGI's working directory, so it resolves to
 *      the *shared* htdocs/urw-fonts even from a per-user sandbox -- you do not
 *      need a urw-fonts directory under your own htdocs-USER.  (You can point
 *      somewhere else with "freeTypeDir" in hg.conf.)  Both Type-1 (.pfb) and
 *      TrueType (.ttf) files work.
 *
 *   2. Add a row to freeTypeFonts[] giving the name and the file.  The name is
 *      what shows up in the configure-page Font dropdown.  The configure page
 *      splits the name on the first '-' into a face and a style: the part before
 *      the '-' is the face shown in the Font menu, the part after is an entry in
 *      the Style menu.  So a plain weight is just the face name ("Lexend") and a
 *      variant is "Face-Style" ("Lexend-Bold").  Group a face's variants on
 *      consecutive rows so the Style menu lists them together.
 *
 * Gotchas:
 *
 *   - Avoid variable fonts (e.g. InterVariable.ttf).  FreeType is opened on face
 *     index 0 with no named instance selected, so a variable font renders only
 *     its default master -- usually not the weight you expected, which looks like
 *     "the option appeared but a different font was drawn".  Use a static,
 *     single-weight file instead (e.g. Inter-Regular.ttf).
 *
 *   - The name in the dropdown must match the name in this table exactly,
 *     including the face/style split above.  A selected name that matches no row
 *     falls back to the bitmap engine (see maybeNewFonts).
 */

struct freeTypeFont
/* A font offered to the FreeType text engine.  The name and its file live on
 * one row so the two can never drift out of sync (this used to be two parallel
 * arrays indexed by position, which silently rendered the wrong font when they
 * disagreed). */
    {
    char *name;	/* Label shown in the configure-page dropdown.  A normal-weight
		 * font is just the face ("Lexend"); a variant is "Face-Style"
		 * ("Lexend-Bold").  The dropdown splits this on the first '-'. */
    char *file;	/* Font file (.pfb Type-1 or .ttf TrueType) found under
		 * freeTypeDir (hg.conf freeTypeDir, default ../htdocs/urw-fonts). */
    };

struct freeTypeFont freeTypeFonts[] = {
{"AvantGarde-Book",			"a010013l.pfb"},
{"AvantGarde-Demi",			"a010015l.pfb"},
{"AvantGarde-BookOblique",		"a010033l.pfb"},
{"AvantGarde-DemiOblique",		"a010035l.pfb"},
{defaultVectorFont,			defaultVectorFontFile},
{"Helvetica-Bold",			"n019004l.pfb"},
{"Helvetica-Oblique",			"n019023l.pfb"},
{"Helvetica-BoldOblique",		"n019024l.pfb"},
{"Helvetica-Narrow",			"n019043l.pfb"},
{"Helvetica-Narrow-Bold",		"n019044l.pfb"},
{"Helvetica-Narrow-Oblique",		"n019063l.pfb"},
{"Helvetica-Narrow-BoldOblique",	"n019064l.pfb"},
{"Times-Roman",				"n021003l.pfb"},
{"Times-Bold",				"n021004l.pfb"},
{"Times-Italic",			"n021023l.pfb"},
{"Times-BoldItalic",			"n021024l.pfb"},
{"Courier",				"n022003l.pfb"},
{"Courier-Bold",			"n022004l.pfb"},
{"Courier-Oblique",			"n022023l.pfb"},
{"Courier-BoldOblique",			"n022024l.pfb"},
{"ZapfChancery-MediumItalic",		"z003034l.pfb"},
{"Atkinson",				"AtkinsonHyperlegible-Regular.ttf"},
{"Atkinson-Bold",			"AtkinsonHyperlegible-Bold.ttf"},
{"Atkinson-Oblique",			"AtkinsonHyperlegible-Italic.ttf"},
{"Atkinson-BoldOblique",		"AtkinsonHyperlegible-BoldItalic.ttf"},
{"Lexend",				"Lexend-Regular.ttf"},
{"Lexend-Bold",				"Lexend-Bold.ttf"},
};

char *emptyStyles[] = {
"Normal"
};

static boolean freeTypeOn()
{
#ifdef USE_FREETYPE
char *defaultState = "on";
#else // USE_FREETYPE
char *defaultState = "off";
#endif // USE_FREETYPE

return sameString(cfgOptionDefault("freeType", defaultState), "on");
}

static char *chosenFreeTypeFont(char **retFontName)
/* Return the file holding the FreeType font the user has picked, and its name in
 * retFontName.  NULL means stay on the bitmap engine. */
{
if (!freeTypeOn())
    return NULL;

if (sameString(tl.textFont, "Bitmap"))
    return NULL;

int ii;
for(ii=0; ii < ArraySize(freeTypeFonts); ii++)
    if (sameString(freeTypeFonts[ii].name, tl.textFont))
        break;
if (ii == ArraySize(freeTypeFonts))
    return NULL;   // not a font we know about; leave the bitmap engine in place

static char buffer[PATH_LEN];
char *fontDir = cfgOptionDefault("freeTypeDir", "../htdocs/urw-fonts");
safef(buffer, sizeof buffer, "%s/%s", fontDir, freeTypeFonts[ii].file);
*retFontName = freeTypeFonts[ii].name;
return buffer;
}

boolean freeTypeFontActive()
/* TRUE when the FreeType font engine is the one maybeNewFonts() will actually switch to.  Callers
 * that pick a font to match the live engine (e.g. squishCodonFont) must use this rather than a
 * looser test, or they can hand the bitmap engine a cell height it cannot render. */
{
char *fontName = NULL;
return chosenFreeTypeFont(&fontName) != NULL;
}

/* Cart variables behind the FreeType tuning controls.  They only do anything while
 * hg.conf freeTypeTuneUI is on, so a value left behind by an experiment cannot change
 * what a server without that setting draws. */
#define ftTuneSizeScaleVar  "freeTypeTune.sizeScale"
#define ftTuneBaselineVar   "freeTypeTune.baselineDelta"
#define ftTuneGammaVar      "freeTypeTune.gamma"
#define ftTuneBoxScaleVar   "freeTypeTune.boxScale"
#define ftTuneHintingVar    "freeTypeTune.hinting"
#define ftTuneAntiAliasVar  "freeTypeTune.antiAlias"
#define ftTunePrefix        "freeTypeTune."

boolean freeTypeTuneUiOn()
/* Is the FreeType tuning section switched on?  It is a debugging aid, off everywhere
 * until someone puts freeTypeTuneUI=on in hg.conf. */
{
return cfgOptionBooleanDefault("freeTypeTuneUI", FALSE);
}

void freeTypeTuneResetCart()
/* Drop the FreeType tuning variables, returning the rendering to the stock settings. */
{
cartRemovePrefix(cart, ftTunePrefix);
}

static void applyFreeTypeTuning()
/* Push the tuning from the cart into the FreeType code.  Defaults reproduce the stock
 * rendering, so this is a no-op until someone moves a control. */
{
struct ftTuning tuning;
ftTuningDefaults(&tuning);
if (freeTypeTuneUiOn())
    {
    tuning.sizeScale = cartUsualDouble(cart, ftTuneSizeScaleVar, tuning.sizeScale);
    tuning.baselineDelta = cartUsualInt(cart, ftTuneBaselineVar, tuning.baselineDelta);
    tuning.gamma = cartUsualDouble(cart, ftTuneGammaVar, tuning.gamma);
    tuning.boxScale = cartUsualDouble(cart, ftTuneBoxScaleVar, tuning.boxScale);
    tuning.hinting = cartUsualInt(cart, ftTuneHintingVar, tuning.hinting);
    tuning.antiAlias = cartUsualInt(cart, ftTuneAntiAliasVar, tuning.antiAlias);
    }
ftSetTuning(&tuning);
}

void initFontEngine()
/* Load the text engine the user has picked, before anything measures a string.
 * Pack mode reserves room for an item by measuring its label, and
 * mgFontStringWidth answers from whichever engine is loaded at the time.  Doing
 * this up front keeps the packing from depending on whether some earlier image
 * on the page -- the ideogram -- happened to load FreeType first. */
{
char *fontName = NULL;
char *fontFile = chosenFreeTypeFont(&fontName);
if (fontFile != NULL)
    mgLoadFontEngine(FONT_METHOD_FREETYPE, fontFile);
applyFreeTypeTuning();
}

void maybeNewFonts(struct hvGfx *hvg)
/* Check to see if we want to use the alternate font engine (FreeType2). */
{
char *fontName = NULL;
char *fontFile = chosenFreeTypeFont(&fontName);
if (fontFile != NULL)
    hvGfxSetFontMethod(hvg, FONT_METHOD_FREETYPE, fontName, fontFile);
applyFreeTypeTuning();
}

static void freeTypeTuneUi()
/* Rough controls for experimenting with the FreeType rendering, shown only when hg.conf
 * freeTypeTuneUI is on.  Every control's default is the stock rendering. */
{
struct ftTuning stock;
ftTuningDefaults(&stock);

hPrintf("<TR><TD colspan=4><HR>FreeType tuning (debugging, hg.conf freeTypeTuneUI)</TD></TR>");

hPrintf("<TR><TD>Size scale:<TD style=\"text-align: right\">");
hDoubleVar(ftTuneSizeScaleVar, cartUsualDouble(cart, ftTuneSizeScaleVar, stock.sizeScale), 6);
hPrintf("<TD>multiplies the pixel size picked for the text size, stock %g</TD></TR>",
        stock.sizeScale);

hPrintf("<TR><TD>Baseline shift:<TD style=\"text-align: right\">");
hIntVar(ftTuneBaselineVar, cartUsualInt(cart, ftTuneBaselineVar, stock.baselineDelta), 6);
hPrintf("<TD>pixels added to the baseline, stock %d</TD></TR>", stock.baselineDelta);

hPrintf("<TR><TD>Coverage gamma:<TD style=\"text-align: right\">");
hDoubleVar(ftTuneGammaVar, cartUsualDouble(cart, ftTuneGammaVar, stock.gamma), 6);
hPrintf("<TD>exponent on a pixel's coverage, under 1 is heavier, stock %g</TD></TR>",
        stock.gamma);

hPrintf("<TR><TD>Box size scale:<TD style=\"text-align: right\">");
hDoubleVar(ftTuneBoxScaleVar, cartUsualDouble(cart, ftTuneBoxScaleVar, stock.boxScale), 6);
hPrintf("<TD>the multiplier text drawn to fit a box uses, stock %g</TD></TR>",
        stock.boxScale);

static char *hintLabels[] = {"default", "none", "force autohint"};
static char *hintValues[] = {"0", "1", "2"};
char hintNow[16];
safef(hintNow, sizeof hintNow, "%d", cartUsualInt(cart, ftTuneHintingVar, stock.hinting));
hPrintf("<TR><TD>Hinting:<TD style=\"text-align: right\">");
cgiMakeDropListFull(ftTuneHintingVar, hintLabels, hintValues, ArraySize(hintValues),
                    hintNow, NULL, NULL);
hPrintf("<TD>what to ask FreeType for, stock default</TD></TR>");

static char *aaLabels[] = {"Normal", "Light", "Light (Sub Pixel)", "LCD (RGB)",
                           "LCD (BGR)", "LCD (vert. RGB)", "LCD (vert. BGR)"};
static char *aaValues[] = {"0", "1", "2", "3", "4", "5", "6"};
char aaNow[16];
safef(aaNow, sizeof aaNow, "%d", cartUsualInt(cart, ftTuneAntiAliasVar, stock.antiAlias));
hPrintf("<TR><TD>Anti-aliasing:<TD style=\"text-align: right\">");
cgiMakeDropListFull(ftTuneAntiAliasVar, aaLabels, aaValues, ArraySize(aaValues),
                    aaNow, NULL, NULL);
hPrintf("<TD>the render target, stock Normal.  The LCD modes need auto-hinting to be "
        "worth looking at.</TD></TR>");

hPrintf("<TR><TD colspan=2>");
cgiMakeButton(configFreeTypeReset, "Reset FreeType");
hPrintf("<TD>puts every control above back to the stock setting</TD></TR>");

hPrintf("<TR><TD colspan=4><HR></TD></TR>");
}

static void textFontDropDown()
/* Create drop down for font size. */
{
/* get current values for font and style */
char *currentFontName = cloneString(tl.textFont);
char *currentStyle = strchr(currentFontName, '-');
if (currentStyle)
    *currentStyle++ = 0;
else
    currentStyle = "Normal";

char *faceNames[ArraySize(freeTypeFonts) + 1];  // +1 for the "Bitmap" entry below
int ii;
int numFonts = 0;
struct dyString *dy = dyStringNew(1024);
dyStringPrintf(dy, "  fontStyles = [];\n");

int numStyle = 0;
char *lastName = NULL;

faceNames[numFonts++] = "Bitmap";
dyStringPrintf(dy, "  fontStyles['Bitmap'] = ['Normal'];");

for (ii=0; ii < ArraySize(freeTypeFonts); ii++)
    {
    char *fontName = cloneString(freeTypeFonts[ii].name);
    char *style = strchr(fontName, '-');

    if (style)
        *style++ = 0;

    if ((lastName == NULL) || differentString(lastName, fontName))
        {
        faceNames[numFonts] = fontName;
        if (lastName != NULL)
            dyStringPrintf(dy, "  ];\n");

        dyStringPrintf(dy, "  fontStyles['%s'] = [", fontName);
        numStyle = 0;
        numFonts++;
        }

    if (style == NULL)
        style = cloneString("Normal");
    if (numStyle)
        dyStringPrintf(dy, ",");
    dyStringPrintf(dy, "'%s'", style);
    numStyle++;

    lastName = fontName;
    }
    dyStringPrintf(dy, "  ];\n");

dyStringPrintf(dy, "$(\"[name='%s']\").change(function()\n", textFontVar);
dyStringPrintf(dy, "{\n");
dyStringPrintf(dy, "  $(\"[name='textStyle']\").empty();");
dyStringPrintf(dy, "  val= $(this).find(':selected').val(); \n");
dyStringPrintf(dy, "  if (fontStyles[val].length == 1) {\n");
dyStringPrintf(dy, "    $(\"[id='textStyleDrop']\").hide();$(\"[id='textStyleName']\").hide();\n");
dyStringPrintf(dy, "    $(\"[name='textStyle']\").val('Normal');\n");
dyStringPrintf(dy, "  } else {\n");
dyStringPrintf(dy, "    $(\"[id='textStyleDrop']\").show();$(\"[id='textStyleName']\").show();\n");
dyStringPrintf(dy, "  }\n");
dyStringPrintf(dy, "  for(ii=0; ii < fontStyles[val].length; ii++) { $(\"[name='textStyle']\").append( new Option(fontStyles[val][ii],fontStyles[val][ii],))};\n");
dyStringPrintf(dy, "});\n");
dyStringPrintf(dy, "$(\"[name='textFont']\").change();\n");
dyStringPrintf(dy, "$(\"[name='textStyle']\").val('%s');\n", currentStyle);
jsInline(dy->string);

hDropList(textFontVar, faceNames, numFonts, currentFontName);
jsInline("$('[name=\"textFont\"]')[0].style.width='15em';\n"); // hDropList has no 'style' nor 'id' argument <-> no opt args in C
}

static void textStyleDropDown()
/* Create drop down for font size. */
{
hDropList(textStyleVar, emptyStyles, ArraySize(emptyStyles), emptyStyles[0]);
}

static void textSizeDropDown()
/* Create drop down for font size. */
{
static char *sizes[] = {"6", "8", "10", "12", "14", "18", "24", "34"};
hDropList(textSizeVar, sizes, ArraySize(sizes), tl.textSize);
}

static void addSuperMembers(struct trackRef **refList, struct hash *trackHash,
                            struct trackDb *superTdb)
/* Add a supertrack's members to refList, and the members of any supertrack among them.
 * A supertrack may hold another supertrack, whose own members are reached only through it. */
{
struct slRef *child;
for (child = superTdb->children; child != NULL; child = child->next)
    {
    struct trackDb *childTdb = child->val;
    struct track *childTrack = hashFindVal(trackHash, childTdb->track);
    // Try adding downloadsOnly track
    if (childTrack == NULL && tdbIsDownloadsOnly(childTdb))
        {
        AllocVar(childTrack);           // Fake a track!
        childTrack->tdb = childTdb;
        childTrack->hasUi = FALSE;
        }
    if (childTrack != NULL)
        {
        struct trackRef *ref;
        AllocVar(ref);
        ref->track = childTrack;
        slAddTail(refList, ref);
        }
    if (tdbIsSuper(childTdb))
        addSuperMembers(refList, trackHash, childTdb);
    }
}

static int superTrackDepth(struct trackDb *tdb)
/* How many supertracks are above this track. */
{
int depth = 0;
struct trackDb *ancestor;
for (ancestor = tdb->parent; ancestor != NULL; ancestor = ancestor->parent)
    if (tdbIsSuper(ancestor))
        depth++;
return depth;
}

static void trackConfig(struct track *trackList, struct group *groupList,
	char *groupTarget,  int changeVis)
/* Put up track configurations. If groupTarget is
 * NULL then set visibility for tracks in all groups.  Otherwise,
 * just set it for the given group.  If vis is -2, then visibility is
 * unchanged.  If -1 then set visibility to default, otherwise it should
 * be tvHide, tvDense, etc. */
{
struct group *group;
boolean showedRuler = FALSE;

setRulerMode();
changeTrackVis(groupList, groupTarget, changeVis);

/* Set up ruler mode according to changeVis. */
#ifdef BOB_DOESNT_LIKE
if (changeVis != -2)
    {
    if (groupTarget == NULL ||
        (groupList != NULL && sameString(groupTarget, groupList->name)))
	{
	if (changeVis == -1)
	    rulerMode = tvFull;
	else
	    rulerMode = changeVis;
	}
    }
#endif /* BOB_DOESNT_LIKE */

jsInit();
cgiMakeHiddenVar(configGroupTarget, "none");

// Now all groups are in a single table, divided by an empty borderless row
hPrintf("<TABLE BORDER='0' CELLSPACING='0' class='groupLists'>\n");
struct hash *superHash = hashNew(8);
for (group = groupList; group != NULL; group = group->next)
    {
    struct trackRef *tr;

    if (group->trackList == NULL)
	continue;

    /* check if group section should be displayed */
    char *otherState;
    char *indicator;
    char *indicatorImg;
    boolean isOpen = !isCollapsedGroup(group);
    collapseGroupGoodies(isOpen, FALSE, &indicatorImg,
                            &indicator, &otherState);
    hPrintf("<TR NOWRAP class='nativeToggleBar'>");
    hPrintf("<TH NOWRAP align='left' colspan=3>");
    hPrintf("<table style='width:100%%;'><tr class='noData'><td style='text-align:left;'>");
    hPrintf("\n<A NAME='%sGroup'></A>",group->name);
    hPrintf("<input type=hidden name='%s' id='%s' value=%d>",
            collapseGroupVar(group->name),collapseGroupVar(group->name), (isOpen?0:1));
    char idText[256];
    safef(idText, sizeof idText, "%s_button", group->name);
    hPrintf("<IMG class='toggleButton' "
            "id='%s' src='%s' alt='%s' title='%s this group'>&nbsp;&nbsp;",
            idText, indicatorImg, indicator,isOpen?"Collapse":"Expand");
    jsOnEventByIdF("click", idText, "return vis.toggleForGroup(this,'%s');", group->name);

    // a hub group's label is built from the hub's shortLabel and its groups.txt label
    hPrintf("<B>&nbsp;%s</B> ", htmlEncode(group->label));
    hPrintf("&nbsp;&nbsp;&nbsp;");
    hPrintf("</td><td style='text-align:right;'>\n");
    safef(idText, sizeof idText, "%s_hideAllBut", group->name);
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"%s\" id='%s' VALUE=\"%s\" "
            "title='Hide all tracks in this group'>",
	    configHideAll, idText, "Hide all");
    // TODO XSS filter configGroupTarget
    char jsText[256]; 
    // used several times
    safef(jsText, sizeof jsText, "document.mainForm.%s.value='%s'; %s",
	    configGroupTarget, group->name, jsSetVerticalPosition("mainForm"));
    jsOnEventById("click", idText, jsText);
    hPrintf(" ");
    safef(idText, sizeof idText, "%s_showAllBut", group->name);
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"%s\" id='%s' VALUE=\"%s\" "
            "title='Show all tracks in this groups'>",
	    configShowAll, idText, "Show all");
    jsOnEventById("click", idText, jsText);
    hPrintf(" ");
    safef(idText, sizeof idText, "%s_defaultBut", group->name);
    hPrintf("<INPUT TYPE=SUBMIT NAME=\"%s\" id='%s' VALUE=\"%s\" "
            "title='Show default tracks in this group'>",
	    configDefaultAll, idText, "Default");
    jsOnEventById("click", idText, jsText);
    hPrintf(" ");
    /* do not want all the submit buttons named the same.  It is
     * confusing to the javascript submit() function.
     */
    char submitName[256];
    safef(submitName, sizeof(submitName), "%sSubmit", group->name);
    cgiMakeButtonWithMsg(submitName, "Submit","Submit your selections and view them in the browser");
    hPrintf("</td></tr></table>\n");
    hPrintf("</TH></TR>\n");

    /* First non-CT, non-hub group gets ruler. */
    if (!showedRuler && !isHubTrack(group->name) &&
                differentString(group->name, "user"))
	{
        showedRuler = TRUE;
        hPrintf("<TR %sid='%s-0'>",(isOpen ? "" : "style='display: none'"), group->name);
	hPrintf("<TD>");
        hPrintf("<A HREF=\"%s?%s=%s&db=%s&c=%s&g=%s&hgTracksConfigPage=configure\">", hgTrackUiName(),
                cartSessionVarName(), cartSessionId(cart),
                database, chromName, RULER_TRACK_NAME);
        hPrintf("%s</A>", RULER_TRACK_LABEL);
	hPrintf("</TD><TD>");
	hTvDropDownClassVisOnlyWithLabel("ruler", rulerMode, FALSE,
		rulerMode ? "normalText trackVis" : "hiddenText trackVis",
		NULL, RULER_TRACK_LABEL);
	hPrintf("</TD><TD>");
	hPrintf("Chromosome position in bases.  (Clicks here zoom in 3x)");
	hPrintf("</TD></TR>\n");
	}
    /* Scan track list to determine which supertracks have visible member
     * tracks, and to insert a track in the list for the supertrack.
     * Sort tracks and supertracks together by priority */
    makeGlobalTrackHash(trackList);
    groupTrackListAddSuper(cart, group, superHash, trackHash);

    if (!withPriorityOverride)
        {
        /* sort hierarchically by priority, considering supertracks */
        struct trackRef *refList = NULL, *ref;
        for (tr = group->trackList; tr != NULL; tr = tr->next)
            {
            struct track *track = tr->track;
            if (tdbIsSuperTrackChild(track->tdb))
                /* ignore supertrack member tracks till supertrack is found */
                continue;
            AllocVar(ref);
            ref->track = track;
            slAddTail(&refList, ref);
            if (tdbIsSuper(track->tdb))
                addSuperMembers(&refList, trackHash, track->tdb);
            }
        group->trackList = refList;
        }

    /* Loop through this group and display */
    int rowCount=1;
    for (tr = group->trackList; tr != NULL; tr = tr->next)
        {
        struct track *track = tr->track;
        struct trackDb *tdb = track->tdb;

        hPrintf("<TR %sid='%s-%d'>",(isOpen ? "" : "style='display: none;'"),
                group->name, rowCount++);
        hPrintf("<TD NOWRAP>");
        int superDepth = superTrackDepth(tdb);
        int indent;
        for (indent = 0; indent < superDepth; indent++)
            /* indent members of a supertrack, one step per enclosing supertrack */
            hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;");

        hPrintIcons(tdb);

        if (track->hasUi)
            // the labels come from trackDb, which a track hub controls, escape them
            hPrintf("<A TITLE='%s%s...' HREF='%s?%s=%s&db=%s&g=%s&hgTracksConfigPage=configure'>",
                    tdb->parent ? "Part of super track: " : "Configure ",
                    htmlEncode(tdb->parent ? tdb->parent->shortLabel : tdb->shortLabel),
                    hTrackUiForTrack(tdb->track),
                    cartSessionVarName(), cartSessionId(cart), database, track->track);
        hPrintf(" %s", htmlEncode(tdb->shortLabel));
        if (track->hasUi)
	    hPrintf("</A>");
	hPrintf("</TD><TD NOWRAP>");
        for (indent = 0; indent < superDepth; indent++)
            /* indent members of a supertrack, one step per enclosing supertrack */
            hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;");

	/* If track is not on this chrom print an informational
	   message for the user. */
        if (tdbIsDownloadsOnly(tdb))    // No vis display for downloadsOnly
            hPrintf("<A TITLE='Downloadable files...' HREF='%s?%s=%s&g=%s'>Downloads</A>",
                    hgFileUiName(),cartSessionVarName(), cartSessionId(cart), tdb->track);
        else if (hTrackOnChrom(track->tdb, chromName))
	    {
            if (tdbIsSuper(track->tdb))
                {
                /* supertrack dropdown is hide/show */
                superTrackDropDown(cart, track->tdb, 1);
                }
            else
                {
                /* check for option of limiting visibility to one mode */
                hTvDropDownClassVisOnlyWithLabel(track->track, track->visibility,
                                        rTdbTreeCanPack(track->tdb),
                                        (track->visibility == tvHide) ? "hiddenText trackVis" : "normalText trackVis",
                                        trackDbSetting(track->tdb, "onlyVisibility"),
                                        tdb->shortLabel);
                }
	    }
        else
	    hPrintf("[No data-%s]", chromName);
	hPrintf("</TD><TD NOWRAP>");
        hPrintf("%s", htmlEncode(tdb->longLabel));
	hPrintf("</TD></TR>\n");
	}
    hPrintf("<tr class='noData'><td colspan=3>");
    cgiDown(0.9);
    hPrintf("</td></tr>\n");
    }
hashFree(&superHash);
hPrintf("</TABLE>\n");

jsInline("$(document).ready( cfgPageAddListeners )");
}

static int addDownloadOnlyTracks(char *db,struct group **pGroupList,struct track **pTrackList)
// Download only tracks are not normaly incorporated into the grou and track lists
{
int count = 0;
struct track *track = NULL;
struct group *group = NULL;
struct trackDb *tdbList = hTrackDb(db);
struct trackDb *tdb = tdbList;
for (;tdb != NULL; tdb = tdb->next)
    {
    if (!tdbIsDownloadsOnly(tdb)
    || tdbIsFolderContent(tdb)
    || tdbIsCompositeChild(tdb))
        continue;

    // Must find group
    if (tdb->grp == NULL)
        continue;

    for (group = *pGroupList;group != NULL; group = group->next)
        {
        if (sameWord(group->name,tdb->grp))
            break;
        }
    if (group == NULL)
        continue;

    // Make the track
    track = trackFromTrackDb(tdb);
    track->group = group;
    track->groupName = cloneString(group->name);
    slAddHead(pTrackList,track);
    count++;
    }

if (count > 0)
    {
    // Going to have to make all new group->trackLists
    slSort(pGroupList, gCmpPriority);
    for (group = *pGroupList;group != NULL; group = group->next)
        slFreeList(&group->trackList);

    // Sort the tracks anew and add each on into it's group.
    slSort(pTrackList, tgCmpPriority);
    for (track = *pTrackList; track != NULL; track = track->next)
        {
        struct trackRef *tr;
        AllocVar(tr);
        tr->track = track;
        slAddHead(&track->group->trackList, tr);
        }

    /* Straighten things out, clean up, and go home. */
    for (group = *pGroupList;group != NULL; group = group->next)
        slReverse(&group->trackList);
    }
return count;
}


void configInitTrackList(
    int vis, 
    char **pGroupTarget,
    struct track **pTrackList,
    struct track **pIdeoTrack,
    struct group **pGroupList
)
{
char *groupTarget = NULL;
struct track *trackList =  NULL;
struct track *ideoTrack = NULL;
struct group *groupList = NULL;

withPriorityOverride = cartUsualBoolean(cart, configPriorityOverride, FALSE);

/* Get track list and group them. */
ctList = customTracksParseCart(database, cart, &browserLines, &ctFileName);
trackList = getTrackList(&groupList, vis);

if (trackHash == NULL)
    trackHash = makeGlobalTrackHash(trackList);
// Subtrack settings must be removed when composite/view settings are updated
parentChildCartCleanup(trackList,cart,oldVars);

addDownloadOnlyTracks(database,&groupList,&trackList);

/* The ideogram for some reason is considered a track.
 * We don't really want to process it as one though, so
 * we see if it's there, and if necessary remove it. */
ideoTrack = chromIdeoTrack(trackList);
if (ideoTrack != NULL)
    removeTrackFromGroup(ideoTrack);

/* Fetch group to change on if any from CGI,
 * and remove var so it doesn't get used again. */
groupTarget = cloneString(cartUsualString(cart, configGroupTarget, ""));
cartRemove(cart, configGroupTarget);
if (sameString(groupTarget, "none"))
    freez(&groupTarget);

*pGroupTarget = groupTarget;
*pTrackList = trackList;
*pIdeoTrack = ideoTrack;
*pGroupList = groupList;

}

void configPageSetTrackVis(int vis)
/* Do config page after setting track visibility. If vis is -2, then visibility
 * is unchanged.  If -1 then set visibility to default, otherwise it should
 * be tvHide, tvDense, etc. */
{
char *groupTarget;
struct track *trackList;
struct track *ideoTrack;
struct group *groupList;

configInitTrackList(vis, &groupTarget, &trackList, &ideoTrack, &groupList);

struct dyString *title = dyStringNew(0);

dyStringPrintf(title, "Configure Image - Genome Browser V%s", CGI_VERSION);

hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=%s>\n", hgTracksName(),
	cartUsualString(cart, "formMethod", "POST"));
webStartWrapperDetailedNoArgs(cart, database, "", title->string, FALSE, FALSE, FALSE, FALSE);
cartSaveSession(cart);

hPrintf("<INPUT TYPE=HIDDEN NAME=\"hgTracksConfigPage\" VALUE=\"\">");
/* do not want all the submit buttons named the same thing, this one is: */
cgiMakeButton("topSubmit", "Submit");

// 3 column table
hPrintf("<TABLE style=\"border:0px; \">\n");
hPrintf("<TR><TD>Image width:");
hPrintf("<TD style=\"text-align: right\">");
hIntVar("pix", tl.picWidth, 4);
hPrintf("<TD>pixels</TR>");

hPrintf("<TR><TD>Label area width:");
hPrintf("<TD style=\"text-align: right\">");
hIntVar(leftLabelWidthVar, tl.leftLabelWidthChars, 2);
hPrintf("<TD>characters<TD></TR>");

hPrintf("<TR><TD>Text size:");
hPrintf("<TD style=\"text-align: right\">");
textSizeDropDown();
hPrintf("</TD>");
if (trackLayoutInclFontExtras())
    {
    hPrintf("<TD>");
    char *defaultStyle = cartUsualString(cart, "fontType", "medium");
    cartMakeRadioButton(cart, "fontType", "medium", defaultStyle);
    hPrintf("&nbsp;medium&nbsp;");
    cartMakeRadioButton(cart, "fontType", "fixed", defaultStyle);
    hPrintf("&nbsp;fixed&nbsp;");
    cartMakeRadioButton(cart, "fontType", "bold", defaultStyle);
    hPrintf("&nbsp;bold&nbsp;");
    hPrintf("&nbsp;");
    hPrintf("</TD>");
    }
hPrintf("</TR>");

if (freeTypeOn())
    {
    hPrintf("<TR><TD>Font:");
    hPrintf("<TD style=\"text-align: right\">");
    textFontDropDown();
    hPrintf("</TD></TR>");
    hPrintf("<TR><TD id='textStyleName'>Style:");
    hPrintf("<TD style=\"text-align: right\" id='textStyleDrop' >");
    textStyleDropDown();
    hPrintf("</TR>");
    hPrintf("</TR>");
    if (freeTypeTuneUiOn())
        freeTypeTuneUi();
    }

/* I predict most people will want the browser text size as the tooltip text size
 * but just in case, users can change the value and it will remain independent
 * of the font size by saving to localStorage. */
hPrintf("<tr><td>Tooltip text size:</td>");
hPrintf("<td style=\"text-align: right\">");
static char *sizes[] = {"6", "8", "10", "12", "14", "18", "24", "34"};
int i;
hPrintf("<select name='tooltipTextSize'>");
for (i = 0; i < ArraySize(sizes); i++)
    {
    hPrintf("<option ");
    if (sameString(tl.textSize,sizes[i])) {hPrintf("selected");}
    hPrintf(">%s</option>", sizes[i]);
    }
hPrintf("</select>");
hPrintf("</td>");
hPrintf("</tr>");
jsInlineF(""
    "function updateSelectedTooltipSize(newSize) {\n"
    "    let options = document.getElementsByName('tooltipTextSize')[0];\n"
    "    let i = 0;\n"
    "    for (i; i < options.length; i++) {\n"
    "        if (options[i].value === newSize) {\n"
    "            options[i].selected = true;\n"
    "            localStorage.setItem('tooltipTextSize', options[i].value);\n"
    "        } else {\n"
    "            options[i].selected = false;\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "// set the tooltip text size based on localStorage values\n"
    "let currTooltipSize = localStorage.getItem('tooltipTextSize');\n"
    "let browserTextSize = document.getElementsByName('textSize')[0];\n"
    "if (currTooltipSize === null) {\n"
    "    localStorage.setItem('tooltipTextSize', browserTextSize.value);\n"
    "} else {\n"
    "    updateSelectedTooltipSize(currTooltipSize);\n"
    "}"
    "document.getElementsByName('tooltipTextSize')[0].addEventListener('change', function(e) {\n"
    "    updateSelectedTooltipSize(document.getElementsByName('tooltipTextSize')[0].selectedOptions[0].value);\n"
    "});\n"
    );

hPrintf("<TR><TD>Maximum track load time: ");
printInfoIcon("Maximum time in seconds the Genome Browser will wait for any individual track to "
"load. This limit can be hit by slower network connections, or densely populated tracks, especially in large regions.");
hPrintf("<TD style=\"text-align: right\">");
hIntVar("parallelFetch.timeout", getParaLoadTimeout(), 3);
hPrintf("<TD>seconds<TD></TR>");

themeDropDown(cart);

hTableStart();
if (ideoTrack != NULL)
    {
    hPrintf("<TR><TD>");
    hCheckBox("ideogram", cartUsualBoolean(cart, "ideogram", TRUE));
    hPrintf("</TD><TD>");
    hPrintf("Display chromosome ideogram above main graphic");
    hPrintf("</TD></TR>\n");
    }
hPrintf("<TR><TD>");
hCheckBox("guidelines", cartUsualBoolean(cart, "guidelines", TRUE));
hPrintf("</TD><TD>");
hPrintf("Show light blue vertical guidelines, or light red vertical  window separators in multi-region view");
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
hCheckBox("leftLabels", cartUsualBoolean(cart, "leftLabels", TRUE));
hPrintf("</TD><TD>");
hPrintf("Display labels to the left of items in tracks");
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
hCheckBox("centerLabels", cartUsualBoolean(cart, "centerLabels", TRUE));
hPrintf("</TD><TD>");
hPrintf("Display description above each track");
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
hCheckBox("trackControlsOnMain", cartUsualBoolean(cart, "trackControlsOnMain", TRUE));
hPrintf("</TD><TD>");
hPrintf("Show track controls under main graphic");
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
hCheckBox("nextItemArrows", cartUsualBoolean(cart, "nextItemArrows", FALSE));
hPrintf("</TD><TD>");
hPrintf("Next/previous item navigation");
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
hCheckBox("nextExonArrows", cartUsualBoolean(cart, "nextExonArrows", TRUE));
hPrintf("</TD><TD>");
hPrintf("Next/previous exon navigation");
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
hCheckBox("exonNumbers", cartUsualBoolean(cart, "exonNumbers", TRUE));
hPrintf("</TD><TD>");
hPrintf("Show exon numbers");
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
hCheckBox("showDinkButtons", cartUsualBoolean(cart, "showDinkButtons", FALSE));
hPrintf("</TD><TD>");
hPrintf("Show move left/right limit buttons under image");
hPrintf("</TD></TR>\n");

hPrintf("<TR><TD>");
hCheckBox("enableHighlightingDialog", cartUsualBoolean(cart, "enableHighlightingDialog", TRUE));
hPrintf("</TD><TD>");
hPrintf("Enable highlight with drag-and-select "
        "(if unchecked, drag-and-select always zooms to selection)");
hPrintf("</TD></TR>\n");

// put a checkbox, on by default, to control whether item clicks stay on hgTracks or
// go to hgTracks
hPrintf("<tr><td>");
hCheckBox("doHgcInPopUp", cartUsualBoolean(cart, "doHgcInPopUp", TRUE));
hPrintf("<td>Enable pop-up when clicking items</td></tr>\n");

hTableEnd();

// My Annotations preferences.  They are stored in localStorage, so the controls
// have no name, stay out of the cart, and are read and written in the browser.
if (cfgOptionBooleanDefault("doMyVariants", FALSE))
    {
    hPrintf("<P><B>My Annotations</B></P>\n");

    hPrintf("<TABLE style=\"border:0px; \">\n");
    hPrintf("<TR><TD COLSPAN=2>");
    hPrintf("<input type='checkbox' id='myVariantsDeleteAsk'> ");
    hPrintf("Ask before deleting an annotation");
    hPrintf("</TD></TR>\n");

    hPrintf("<TR><TD>After adding an annotation outside the current view:</TD>");
    hPrintf("<TD><select id='myVariantsNavPref'>"
            "<option value=''>Ask</option>"
            "<option value='jump'>Go to the annotation</option>"
            "<option value='stay'>Stay here</option>"
            "</select></TD></TR>\n");

    hPrintf("<TR><TD>After a search:</TD>");
    hPrintf("<TD><select id='myVariantsSearchSavePref'>"
            "<option value=''>Ask whether to save</option>"
            "<option value='save'>Save without asking</option>"
            "<option value='skip'>Do not save</option>"
            "</select></TD></TR>\n");
    hPrintf("</TABLE>\n");

    // Show what each preference is set to now, and store what the controls say
    // when the page is submitted.  An empty value means ask, the default.
    jsInlineF(
        "document.addEventListener('DOMContentLoaded', function() {"
        "    var prefs = [['myVariantsNavPref', 'myVariants_navPref'],"
        "                 ['myVariantsSearchSavePref', 'myVariants_searchSavePref']];"
        "    var delCb = document.getElementById('myVariantsDeleteAsk');"
        "    if (delCb)"
        "        delCb.checked = localStorage.getItem('myVariants_deleteNoConfirm') !== 'yes';"
        "    prefs.forEach(function(p) {"
        "        var sel = document.getElementById(p[0]);"
        "        if (sel)"
        "            sel.value = localStorage.getItem(p[1]) || '';"
        "    });"
        "    var form = document.forms['mainForm'];"
        "    if (form) {"
        "        form.addEventListener('submit', function() {"
        "            if (delCb) {"
        "                if (delCb.checked)"
        "                    localStorage.removeItem('myVariants_deleteNoConfirm');"
        "                else"
        "                    localStorage.setItem('myVariants_deleteNoConfirm', 'yes');"
        "            }"
        "            prefs.forEach(function(p) {"
        "                var sel = document.getElementById(p[0]);"
        "                if (!sel)"
        "                    return;"
        "                if (sel.value)"
        "                    localStorage.setItem(p[1], sel.value);"
        "                else"
        "                    localStorage.removeItem(p[1]);"
        "            });"
        "        });"
        "    }"
        "});"
    );
    }


cgiDown(0.9);

char buf[256];
char *freeze = hFreezeFromDb(database);
if (freeze == NULL)
    safef(buf, sizeof buf, "Configure Tracks on %s %s: %s",
	  organization, browserName, trackHubSkipHubName(organism));
else if (stringIn(database, freeze))
    safef(buf, sizeof buf, "Configure Tracks on %s %s: %s %s",
	  organization, browserName, trackHubSkipHubName(organism), freeze);
else
    safef(buf, sizeof buf, "Configure Tracks on %s %s: %s %s (%s)",
	  organization, browserName, trackHubSkipHubName(organism), freeze, trackHubSkipHubName(database));
webNewSection("%s",buf);
hPrintf("Tracks: ");
if (isSearchTracksSupported(database,cart))
    {
    cgiMakeButtonWithMsg(TRACK_SEARCH, TRACK_SEARCH_BUTTON,TRACK_SEARCH_HINT);
    hPrintf(" ");
    }
cgiMakeButtonWithMsg(configHideAll, "Hide all","Hide all tracks in this genome assembly");
hPrintf(" ");
cgiMakeButtonWithMsg(configShowAll, "Show all","Show all tracks in this genome assembly");
hPrintf(" ");
cgiMakeButtonWithMsg(configDefaultAll, "Default","Display only default tracks");
hPrintf("&nbsp;&nbsp;&nbsp;Groups:  ");
hButtonWithOnClick("hgt.collapseGroups", "Collapse all", "Collapse all track groups",
                   "return vis.expandAllGroups(false)");
hPrintf(" ");
hButtonWithOnClick("hgt.expandGroups", "Expand all", "Expand all track groups",
                   "return vis.expandAllGroups(true)");
hPrintf("<div style='margin-top:.2em; margin-bottom:.9em;'>Control track and group visibility "
        "more selectively below.</div>");
trackConfig(trackList, groupList, groupTarget, vis);

dyStringFree(&title);
freez(&groupTarget);
webEndSectionTables();
hPrintf("</FORM>");
}

void configPage()
/* Put up configuration page. */
{
configPageSetTrackVis(-2);
}

struct mrGeneTrack
/* A track the new multi-region dialog offers as the source of exons or genes. */
    {
    struct mrGeneTrack *next;
    char *value;        /* table name of a genePred track, track name of a bigGenePred one */
    char *label;        /* what the menu shows */
    boolean isBig;      /* bigGenePred, read by maneExons/maneGenes;  else exonMostly/geneMostly */
    char *composite;    /* name of the composite it is in, or NULL */
    int rank;           /* sort order:  recommended tracks and their composite first */
    };

#define MR_NOT_RECOMMENDED 10000    /* rank of a track that is not among the recommended */

static int mrGeneTrackCmp(const void *va, const void *vb)
/* Recommended tracks first in their order, then the rest by label. */
{
const struct mrGeneTrack *a = *((struct mrGeneTrack **)va);
const struct mrGeneTrack *b = *((struct mrGeneTrack **)vb);
if (a->rank != b->rank)
    return a->rank - b->rank;
return strcasecmp(a->label, b->label);
}

static char *mrRecommended[] = {"mane", "knownGene", "catLiftOffGenesV1", "ncbiRefSeqCurated",
                                "ncbiRefSeq", "refGene", "ensGene"};

static void mrAddGeneTracks(struct trackDb *tdbList, struct hash *seen,
                            struct mrGeneTrack **pList)
/* Add the native genePred and bigGenePred tracks of tdbList and their subtracks. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (tdb->subtracks != NULL)
        {
        mrAddGeneTracks(tdb->subtracks, seen, pList);
        continue;
        }
    if (tdb->type == NULL || isCustomTrack(tdb->track) || hashLookup(seen, tdb->track))
        continue;
    /* a bigGenePred track can also come from a hub, on any assembly (hs1:  CAT/Liftoff,
     * NCBI RefSeq);  a genePred table only from the assembly's own database */
    boolean isBig = startsWith("bigGenePred", tdb->type);
    /* exonMostly reads the table by the track's name, so they must be the same */
    boolean isSql = startsWith("genePred", tdb->type) && sameOk(tdb->track, tdb->table) &&
                    !isHubTrack(tdb->track) && !trackHubDatabase(database);
    if (!isBig && !isSql)
        continue;
    hashAdd(seen, tdb->track, NULL);
    struct mrGeneTrack *gt;
    AllocVar(gt);
    gt->value = isBig ? tdb->track : tdb->table;
    gt->isBig = isBig;
    struct trackDb *parent = tdbGetComposite(tdb);   // above any view
    if (parent != NULL)
        {
        char buf[1024];
        safef(buf, sizeof buf, "%s - %s", parent->shortLabel, tdb->shortLabel);
        gt->label = cloneString(buf);
        }
    else
        gt->label = tdb->shortLabel;
    gt->composite = parent ? parent->track : NULL;
    gt->rank = MR_NOT_RECOMMENDED;
    int i;
    for (i = 0; i < ArraySize(mrRecommended); i++)
        if (sameString(trackHubSkipHubName(gt->value), mrRecommended[i]))  // also hs1's tracks
            gt->rank = 10 * i;
    slAddHead(pList, gt);
    }
}

static void mrRankCompositeSiblings(struct mrGeneTrack *list)
/* Put the other tracks of a composite that has a recommended track (e.g. RefSeq HGMD
 * beside RefSeq Curated) right after the last recommended one of that composite. */
{
struct hash *lastRank = hashNew(0);
struct mrGeneTrack *gt;
for (gt = list; gt != NULL; gt = gt->next)
    if (gt->composite != NULL && gt->rank < MR_NOT_RECOMMENDED)
        {
        int old = hashIntValDefault(lastRank, gt->composite, -1);
        if (gt->rank > old)
            hashReplace(lastRank, gt->composite, intToPt(gt->rank));
        }
for (gt = list; gt != NULL; gt = gt->next)
    if (gt->composite != NULL && gt->rank == MR_NOT_RECOMMENDED)
        {
        int last = hashIntValDefault(lastRank, gt->composite, -1);
        if (last >= 0)
            gt->rank = last + 5;
        }
hashFree(&lastRank);
}

static boolean mrIsGeneType(struct trackDb *tdb)
/* Gene tracks are offered in the gene track menu, not in the track search. */
{
return startsWith("genePred", tdb->type) || startsWith("bigGenePred", tdb->type);
}

static char *mrTrackLabel(struct trackDb *tdb)
/* The label the track search shows:  the composite's label in front of a subtrack's. */
{
struct trackDb *parent = tdbGetComposite(tdb);
if (parent == NULL)
    return cloneString(tdb->shortLabel);
char buf[1024];
safef(buf, sizeof buf, "%s - %s", parent->shortLabel, tdb->shortLabel);
return cloneString(buf);
}

static boolean mrMatchesAll(struct slName *words, char *text)
/* Is every word in text, ignoring case? */
{
struct slName *w;
for (w = words; w != NULL; w = w->next)
    if (!containsStringNoCase(text, w->name))
        return FALSE;
return TRUE;
}

struct mrFound
/* A track the track search found. */
    {
    struct mrFound *next;
    struct trackDb *tdb;
    char *label;
    int score;          /* 0 if the label starts with the first word, else 1 */
    };

static int mrFoundCmp(const void *va, const void *vb)
/* Better matches first, then by label. */
{
const struct mrFound *a = *((struct mrFound **)va);
const struct mrFound *b = *((struct mrFound **)vb);
if (a->score != b->score)
    return a->score - b->score;
return strcasecmp(a->label, b->label);
}

static void mrSearchTracks(struct trackDb *tdbList, struct slName *words, struct hash *seen,
                           struct mrFound **pList)
/* Add the tracks under tdbList that trackItems can use and that match all words. */
{
struct trackDb *tdb;
for (tdb = tdbList; tdb != NULL; tdb = tdb->next)
    {
    if (tdb->subtracks != NULL)
        {
        mrSearchTracks(tdb->subtracks, words, seen, pList);
        continue;
        }
    if (hashLookup(seen, tdb->track) || mrIsGeneType(tdb) || !multiRegionTrackUsable(tdb))
        continue;
    hashAdd(seen, tdb->track, NULL);
    char *label = mrTrackLabel(tdb);
    char text[4096];
    safef(text, sizeof text, "%s %s %s", label, tdb->longLabel, tdb->track);
    if (!mrMatchesAll(words, text))
        {
        freeMem(label);
        continue;
        }
    struct mrFound *f;
    AllocVar(f);
    f->tdb = tdb;
    f->label = label;
    f->score = (words == NULL || startsWithNoCase(words->name, label)) ? 0 : 1;
    slAddHead(pList, f);
    }
}

static void mrWriteTranscript(struct jsonWrite *jw, char *name, char *gene, char *chrom,
                              unsigned start, unsigned end, char *strand)
/* One transcript of the transcript menu:  its name, chrom, and the text the menu shows. */
{
char text[1024];
char startBuf[32], endBuf[32];
if (sameOk(gene, name))     // some files repeat the accession as the gene name
    gene = "";
sprintLongWithCommas(startBuf, start + 1);
sprintLongWithCommas(endBuf, end);
safef(text, sizeof text, "%s%s%s%s  %s:%s-%s %s", name, isNotEmpty(gene) ? " (" : "",
      isNotEmpty(gene) ? gene : "", isNotEmpty(gene) ? ")" : "", chrom, startBuf, endBuf,
      strand);
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "value", name);
jsonWriteString(jw, "chrom", chrom);
jsonWriteString(jw, "text", text);
jsonWriteObjectEnd(jw);
}

void multiRegionTrackSearch()
/* Answer the multi-region dialog's track search, hgt.mrTrackSearch=<words>, with JSON:
 * the tracks whose items trackItems can lay out (no gene tracks, those have their own
 * menu) and whose labels or name contain all words, at most 200, and how many matched.
 * The cart is read but never saved.  refs #38830 */
{
struct errCatch *errCatch = errCatchNew();
struct jsonWrite *jw = jsonWriteNew();
if (errCatchStart(errCatch))
    {
    cart = cartForSession(hUserCookie(), NULL, NULL);
    database = cartUsualString(cart, "db", hDefaultDb());
    struct trackDb *tdbList = NULL;
    struct grp *grpList = NULL;
    cartTrackDbInitForApi(cart, database, &tdbList, &grpList, FALSE);
    char *query = cloneString(cgiUsualString("hgt.mrTrackSearch", ""));
    struct slName *words = slNameListFromString(query, ' ');
    struct hash *seen = hashNew(0);
    struct mrFound *found = NULL, *f;
    mrSearchTracks(tdbList, words, seen, &found);
    slSort(&found, mrFoundCmp);
    int total = slCount(found), n = 0;
    jsonWriteObjectStart(jw, NULL);
    jsonWriteNumber(jw, "total", total);
    jsonWriteListStart(jw, "items");
    for (f = found; f != NULL && n < 200; f = f->next, n++)
        {
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "value", f->tdb->track);
        jsonWriteString(jw, "text", f->label);
        jsonWriteString(jw, "descr", f->tdb->longLabel);
        jsonWriteObjectEnd(jw);
        }
    jsonWriteListEnd(jw);
    jsonWriteObjectEnd(jw);
    }
errCatchEnd(errCatch);
cgiPrintContentType("application/json");
if (errCatch->gotError)
    {
    jsonWriteFree(&jw);
    jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "error", errCatch->message->string);
    jsonWriteObjectEnd(jw);
    }
puts(jw->dy->string);
jsonWriteFree(&jw);
errCatchFree(&errCatch);
}

void multiRegionTranscriptList()
/* Answer the multi-region dialog's transcript menu, hgt.mrTranscripts=<gene track> with
 * hgt.mrPos=<chrom:start-end>, with JSON:  the transcripts of the track that overlap the
 * position, at most 500, in genomic order.  Native genePred and bigGenePred tracks only,
 * the ones the gene track menu offers.  The cart is read but never saved.  refs #38830 */
{
struct errCatch *errCatch = errCatchNew();
struct jsonWrite *jw = jsonWriteNew();
if (errCatchStart(errCatch))
    {
    cart = cartForSession(hUserCookie(), NULL, NULL);
    database = cartUsualString(cart, "db", hDefaultDb());
    char *track = cgiString("hgt.mrTranscripts");
    char *pos = cloneString(cgiUsualString("hgt.mrPos", ""));
    stripChar(pos, ',');
    char *chrom = NULL;
    int start = 0, end = 0;
    if (!hgParseChromRange(database, pos, &chrom, &start, &end))
        errAbort("Cannot list transcripts for position %s", pos);
    struct sqlConnection *conn = trackHubDatabase(database) ? NULL : hAllocConn(database);
    struct trackDb *tdb = NULL;
    if (isHubTrack(track))
        tdb = hubConnectAddHubForTrackAndFindTdb(database, track, NULL, hashNew(0));
    else if (conn != NULL)
        tdb = hMaybeTrackInfo(conn, track);
    if (tdb == NULL)
        errAbort("Unknown gene track %s", track);
    int maxItems = 500, count = 0;
    jsonWriteObjectStart(jw, NULL);
    jsonWriteListStart(jw, "items");
    if (startsWith("bigGenePred", tdb->type))
        {
        char *fileName = hReplaceGbdb(bbiNameFromSettingOrTable(tdb, conn, tdb->table));
        struct bbiFile *bbi = bigBedFileOpenAlias(fileName, chromAliasFindAliases);
        int geneIx = bigGenePredSymbolIx(bbi);
        struct lm *lm = lmInit(0);
        struct bigBedInterval *bb, *bbList = bigBedIntervalQuery(bbi, chrom, start, end,
                                                                 maxItems, lm);
        for (bb = bbList; bb != NULL; bb = bb->next, count++)
            {
            char startBuf[16], endBuf[16];
            char *row[bbi->fieldCount];
            bigBedIntervalToRow(bb, chrom, startBuf, endBuf, row, bbi->fieldCount);
            char *gene = (geneIx >= 0 && geneIx < bbi->fieldCount) ? row[geneIx] : "";
            mrWriteTranscript(jw, row[3], gene, chrom, bb->start, bb->end, row[5]);
            }
        lmCleanup(&lm);
        bigBedFileClose(&bbi);
        }
    else if (startsWithWord("genePred", tdb->type) && conn != NULL)
        {
        boolean hasName2 = (sqlFieldIndex(conn, tdb->table, "name2") >= 0);
        char query[1024];
        sqlSafef(query, sizeof query, "select name, %s, strand, txStart, txEnd from %s "
                 "where chrom='%s' and txStart < %d and txEnd > %d order by txStart limit %d",
                 hasName2 ? "name2" : "name", tdb->table, chrom, end, start, maxItems);
        struct sqlResult *sr = sqlGetResult(conn, query);
        char **row;
        while ((row = sqlNextRow(sr)) != NULL)
            {
            mrWriteTranscript(jw, row[0], hasName2 ? row[1] : "", chrom, sqlUnsigned(row[3]),
                              sqlUnsigned(row[4]), row[2]);
            count++;
            }
        sqlFreeResult(&sr);
        }
    else
        errAbort("%s is not a gene track", tdb->shortLabel);
    hFreeConn(&conn);
    jsonWriteListEnd(jw);
    jsonWriteBoolean(jw, "more", count >= maxItems);
    jsonWriteObjectEnd(jw);
    }
errCatchEnd(errCatch);
cgiPrintContentType("application/json");
if (errCatch->gotError)
    {
    jsonWriteFree(&jw);
    jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "error", errCatch->message->string);
    jsonWriteObjectEnd(jw);
    }
puts(jw->dy->string);
jsonWriteFree(&jw);
errCatchFree(&errCatch);
}

void multiRegionGeneTrackList()
/* Answer the multi-region dialog's gene track menu, hgt.mrGeneTracks=1, with JSON:  the
 * native genePred and bigGenePred tracks, recommended ones and their composites first,
 * and the quick picks this assembly has.  The cart is read but never saved.  refs #38830 */
{
struct errCatch *errCatch = errCatchNew();
struct jsonWrite *jw = jsonWriteNew();
if (errCatchStart(errCatch))
    {
    cart = cartForSession(hUserCookie(), NULL, NULL);
    database = cartUsualString(cart, "db", hDefaultDb());
    struct mrGeneTrack *geneTracks = NULL, *gt;
    struct trackDb *tdbList = NULL;
    struct grp *grpList = NULL;
    cartTrackDbInitForApi(cart, database, &tdbList, &grpList, FALSE);
    mrAddGeneTracks(tdbList, hashNew(0), &geneTracks);
    mrRankCompositeSiblings(geneTracks);
    slSort(&geneTracks, mrGeneTrackCmp);
    jsonWriteObjectStart(jw, NULL);
    jsonWriteListStart(jw, "items");
    for (gt = geneTracks; gt != NULL; gt = gt->next)
        {
        /* B| a bigGenePred track, T| a genePred table:  Selectize keeps only the value */
        char value[1024];
        safef(value, sizeof value, "%s%s", gt->isBig ? "B|" : "T|", gt->value);
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "value", value);
        jsonWriteString(jw, "text", gt->label);
        jsonWriteString(jw, "group", gt->rank < MR_NOT_RECOMMENDED ? "Recommended"
                                                                   : "All gene tracks");
        jsonWriteObjectEnd(jw);
        }
    jsonWriteListEnd(jw);
    /* Quick picks for the gene tracks most people want, those this assembly has */
    static char *quickTracks[][2] = {{"mane", "MANE"}, {"knownGene", "GENCODE"},
        {"catLiftOffGenesV1", "CAT/Liftoff"},
        {"ncbiRefSeqCurated", "RefSeq Curated"}, {"ncbiRefSeqSelect", "RefSeq Select"},
        {"ncbiRefSeqHgmd", "RefSeq HGMD"}, {"refGene", "UCSC RefSeq"},
        {"ensGene", "Ensembl"}};
    jsonWriteListStart(jw, "quick");
    int q;
    for (q = 0; q < ArraySize(quickTracks); q++)
        {
        for (gt = geneTracks; gt != NULL; gt = gt->next)
            if (sameString(trackHubSkipHubName(gt->value), quickTracks[q][0]))
                break;
        if (gt == NULL)
            continue;
        char value[1024];
        safef(value, sizeof value, "%s%s", gt->isBig ? "B|" : "T|", gt->value);
        jsonWriteObjectStart(jw, NULL);
        jsonWriteString(jw, "value", value);
        jsonWriteString(jw, "label", quickTracks[q][1]);
        jsonWriteString(jw, "title", gt->label);
        jsonWriteObjectEnd(jw);
        }
    jsonWriteListEnd(jw);
    jsonWriteObjectEnd(jw);
    }
errCatchEnd(errCatch);
cgiPrintContentType("application/json");
if (errCatch->gotError)
    {
    jsonWriteFree(&jw);
    jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "error", errCatch->message->string);
    jsonWriteObjectEnd(jw);
    }
puts(jw->dy->string);
jsonWriteFree(&jw);
errCatchFree(&errCatch);
}

// TODO GALT there is duplication still between config and configMultiRegionPageSetTrackVis
//  that could maybe be addressed by pulling the code that initializes the tracklist,
//  and the code that draws the multi-region options, into 2 functions to be called by each.

void configMultiRegionPage()
/* Do multi-region config page after setting track visibility. If vis is -2, then visibility
 * is unchanged.  If -1 then set visibility to default, otherwise it should
 * be tvHide, tvDense, etc. */
{
char *groupTarget;
struct track *trackList;
struct track *ideoTrack;
struct group *groupList;
int vis = -2;

configInitTrackList(vis, &groupTarget, &trackList, &ideoTrack, &groupList);

hPrintf("<FORM ACTION=\"%s\" NAME=\"mainForm\" METHOD=%s>\n", hgTracksName(),
	cartUsualString(cart, "formMethod", "POST"));

webStartWrapperDetailedNoArgs(cart, database, "", "", FALSE, FALSE, FALSE, FALSE);

cartSaveSession(cart);

hPrintf("<a href=\"../goldenPath/help/multiRegionHelp.html\" target='_blank' class='blueLink'>"
                "<b>Multi-region display</b></a>"
            " 'slices' the genome to allow viewing discontinuous regions"
            " together in the browser window. &nbsp;&nbsp;");
// mode-specific message filled in by JS when dialog opened
hPrintf("<span id='multiRegionConfigStatusMsg'></span>");
hPrintf("<p></p>");

hTableStart();

virtModeType = cartUsualString(cart, "virtModeType", virtModeType);

hPrintf("<TR><TD>");
cgiMakeRadioButton("virtModeType", "default", sameWord("default", virtModeType));
hPrintf("</TD>");
hPrintf("<TD id='virtModeTypeDefaultLabel'>");
hPrintf("Exit multi-region mode &nbsp; (keyboard shortcut: d then v)");
hPrintf("</TD></TR>\n");

struct sqlConnection *conn = NULL;
if (!trackHubDatabase(database))  // no db conn for assembly hubs 
    conn = hAllocConn(database);

// Does this assembly have MANE?
if (maneExonsTdb() != NULL)
    {
    hPrintf("<TR><TD>");
    cgiMakeRadioButton("virtModeType", "maneExons", sameWord("maneExons", virtModeType));
    hPrintf("</TD><TD>");
    hPrintf("Show exons of MANE transcripts. &nbsp;&nbsp; Use padding of: ");
    hIntVar("manePadding", cartUsualInt(cart, "manePadding", manePadding), 3);
    hPrintf(" bases.");
    hPrintf("</TD></TR>\n");
    }

// Do we have a gene table for exonMostly?
findBestEMGeneTable(trackList);
if (emGeneTable)
    {
    hPrintf("<TR><TD>");
    cgiMakeRadioButton("virtModeType", "exonMostly", sameWord("exonMostly", virtModeType));
    hPrintf("</TD><TD>");
    hPrintf("Show exons using %s &nbsp; (keyboard shortcut: e then v). &nbsp;&nbsp; Use padding of: ", emGeneTrack->shortLabel);
    hIntVar("emPadding", cartUsualInt(cart, "emPadding", emPadding), 3);
    hPrintf(" bases.");
    hPrintf("</TD></TR>\n");
    }

if (emGeneTable)
    {
    hPrintf("<TR><TD>");
    cgiMakeRadioButton("virtModeType", "geneMostly", sameWord("geneMostly", virtModeType));
    hPrintf("</TD><TD>");
    hPrintf("Show genes using %s. &nbsp;&nbsp; Use padding of: ", emGeneTrack->shortLabel);
    hIntVar("gmPadding", cartUsualInt(cart, "gmPadding", gmPadding), 3);
    hPrintf(" bases.");
    hPrintf("</TD></TR>\n");
    }

/* obsolete    
if (conn && sqlTableExists(conn,"knownCanonical"))
    {
    hPrintf("<TR><TD>");
    cgiMakeRadioButton("virtModeType", "kcGenes", sameWord("kcGenes", virtModeType));
    hPrintf("</TD><TD>");
    hPrintf("Show gene regions genome-wide.");
    hPrintf("</TD></TR>\n");
    }
*/

hPrintf("<TR><TD>");
cgiMakeRadioButton("virtModeType", "customUrl", sameWord("customUrl", virtModeType));
hPrintf("</TD><TD>");
hPrintf("Enter custom regions as BED, or a URL to them:<br>");
multiRegionsBedUrl = cartUsualString(cart, "multiRegionsBedUrl", multiRegionsBedUrl);
struct dyString *dyMultiRegionsBedInput = dyStringNew(256);
if (strstr(multiRegionsBedUrl,"://"))
    {
    dyStringAppend(dyMultiRegionsBedInput, multiRegionsBedUrl);
    }
else
    {
    /* Not a URL, so this is the trash file we wrote the pasted BED to.  Check it before
     * reading, since the contents go straight back to the user in the text area below. */
    if (isServerUserFilePath(multiRegionsBedUrl) && fileExists(multiRegionsBedUrl))
	{
	struct lineFile *lf = lineFileMayOpen(multiRegionsBedUrl, TRUE);
	char *line;
	int lineSize;
	while (lineFileNext(lf, &line, &lineSize))
	    {
	    dyStringPrintf(dyMultiRegionsBedInput, "%s\n", line);
	    }
	lineFileClose(&lf);
	}
    }
hPrintf("<TEXTAREA NAME='multiRegionsBedInput' ID='multiRegionsBedInput' rows='4' cols='58' style='white-space: pre;'>%s</TEXTAREA>",
    dyMultiRegionsBedInput->string);

// option to set viewing window to show all regions.  This id also known to JS.
if (cfgOptionBooleanDefault(MULTI_REGION_CFG_BUTTON_TOP, TRUE))
    {
    boolean isChecked = cartUsualBoolean(cart, MULTI_REGION_BED_WIN_FULL, FALSE);
    hPrintf("&nbsp;&nbsp");
    cgiMakeCheckBoxUtil(MULTI_REGION_BED_WIN_FULL, isChecked, 
                            "If unchecked, when regions are changed here the view is zoomed in and does not display all regions", 
                            MULTI_REGION_BED_WIN_FULL);
    hPrintf("Show all");
    hPrintf("</TD></TR>\n");
}

/* The AllChroms option will be released in future
if (emGeneTable && sqlTableExists(conn, emGeneTable))
    {
    hPrintf("<TR><TD>");
    cgiMakeRadioButton("virtModeType", "singleTrans", sameWord("singleTrans", virtModeType));
    hPrintf("</TD><TD>");
    hPrintf("Show only one transcript using an ID from %s : ", emGeneTrack->shortLabel);
    char *trans = cartUsualString(cart, "singleTransId", singleTransId);
    char sql[1024];
    sqlSafef(sql, sizeof sql, "select name from %s where name='%s'", emGeneTable, trans);
    char *result = sqlQuickString(conn, sql);
    if (!result)
	{
	sqlSafef(sql, sizeof sql, "select name from %s limit 1", emGeneTable);
	trans = sqlQuickString(conn, sql);
	}
    hTextVar("singleTransId", trans, 20);
    hPrintf("</TD></TR>\n");
    }
*/

if (conn)
    {
    boolean altLocExists = sqlTableExists(conn, "altLocations");
    boolean fixLocExists = sqlTableExists(conn, "fixLocations");
    if (altLocExists || fixLocExists)
        {
        hPrintf("<TR><TD>");
        cgiMakeRadioButton("virtModeType", "singleAltHaplo",
                           sameWord("singleAltHaplo", virtModeType));
        hPrintf("</TD><TD>");
        hPrintf("Show one alternate haplotype");
        if (fixLocExists)
            hPrintf(" or fix patch");
        hPrintf(", placed on its chromosome, using ID: ");
        char *haplo = cartUsualString(cart, "singleAltHaploId", singleAltHaploId);
        char *foundHaplo = NULL;
        char sql[1024];
        if (altLocExists)
            {
            sqlSafef(sql, sizeof sql,
                     "select name from altLocations where name rlike '^%s(:[0-9-]+)?'", haplo);
            foundHaplo = sqlQuickString(conn, sql);
            }
        if (!foundHaplo && fixLocExists)
            {
            sqlSafef(sql, sizeof sql,
                     "select name from fixLocations where name rlike '^%s(:[0-9-]+)?'", haplo);
            foundHaplo = sqlQuickString(conn, sql);
            }
        if (!foundHaplo)
            {
            if (altLocExists)
                sqlSafef(sql, sizeof sql, "select name from altLocations limit 1");
            else
                sqlSafef(sql, sizeof sql, "select name from fixLocations limit 1");
            haplo = sqlQuickString(conn, sql);
            chopSuffixAt(haplo, ':');
            }
        hTextVar("singleAltHaploId", haplo, 60);
        hPrintf("</TD></TR>\n");
        }
    }

/* disable demo for now
if (sameString(database,"hg19") || sameString(database, "hg38"))
    {
    hPrintf("<TR><TD>");
    cgiMakeRadioButton("virtModeType", "demo1", sameWord("demo1", virtModeType));
    hPrintf("</TD><TD>");
    hPrintf("demo1 two windows on two chroms (default pos on chr21, and same loc on chr22)");
    hPrintf("</TD></TR>\n");
    }
*/


/* Disabled for now 
hPrintf("<TR><TD>");
cgiMakeRadioButton("virtModeType", "demo2", sameWord("demo2", virtModeType));
hPrintf("</TD><TD>");
hPrintf("demo2 multiple "); 
hIntVar("demo2NumWindows", cartUsualInt(cart, "demo2NumWindows", demo2NumWindows), 3);
hPrintf(" windows on one chrom chr21 def posn, window size ");
hIntVar("demo2WindowSize", cartUsualInt(cart, "demo2WindowSize", demo2WindowSize), 3);
hPrintf(" and step size ");
hIntVar("demo2StepSize", cartUsualInt(cart, "demo2StepSize", demo2StepSize), 3);
hPrintf(" exon-like");
hPrintf("</TD></TR>\n");
*/

/* The AllChroms option will be released in future
if (conn)  // requires chromInfo from database. 
    { // TODO allow it to use assembly hubs via trackHubAllChromInfo() ?
    hPrintf("<TR><TD>");
    cgiMakeRadioButton("virtModeType", "allChroms", sameWord("allChroms", virtModeType));
    hPrintf("</TD><TD>");
    hPrintf("<br>Show all chromosomes.<br><span style='color:red'>Warning:</span> Turn off all tracks except bigBed, bigWig, and very sparse tracks.<br>Press Hide All to hide all tracks.");
    hPrintf("</TD></TR>\n");
    }
*/


/* Disabled for now 
hPrintf("<TR><TD>");
cgiMakeRadioButton("virtModeType", "demo4", sameWord("demo4", virtModeType));
hPrintf("</TD><TD>");
hPrintf("demo4 multiple (311) windows showing exons from TITIN gene uc031rqd.1.");
hPrintf("</TD></TR>\n");
*/

/* Disabled for now 
hPrintf("<TR><TD>");
cgiMakeRadioButton("virtModeType", "demo5", sameWord("demo5", virtModeType));
hPrintf("</TD><TD>");
hPrintf("demo5 alt locus on hg38. Shows alt chrom surrounded by regions of same size from reference genome.");
hPrintf("</TD></TR>\n");
*/

/* Disabled for now 
hPrintf("<TR><TD>");
cgiMakeRadioButton("virtModeType", "demo6", sameWord("demo6", virtModeType));
hPrintf("</TD><TD>");
hPrintf("demo6 shows zoomed in exon-exon junction from SOD1 gene, between exon1 and exon2.");
hPrintf("</TD></TR>\n");
*/


hTableEnd();

hPrintf("<BR>\n");
hPrintf("<TABLE style=\"border:0px; \">\n");
hPrintf("<TR><TD>");
hCheckBox("emAltHighlight", cartUsualBoolean(cart, "emAltHighlight", FALSE));
hPrintf("</TD><TD>");
hPrintf("Highlight alternating regions in multi-region view");
hPrintf("</TD></TR>\n");
hPrintf("</TABLE>\n");

hPrintf("<BR>\n");
hPrintf("<TABLE style=\"border:0px;width:650px \">\n");
hPrintf("<TR><TD>");
cgiMakeButton("topSubmit", "Submit");
hPrintf("&nbsp;&nbsp");
cgiMakeCancelButton("Cancel");
hPrintf("</TD></TR>\n");
hPrintf("</TABLE>\n");

hFreeConn(&conn);

cgiDown(0.9);

freez(&groupTarget);
webEndSectionTables();
hPrintf("</FORM>");

// The previous version of the hgTracks js object will get over-written by this page,
// so add all the variables needed htere. So any new hgTracks.somefield references
// added to hgTracks.js may need to be put here.

if (differentString(virtModeType, "default"))
    jsonObjectAdd(jsonForClient, "virtModeType", newJsonString(virtModeType));


}

