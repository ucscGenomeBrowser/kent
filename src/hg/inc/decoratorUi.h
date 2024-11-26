/* decorator controls */

/* Copyright (C) 2023 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef DECORATOR_UI_H
#define DECORATOR_UI_H

#include "decoration.h"

typedef enum {DECORATOR_MODE_HIDE,DECORATOR_MODE_OVERLAY,DECORATOR_MODE_ADJACENT} decoratorMode;

struct trackDb *getTdbsForDecorators(struct trackDb *tdb);
/* Gin up a list of fake tdb structure to pretend decorator settings are for
 * their own tracks.  This makes processing filter options much easier. */

void decoratorUi(struct trackDb *tdb, struct cart *cart, struct slName *decoratorSettings);
/* decoratorSettings is a list of the decorator-relevant trackDb settings for this track.
 * Right now it's populated by including parent settings; that might be a bad idea.
 * We'll see.
 */

decoratorMode decoratorDrawMode(struct trackDb *tdb, struct cart *cart, decorationStyle style);
/* Return the chosen (or default) draw mode (hide/overlay/etc.) for the chose decoration
 * style (glyph/block) from the given decoration source (tdb).
 */

boolean decoratorUiShowOverlayLabels(struct trackDb *tdb, struct cart *cart);
/* Return true if the checkbox for displaying labels for block decorations in overlay
 * mode is checked. Must be provided with the mock tdb for the decorator itself (as
 * generated getTdbForDecorator()), not the tdb for the parent track. */


long decoratorUiMaxLabeledBaseCount(struct trackDb *tdb, struct cart *cart);
/* Return the entered (or default) number of bases.  Decoration labels will only
 * be drawn if the size of the global window (spanning all region windows)
 * displays at most that many bases.  This helps reduce confusion in zoomed-out
 * situations. */

#endif // DECORATOR_UI_H
