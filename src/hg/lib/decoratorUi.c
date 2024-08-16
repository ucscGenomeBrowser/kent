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


struct trackDb *getTdbForDecorator(struct trackDb *tdb, char *prefix)
/* Gin up a fake tdb structure to pretend decorator settings are for
 * their own track.  This makes processing filter options much
 * easier. */
{
struct trackDb *new = NULL;
AllocVar(new);
struct dyString *raString = dyStringNew(0);
char decPrefix[2048];
safef(decPrefix, sizeof(decPrefix), "decorator.%s.*", prefix);
struct slName *decoratorSettings = trackDbSettingsWildMatch(tdb, decPrefix);
struct slName *thisSet = decoratorSettings;

safef(decPrefix, sizeof(decPrefix), "decorator.%s.", prefix);
while (thisSet != NULL)
    {
    if (startsWith(decPrefix, thisSet->name))
        {
        char *newName = thisSet->name + strlen(decPrefix);
        dyStringPrintf(raString, "\n%s %s", newName, trackDbSetting(tdb, thisSet->name));
        }
    thisSet = thisSet->next;
    }
new->settings = dyStringCannibalize(&raString);
new->type = cloneString("bigBed");
new->parent = NULL;
new->settingsHash = NULL;
new->isNewFilterHash = NULL;
struct dyString *trackName = dyStringCreate("%s.decorator.%s", tdb->track, prefix);
new->track = dyStringCannibalize(&trackName);
trackDbHashSettings(new);

// Should I be calling trackDbPolish() here?

return new;
}

struct trackDb *getTdbsForDecorators(struct trackDb *tdb)
/* Generate a list of faked-up trackDb structures for each of the decorators listed
 * in the provided track.
 */
{
struct trackDb *decTdbs = NULL;
struct slName *decoratorSettings = trackDbSettingsWildMatch(tdb, "decorator.*");
struct slName *decoratorNames = NULL;
for (struct slName *thisSetting = decoratorSettings; thisSetting != NULL; thisSetting = thisSetting->next)
    {
    char *decoratorName = cloneString(strchr(thisSetting->name, '.')+1);
    char *nameEnd = strchr(decoratorName, '.');
    if (nameEnd != NULL)
        {
        nameEnd[0] = '\0';
        // slNameStore isn't especially efficient, but we don't anticipate a huge number of
        // decorators for any one track.  If that's wrong, swap this to a hash and pull the
        // keys afterward.
        slNameStore(&decoratorNames, decoratorName);
        }
    else
        {
        warn("Invalid decorator setting %s in track %s", thisSetting->name, tdb->track);
        }
    }

//slReverse(&decoratorNames);
// decoratorNames is now a list of the various decorator names, so we just iterate and make a tdb for each
for (struct slName *thisName = decoratorNames; thisName != NULL; thisName = thisName->next)
    {
    struct trackDb *new = getTdbForDecorator(tdb, thisName->name);
    slAddHead(&decTdbs, new);
    }
// Right now the double reverse cancels out and the decorator tdbs are provided in the order
// they came out of the settings.  This could change if we replace slNameStore.
//slReverse(&decTdbs);
return decTdbs;
}
void decoratorUi(struct trackDb *tdb, struct cart *cart, struct slName *decoratorSettings)
/* decoratorSettings is expected to be a list of the decorator-relevant trackDb settings
 * for this track (i.e. anything starting with "decorator."), which might compass settings
 * for multiple decorators.
 * Right now it's populated by including parent settings; that might be a bad idea.
 * We'll see.
 */
{
char cartVar[2048];
puts("<p>");
printf("\n");
printf("<h3>Decoration settings</h3>\n<p>");
printf("<p>Decorators are optional sub-annotations shown in addition to the rectangular annotations of this track. "
        "They can either have the shape of rectangular blocks transparently overlaid or shown beneath features. "
        "Or they can be glyphs, symbols such as triangles, stars, etc. drawn on top of onto existing annotations. "
        "See the <a target=_blank href='../goldenPath/help/decorator.html'>decorator documentation page</a>.</p>");

//
// Consider adding trackDb option to suppress configuration of specific data types.  Would
// have to be paired with skipping the load of those data types (for pathological cases),
// but if your decorator has no glyphs, why should we display a config box for them?

struct trackDb *mockTdbsForDecorators = getTdbsForDecorators(tdb);

char *menuOpt[] = {HIDE_MODE_STR, OVERLAY_MODE_STR, ADJACENT_MODE_STR};

for (struct trackDb *mockTdbForDecorator = mockTdbsForDecorators;
        mockTdbForDecorator != NULL;
        mockTdbForDecorator = mockTdbForDecorator->next)
    {
    printf("<b>Block</b> decoration placement:&nbsp");
    decoratorMode currentBlockMode = decoratorDrawMode(mockTdbForDecorator, cart, DECORATION_STYLE_BLOCK);
    char *modeStr = HIDE_MODE_STR;
    if (currentBlockMode == DECORATOR_MODE_OVERLAY)
        modeStr = OVERLAY_MODE_STR;
    else if (currentBlockMode == DECORATOR_MODE_ADJACENT)
        modeStr = ADJACENT_MODE_STR;
    cgiMakeDropList(cartVar, menuOpt, 3, modeStr);

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

    decoratorMode currentGlyphMode = decoratorDrawMode(mockTdbForDecorator, cart, DECORATION_STYLE_GLYPH);
    modeStr = HIDE_MODE_STR;
    if (currentGlyphMode == DECORATOR_MODE_OVERLAY)
        modeStr = OVERLAY_MODE_STR;
    else if (currentGlyphMode == DECORATOR_MODE_ADJACENT)
        modeStr = ADJACENT_MODE_STR;
    cgiMakeDropList(cartVar, menuOpt, 3, modeStr);

    printf("<br>\n");

    filterBy_t *filterBySet = filterBySetGet(mockTdbForDecorator, cart, mockTdbForDecorator->track);

    if (filterBySet != NULL)
        {
        if (!tdbIsComposite(tdb) && cartOptionalString(cart, "ajax") == NULL)
            jsIncludeFile("hui.js",NULL);

        filterBySetCfgUi(cart,mockTdbForDecorator,filterBySet,TRUE, mockTdbForDecorator->track);
        filterBySetFree(&filterBySet);
        }
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
