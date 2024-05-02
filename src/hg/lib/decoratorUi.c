/* decorator controls */

/* Copyright (C) 2019 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "cheapcgi.h"
#include "cart.h"
#include "hui.h"
#include "web.h"
#include "trackDb.h"
#include "jsHelper.h"
#include "decoration.h"
#include "decoratorUi.h"

#define HIDE_MODE_STR "Hide"
#define OVERLAY_MODE_STR "Overlay"
#define ADJACENT_MODE_STR "Adjacent"

#define OVERLAY_BLOCK_LABEL_TOGGLE_VAR OVERLAY_MODE_STR "toggle"
#define OVERLAY_BLOCK_LABEL_TOGGLE_DEFAULT FALSE
#define BLOCK_DISPLAY_VAR "blockMode"
#define BLOCK_DISPLAY_DEFAULT OVERLAY_MODE_STR
#define GLYPH_DISPLAY_VAR "glyphMode"
#define GLYPH_DISPLAY_DEFAULT OVERLAY_MODE_STR

#define DEFAULT_MAX_LABEL_BASECOUNT 200000
#define MAX_LABEL_BASECOUNT_VAR "maxLabelBases"

struct trackDb *getTdbForDecorator(struct trackDb *tdb)
/* Gin up a fake tdb structure to pretend decorator settings are for
 * their own track.  This makes processing filter options much
 * easier. */
{
struct trackDb *new = NULL;
AllocVar(new);
struct dyString *raString = dyStringNew(0);
struct slName *decoratorSettings = trackDbSettingsWildMatch(tdb, "decorator.default.*");
struct slName *thisSet = decoratorSettings;
while (thisSet != NULL)
    {
    if (startsWith("decorator.default.", thisSet->name))
        {
        char *newName = thisSet->name + strlen("decorator.default.");
        dyStringPrintf(raString, "\n%s %s", newName, trackDbSetting(tdb, thisSet->name));
        }
    thisSet = thisSet->next;
    }
new->settings = dyStringCannibalize(&raString);
new->type = cloneString("bigBed");
new->parent = NULL;
new->settingsHash = NULL;
new->isNewFilterHash = NULL;
struct dyString *trackName = dyStringCreate("%s.decorator.default", tdb->track);
new->track = dyStringCannibalize(&trackName);
trackDbHashSettings(new);

// Should I be calling trackDbPolish() here?

return new;
}


