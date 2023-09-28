#ifndef DECORATOR_H
#define DECORATOR_H

#include "common.h"
#include "decoration.h"
#include "hash.h"
#include "bigBed.h"
#include "hvGfx.h"
#include "hgTracks.h"
#include "bigBedFilter.h"
#include "mouseOver.h"

struct decoratedItemExtent {
// Keep track of how much decorations in a window increase the extent of decorated items
// (i.e. when decorations go past the start or end of the item).  These are pixel-based.
    int start;
    int end;
};

struct decoratorGroup
// A collection of decorators for a window.  Useful because there are some things that you want to
// know that are derived from the combination of decorators.  Like the ultimate extent of a
// decorated item within that window.
    {
    struct decoratorGroup *next;
    struct decorator *decorators;
    struct hash *itemExtents; // Keeps track of the full extent of an item + decorations, used for packing items
                              // because decorations might extend past the start or end of an item
    struct hash *itemOverlayExtents; // keep track of how far we have to pad out item labels b/c decorations
                                    // extend the drawn region on the line of the primary item

    void (*drawItemAt)(struct track *tg, void *item, struct hvGfx *hvg,
        int xOff, int yOff, double scale,
        MgFont *font, Color color, enum trackVisibility vis);
        // Points to the drawItemAt function used by the primary track

    void (*drawItemLabel)(struct track *tg, struct spaceNode *sn,
                          struct hvGfx *hvg, int xOff, int y, int width,
                          MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                          double scale, boolean withLeftLabels);
                          /* Draw the label for an item */

    void (*doItemMapAndArrows)(struct track *tg, struct spaceNode *sn,
                               struct hvGfx *hvg, int xOff, int y, int width,
                               MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                               double scale, boolean withLeftLabels);
    /* Handle item map and exon arrows for an item */
 
    };

struct decorator
    {
    struct decorator *next;
    struct decorator *nextWindow; // points to the copy of this decorator for the next window
    struct decoratorGroup *group; // parent pointer
    struct trackDb *decTdb;
    char *source; // to keep track of which decorator is which
    decorationStyle style;
    struct decoration *decorations;
    struct hash *hash; // takes item labels (chrom:start-end:name) to decorations on that item
    struct hash *spaceSavers; // Used variously for packing adjacent decorations or labels of overlay decorations,
                              // depending on the draw mode of the decorator.  The hash is keyed by decoratedItem
    };

struct decorator *decoratorListFromBbi(struct trackDb *decTdb, char *chrom, struct bigBedInterval *intervalList, struct bigBedFilter *decoratorFilters, int fieldCount, struct mouseOverScheme *mouseScheme);
/* Turn an intervalList (as retrieved from a bbi file) and turn it into a list of decorations.
 * The resulting decorator should also include a hash table that is keyed on the decorated
 * items of the primary track
 */

void decoratorGroupSetExtents (struct decoratorGroup *dg);
/* Update the extents part of the decorator group with the extents of all the decorations
 * from the group.  The resulting mapping takes any chr:start-end:itemName string that
 * defines an item to the earliest decoration start and the last decoration end for
 * decorations that annotate that item within the group.  This information is needed so
 * that we can correctly pack items when some decorations extend past the start or end
 * of their item. */

int hasDecorators(struct track *tg);
/* Does the track have decorators in its trackDb? */

void packDecorators (struct track *tg);
/* Called by packCountRowsOverflow() to handle packing decorations because it affects
 * the relative height of items (some now can occupy multiple rows) */

void drawDecoration(struct decoration *d, struct hvGfx *hvg, double scale, int xOff, int y,
        int heightPer, MgFont *font, boolean filled);
/* Draw a decoration at the provided position.  If filled, draw a solid block/glyph in the
 * decoration's fillColor.  If not filled, draw the outline of the block/glyph in the
 * decoration's primary color.
 */

void drawDecorationLabel(struct decoration *d, struct hvGfx *hvg, double scale, int xOff, int y,
        int heightPer, MgFont *font);
/* Draw a label for the decoration at the provided position
 */

void drawOverflowLabel(int startBase, int endBase, struct hvGfx *hvg, double scale, int xOff, int y,
        int heightPer, MgFont *font);
/* Draw a composite decoration label indicating there are too many decorations to display
 * individual labels.  Not currently used, but might be brought back soon.
 */

void decoratorDrawItemLabel(struct track *tg, struct spaceNode *sn,
                            struct hvGfx *hvg, int xOff, int y, int width,
                            MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                            double scale, boolean withLeftLabels);
/* Wrapper for genericDrawItemLabel that adjusts the planned label position to account for
 * any decorations that have increased the extent or changed placement of the item.
 */

void decoratorDoItemMapAndArrows(struct track *tg, struct spaceNode *sn,
                                 struct hvGfx *hvg, int xOff, int y, int width,
                                 MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                                 double scale, boolean withLeftLabels);
/* Wrapper for genericItemMapAndArrows to push those objects further down the y axis
 * if needed - decorations sometimes push the item lower */


int decoratorGroupHeight(struct track *tg, char *decoratedItem);
/* Return the combined height in rows of all decorations for the decorated item */

int decoratorGroupHeightAbove(struct track *tg, char *decoratedItem);
/* Return the height in rows of the decorations that should appear above the decorated
 * item.  This is used to push the draw of the decorated item further down the page.
 */

void decoratorDrawItemAt(struct track *tg, void *item,
                         struct hvGfx *hvg, int xOff, int y, double scale,
                         MgFont *font, Color color, enum trackVisibility vis);
/* A wrapper to replace a track's drawItemAt function with.  Installed by calling decoratorMethods,
 * which keeps a pointer to the old drawItemAt function.  The old drawItemAt function will be invoked
 * by decoratorDrawItemAt to place the item at the appropriate height, accounting for decorations */

struct decoratorGroup *newDecoratorGroup(void);
/* Allocate and return a new decorator group object.  Also allocates itemExtend and itemOverlayExtents. */

void decoratorMethods (struct track *tg);
/* Replace the standard height and draw functions for a linkedFeatures track with decoration-aware
 * versions.  We'll then feed alternate coordinates to the primary track functions if decorations
 * have shifted those items down at all.
 */

void drawDecorationMouseover(struct decoration *d, struct trackDb *decTdb, struct track *tg, void *item, struct hvGfx *hvg,
        double scale, int xOff, int y, int heightPer, boolean isOverlay);
/* Draw the mouseover block for a decoration.  For adjacent decorations, this is just mouseOver text.  For overlay
 * decorations, this also includes an onclick link to hgc or hgGene for the primary item (to match the onClick link
 * drawn for the decorated item.  Must be called before the decorated item's mouseOver code, as whichever mouseOver
 * is written to the page first has priority.
 */

int decoratorGroupGetOverlayExtent(struct track *tg, char *itemName, int *start, int *end);
/* If itemName has overlay extents defined, populate start and end with those values and return 1.
 * If no overlay extents are defined for the item, return 0.
 */

int decoratorGroupGetExtent(struct track *tg, char *itemName, int *start, int *end);
/* If itemName has extents defined, populate start and end with those values and return 1.
 * If no extents are defined for the item, return 0.
 */


#endif