void decoratorUi(struct trackDb *tdb, struct cart *cart, struct slName *decoratorSettings)
/* decoratorSettings is a list of the decorator-relevant trackDb settings for this track.
 * Right now it's populated by including parent settings; that might be a bad idea.
 * We'll see.
 */
{
char cartVar[2048];
puts("<p>");
printf("\n");
printf("<h3>Decoration settings</h3>\n<p>");

// Consider adding trackDb option to suppress configuration of specific data types.  Would
// have to be paired with skipping the load of those data types (for pathological cases),
// but if your decorator has no glyphs, why should we display a config box for them?

struct trackDb *mockTdbForDecorator = getTdbForDecorator(tdb);

char *menuOpt[] = {HIDE_MODE_STR, OVERLAY_MODE_STR, ADJACENT_MODE_STR};

printf("<b>Block</b> decoration placement:&nbsp");
safef(cartVar, sizeof(cartVar), "%s.%s", mockTdbForDecorator->track, BLOCK_DISPLAY_VAR);
char *currentBlockMode = cartNonemptyString(cart, cartVar);
if (currentBlockMode == NULL)
    currentBlockMode = BLOCK_DISPLAY_DEFAULT; // can pass through trackDbSetting first later for creator config
cgiMakeDropList(cartVar, menuOpt, 3, currentBlockMode);

safef(cartVar, sizeof(cartVar), "%s.%s", mockTdbForDecorator->track, OVERLAY_BLOCK_LABEL_TOGGLE_VAR);
boolean checkBoxStatus = decoratorUiShowOverlayLabels(mockTdbForDecorator, cart);
printf("\ninclude labels in overlay mode:&nbsp;&nbsp;");
cgiMakeCheckBoxWithMsg(cartVar, checkBoxStatus, "Draw labels for blocks in overlay mode");
printf("<br>&nbsp;&nbsp;Auto-hide labels when window is larger than ");
long currentMaxBase = decoratorUiMaxLabeledBaseCount(mockTdbForDecorator, cart);
safef(cartVar, sizeof(cartVar), "%s.%s", mockTdbForDecorator->track, MAX_LABEL_BASECOUNT_VAR);
cgiMakeIntVar(cartVar, currentMaxBase, 9);
printf("bp\n");

printf("<br>\n");
printf("<b>Glyph</b> decoration placement:&nbsp");
safef(cartVar, sizeof(cartVar), "%s.%s", mockTdbForDecorator->track, GLYPH_DISPLAY_VAR);
char *currentGlyphMode = cartNonemptyString(cart, cartVar);
if (currentGlyphMode == NULL)
    currentGlyphMode = GLYPH_DISPLAY_DEFAULT; // can pass through trackDbSetting first later for creator config
cgiMakeDropList(cartVar, menuOpt, 3, currentGlyphMode);
printf("<br>\n");

char decoratorName[2048];
safef(decoratorName, sizeof(decoratorName), "%s.decorator.default", tdb->track);
filterBy_t *filterBySet = filterBySetGet(mockTdbForDecorator, cart, decoratorName);

if (filterBySet != NULL)
    {
    if (!tdbIsComposite(tdb) && cartOptionalString(cart, "ajax") == NULL)
        jsIncludeFile("hui.js",NULL);

    filterBySetCfgUi(cart,mockTdbForDecorator,filterBySet,TRUE, decoratorName);
    filterBySetFree(&filterBySet);
    }
}


decoratorMode decoratorDrawMode(struct trackDb *tdb, struct cart *cart, decorationStyle style)
/* Return the chosen (or default) draw mode (hide/overlay/etc.) for the chose decoration
 * style (glyph/block) from the given decoration source (tdb).
 */
{
char *drawMode = NULL;
if (style == DECORATION_STYLE_BLOCK)
    {
    drawMode = cartOrTdbString(cart, tdb, BLOCK_DISPLAY_VAR, BLOCK_DISPLAY_DEFAULT);
    }
else if (style == DECORATION_STYLE_GLYPH)
    {
    drawMode = cartOrTdbString(cart, tdb, GLYPH_DISPLAY_VAR, GLYPH_DISPLAY_DEFAULT);
    }
if (sameWordOk(drawMode, OVERLAY_MODE_STR))
    return DECORATOR_MODE_OVERLAY;
if (sameWordOk(drawMode, ADJACENT_MODE_STR))
    return DECORATOR_MODE_ADJACENT;
return DECORATOR_MODE_HIDE;
}

boolean decoratorUiShowOverlayLabels(struct trackDb *tdb, struct cart *cart)
/* Return true if the checkbox for displaying labels for block decorations in overlay
 * mode is checked. */
{
boolean checked = cartOrTdbBoolean(cart, tdb, OVERLAY_BLOCK_LABEL_TOGGLE_VAR, OVERLAY_BLOCK_LABEL_TOGGLE_DEFAULT);
return checked;
}

long decoratorUiMaxLabeledBaseCount(struct trackDb *tdb, struct cart *cart)
/* Return the entered (or default) number of bases.  Decoration labels will only
 * be drawn if the size of the global window (spanning all region windows)
 * displays at most that many bases.  This helps reduce confusion in zoomed-out
 * situations. */
{
long maxLabelBases = (long)cartOrTdbInt(cart, tdb, MAX_LABEL_BASECOUNT_VAR, DEFAULT_MAX_LABEL_BASECOUNT);
if (maxLabelBases < 0)
    maxLabelBases = 0;
return maxLabelBases; 
}
