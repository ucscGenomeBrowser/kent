
#include "common.h"
#include "hash.h"
#include "hgTracks.h"
#include "decorator.h"
#include "decoration.h"
#include "decoratorUi.h"
#include "bigBed.h"
#include "bigBedFilter.h"
#include "spaceSaver.h"
#include "mouseOver.h"

// Probably move this to a trackDb variable eventually
#define MAX_DECORATION_ROWS 3

struct point
// For keeping track of a reverse path for making a polygon when tracing exon blocks.  We draw the top
// half of the polygon first, then draw its mirror image back for the bottom half.
{
    struct point *next;
    int x;
    int y;
};

struct startEndPair
// Used to track the extents of item+decorations when decorations go outside the boundaries
// of the item (e.g. a decoration annotating a promoter for a transcript)
{
    int start;
    int end;
}; 
   
struct sameDecorationNode
/* sameItem node */
    {
    struct sameDecorationNode *next; /* next node */
    struct window *window;     /* in which window - can use to detect duplicate keys */
    struct decoration *item;
    struct spaceSaver *ss;
    bool done;                 /* have we (reversed the list and) processed it yet? */
    };


struct decoratorGroup *newDecoratorGroup(void)
/* Allocate and return a new decorator group object.  Also allocates itemExtend and itemOverlayExtents. */
{
struct decoratorGroup *new;
AllocVar(new);
new->itemExtents = hashNew(0);
new->itemOverlayExtents = hashNew(0);
return new;
}


void decoratorMethods (struct track *tg)
/* Replace the standard height and draw functions for a linkedFeatures track with decoration-aware
 * versions.  We'll then feed alternate coordinates to the primary track functions if decorations
 * have shifted those items down at all.
 */
{
if (tg->decoratorGroup == NULL)
    {
    errAbort("Attempting to install decorator methods on a track without attached decorators");
    }
tg->decoratorGroup->drawItemAt = tg->drawItemAt;
tg->drawItemAt = decoratorDrawItemAt;
tg->decoratorGroup->drawItemLabel = tg->drawItemLabel;
tg->drawItemLabel = decoratorDrawItemLabel;
tg->decoratorGroup->doItemMapAndArrows = tg->doItemMapAndArrows;
tg->doItemMapAndArrows = decoratorDoItemMapAndArrows;
}


int hasDecorators (struct track *tg)
/* Does the track have decorators in its trackDb? */
{
if (tg->decoratorGroup != NULL)
    return 1;
return 0;
}


int getDecorationStartPixel(struct decoration *d, struct window *w, double scale)
/* Get the start pixel for this decoration in the window.  This is slightly complicated
 * because while a glyph may be defined in the file as spanning a wide start-end range
 * (in its BED coordinates), the glyph will only ever be drawn at the very center of
 * those coordinates (it's intended to mark the center of the range). */
{
// Assume it's a block to start
int baseStart = d->chromStart;
int startPixel;

if (baseStart <= w->winStart)
    startPixel = 0;
else
    startPixel = round((double)(baseStart - w->winStart)*scale);

// Switch to glyph calculations if necessary
if (decorationGetStyle(d) == DECORATION_STYLE_GLYPH)
    {
    // glyphs must be adjacent if we've reached this point
    int baseEnd = d->chromEnd;
    int centeredBaseStart = (baseStart + baseEnd)/2;
    int centeredBaseEnd = (baseStart + baseEnd + 1)/2;
    int centeredBaseMidpoint = (centeredBaseStart + centeredBaseEnd)/2;
    int middlePixel = round((double)(centeredBaseMidpoint - w->winStart)*scale);
    startPixel = middlePixel - (tl.fontHeight/2);
    if (startPixel < 0)
        startPixel = 0;
    }
return startPixel;
}


int getDecorationEndPixel(struct decoration *d, struct window *w, double scale)
/* Get the end pixel for this decoration in the window.  This is slightly complicated
 * because while a glyph may be defined in the file as spanning a wide start-end range
 * (in its BED coordinates), the glyph will only ever be drawn at the very center of
 * those coordinates (it's intended to mark the center of the range). */
{
int baseEnd = d->chromEnd;
int end;

if (baseEnd >= w->winEnd)
    end = w->insideWidth;
else
    end = round((baseEnd - w->winStart)*scale);

// Switch to glyph calculations if necessary
if (decorationGetStyle(d) == DECORATION_STYLE_GLYPH)
    {
    int baseStart = d->chromStart;
    int centeredBaseStart = (baseStart + baseEnd)/2;
    int centeredBaseEnd = (baseStart + baseEnd + 1)/2;
    int centeredBaseMidpoint = (centeredBaseStart + centeredBaseEnd)/2;
    int middlePixel = round((double)(centeredBaseMidpoint - w->winStart)*scale);
    int start = middlePixel - (tl.fontHeight/2);
    end = start + tl.fontHeight;
    if (end > w->insideWidth)
        end = w->insideWidth;
    }
return end;
}


void decoratorDrawItemAt(struct track *tg, void *item,
                         struct hvGfx *hvg, int xOff, int y, double scale,
                         MgFont *font, Color color, enum trackVisibility vis)
/* A wrapper to replace a track's drawItemAt function with.  Installed by calling decoratorMethods,
 * which keeps a pointer to the old drawItemAt function.  The old drawItemAt function will be invoked
 * by decoratorDrawItemAt to place the item at the appropriate height, accounting for decorations */
{
struct linkedFeatures *lf = item;
char itemName[4096];
struct decorator *td;
safef(itemName, sizeof(itemName), "%s:%d-%d:%s", chromName, lf->start, lf->end, lf->name);
int rowsAbove = decoratorGroupHeightAbove(tg, itemName); // should I be passing in vis?
int itemY = y + rowsAbove*(tg->lineHeight);

// Draw the primary track item without doing its mouseover yet
tg->decoratorGroup->drawItemAt(tg, item, hvg, xOff, itemY, scale, font, color, vis);

int primaryItemRows = 1;
if (tg->itemHeightRowsForPack)
    primaryItemRows = tg->itemHeightRowsForPack(tg, item);
int overlayHeightPer = tg->heightPer*primaryItemRows;

// Draw overlay decorations and their labels, as the labels will ascend above the item
int yOffset = itemY - tg->lineHeight;
for (td = tg->decoratorGroup->decorators; td != NULL; td = td->next)
    {
    if (td->style == DECORATION_STYLE_BLOCK || td->style == DECORATION_STYLE_GLYPH)
        {
        if (decoratorDrawMode(td->decTdb, cart, td->style) == DECORATOR_MODE_OVERLAY)
            {
            // draw overlay decorations and mouseovers
            struct hashEl *hel;
            for (hel = hashLookup(td->hash, itemName); hel != NULL; hel = hashLookupNext(hel))
                {
                struct decoration *d = hel->val;
                drawDecoration(d, hvg, scale, xOff, itemY, overlayHeightPer, font, TRUE);
                // in the future, we might want to break out the outline drawing until after all
                // the fill blocks are drawn.  That way we won't risk overwriting boundary lines
                // with the fill blocks of subsequent, overlapping decorations.
                drawDecoration(d, hvg, scale, xOff, itemY, overlayHeightPer, font, FALSE);
                drawDecorationMouseover(d, td->decTdb, tg, item, hvg, scale, xOff, itemY, tg->heightPer, TRUE);
                }
            // draw any packed decoration labels unless we're in dense or squish
            struct spaceSaver *ss = hashFindVal(td->spaceSavers, itemName);
            if (ss == NULL || (vis != tvFull && vis != tvPack))
                continue;
            struct spaceNode *sn;
            for (sn = ss->nodeList; sn != NULL; sn = sn->next)
                {
                int labelY = yOffset - (sn->row * tg->lineHeight);
                struct decoration *d = (struct decoration*) sn->val;
                if (sn->row >= MAX_DECORATION_ROWS) // overflow label
                    {
                    // Do nothing right now - we don't currently draw an overflow row,
                    // but it sounds like we're about to reverse that decision
                    /*
                    if (!needOverflowLabel)
                        {
                        needOverflowLabel = TRUE;
                        overflowStart = d->chromStart;
                        overflowEnd = d->chromEnd;
                        }
                    else
                        {
                        if (d->chromStart < overflowStart)
                            overflowStart = d->chromStart;
                        if (d->chromEnd > overflowEnd)
                            overflowEnd = d->chromEnd;
                        }
                    */
                    }
                else
                    {
                    int clipX, clipY, clipWidth, clipHeight;
                    hvGfxGetClip(hvg, &clipX, &clipY, &clipWidth, &clipHeight);
                    hvGfxUnclip(hvg);
                    drawDecorationLabel(d, hvg, scale, xOff, labelY, tg->heightPer, font);
                    hvGfxSetClip(hvg, clipX, clipY, clipWidth, clipHeight);
                    }
                }
            yOffset -= ss->rowCount*tg->lineHeight;
            }
        }
    }

// now to draw adjacent blocks underneath the item, but at half height
yOffset = itemY + tg->lineHeight*primaryItemRows;
for (td = tg->decoratorGroup->decorators; td != NULL; td = td->next)
    {
    if (td->style == DECORATION_STYLE_BLOCK && decoratorDrawMode(td->decTdb, cart, td->style) == DECORATOR_MODE_ADJACENT)
        {
            struct spaceSaver *ss = hashFindVal(td->spaceSavers, itemName);
            if (ss == NULL)
                continue;
            struct spaceNode *sn;
            for (sn = ss->nodeList; sn != NULL; sn = sn->next)
                {
                int adjacentY = yOffset + (sn->row * (tg->lineHeight/2));
                struct decoration *d = (struct decoration*) sn->val;
                drawDecoration(d, hvg, scale, xOff, adjacentY, (tg->heightPer/2), font, TRUE);
                drawDecoration(d, hvg, scale, xOff, adjacentY, (tg->heightPer/2), font, FALSE);
                drawDecorationMouseover(d, td->decTdb, tg, item, hvg, scale, xOff, adjacentY, (tg->heightPer/2), FALSE);
                }
            yOffset += ss->rowCount*(tg->lineHeight/2);
        }
    }

// now to draw adjacent glyphs underneath the item, building off the yOffset established by adjacent blocks
for (td = tg->decoratorGroup->decorators; td != NULL; td = td->next)
    {
    if (td->style == DECORATION_STYLE_GLYPH && decoratorDrawMode(td->decTdb, cart, td->style) == DECORATOR_MODE_ADJACENT)
        {
            struct spaceSaver *ss = hashFindVal(td->spaceSavers, itemName);
            if (ss == NULL)
                continue;
            struct spaceNode *sn;
            for (sn = ss->nodeList; sn != NULL; sn = sn->next)
                {
                int adjacentY = yOffset + (sn->row * tg->lineHeight);
                struct decoration *d = (struct decoration*) sn->val;
                drawDecoration(d, hvg, scale, xOff, adjacentY, tg->heightPer, font, TRUE);
                drawDecoration(d, hvg, scale, xOff, adjacentY, tg->heightPer, font, FALSE);
                drawDecorationMouseover(d, td->decTdb, tg, item, hvg, scale, xOff, adjacentY, tg->heightPer, FALSE);
                }
            yOffset += ss->rowCount*tg->lineHeight;
        }
    }

}


void decoratorDrawItemLabel(struct track *tg, struct spaceNode *sn,
                            struct hvGfx *hvg, int xOff, int y, int width,
                            MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                            double scale, boolean withLeftLabels)
/* Wrapper for genericDrawItemLabel that adjusts the planned label position to account for
 * any decorations that have increased the extent or changed placement of the item.
 */
{
int labelY = y;
int labelX = xOff;
if (hasDecorators(tg))
    {
    struct linkedFeatures *lf = (struct linkedFeatures*) sn->val;
    char itemName[4096];
    safef(itemName, sizeof(itemName), "%s:%d-%d:%s", chromName, lf->start, lf->end, lf->name);
    labelY +=  decoratorGroupHeightAbove(tg, itemName)*tg->lineHeight;
    int s = lf->start;
    int sClp = (s < winStart) ? winStart : s;
    int x1 = round((sClp - winStart)*scale);
    int decorationStart;
    if (decoratorGroupGetOverlayExtent(tg, itemName, &decorationStart, NULL))
        {
        if (decorationStart < x1)
            labelX -= (x1 - decorationStart);
        }
    }
tg->decoratorGroup->drawItemLabel(tg, sn, hvg, labelX, labelY, width, font, color, labelColor,
        vis, scale, withLeftLabels);
}


void decoratorDoItemMapAndArrows(struct track *tg, struct spaceNode *sn,
                                 struct hvGfx *hvg, int xOff, int y, int width,
                                 MgFont *font, Color color, Color labelColor, enum trackVisibility vis,
                                 double scale, boolean withLeftLabels)
/* Wrapper function for the linkedFeatures doItemMapAndArrows.  This pushes down the y coordinate for the
 * primary item map/arrows to account for any decorations that have pushed down the draw of that item.
 */
{
int itemY = y;
if (hasDecorators(tg))
    {
    struct linkedFeatures *lf = (struct linkedFeatures*) sn->val;
    char itemName[4096];
    safef(itemName, sizeof(itemName), "%s:%d-%d:%s", chromName, lf->start, lf->end, lf->name);
    itemY +=  decoratorGroupHeightAbove(tg, itemName)*tg->lineHeight;
    }
tg->decoratorGroup->doItemMapAndArrows(tg, sn, hvg, xOff, itemY, width, font, color, labelColor, vis, scale, withLeftLabels);
}


void addDecoratorNextWindowPointers(struct track *tg, struct decorator *activeDecorator)
/* Arranges nextWindow pointers for a particular decorator; these will be used in decoration packing.
 * tg should be a pointer to a track in the first window.
 */
{
struct decorator *lastDecorator = NULL;
for(; tg != NULL; tg=tg->nextWindow)
    {
    struct decorator *tgDecorator = tg->decoratorGroup->decorators;
    // locate the copy of this decorator in each window
    while (tgDecorator != NULL && ( differentStringNullOk(tgDecorator->source, activeDecorator->source) ||
                                    tgDecorator->style != activeDecorator->style) )
        {
        tgDecorator = tgDecorator->next;
        }
    if (tgDecorator == NULL)
        {
        // apparently the decorator isn't part of this window.  Should never happen.
        errAbort("window for %s is missing a decorator", tg->track);
        }
    // Set up nextWindow pointers for this decorator
    if (lastDecorator != NULL)
        {
        lastDecorator->nextWindow = tgDecorator;
        }
    lastDecorator = tgDecorator;
    }
}


void addDecoratorNamesToHash(struct decorator *activeDecorator, struct hash *itemNames)
/* Add the names of all decorated items from this decorator or matching decorators in
 * subsequent windows to a deduplicated hash */
{
while (activeDecorator != NULL)
    {
    struct decoration *thisDecoration = activeDecorator->decorations;
    while (thisDecoration != NULL)
        {
        if (hashLookup(itemNames, thisDecoration->decoratedItem) == NULL)
            {
            hashAdd(itemNames, thisDecoration->decoratedItem, NULL);
            }
        thisDecoration = thisDecoration->next;
        }
    activeDecorator = activeDecorator->nextWindow;
    }
}


struct hash *makeSameDecorationHash(struct decorator *activeDecorator, char *itemName)
/* Run through the decorations for this item in each window and assemble a hash of them
 * keyed by chr:start-end:name.  This will be used to decide if decorations should
 * be packed on the same line across those windows (if the names match, they're likely to
 * be the same long decoration just spread across multiple windows).
 */
{
struct hash *sameDecoration = newHash(10);
struct window *w;
struct decorator *tgDecorator;
for(w=windows,tgDecorator=activeDecorator; w; w=w->next,tgDecorator=tgDecorator->nextWindow)
    {
    char *chrom = w->chromName;
    struct decoration *thisDecoration;
    if (tgDecorator->hash == NULL)
        continue;  // looks like there are no decorations in this window

    struct hashEl *decorationEl = NULL;
    // loop over the decorations associated with the item in this window, add them to sameDecoration
    for (decorationEl = hashLookup(tgDecorator->hash, itemName); decorationEl != NULL;
                decorationEl = hashLookupNext(decorationEl))
        {
        thisDecoration = decorationEl->val;
        int baseStart = thisDecoration->chromStart;
        int baseEnd = thisDecoration->chromEnd;
        if (baseStart < w->winEnd && baseEnd > w->winStart)
            // it intersects the window
            {
            struct spaceSaver *itemSs = hashFindVal(tgDecorator->spaceSavers, thisDecoration->decoratedItem);
                // itemSs is not used for packing, but the nodeList and rowCount are used to keep track
                // of decoration placement in each window so they're easily found later during draw.
            if (itemSs == NULL)
                {
                itemSs = spaceSaverNew(0, fullInsideWidth, MAX_DECORATION_ROWS);
                hashAddUnique(tgDecorator->spaceSavers, thisDecoration->decoratedItem, itemSs);
                }
            char key[2048];
            // There might be some decoration duplication here, if names aren't particularly unique.
            // Not going to worry about that right now; it might not matter anyway (and we do
            // the same thing with primary track items).
            safef(key, sizeof key, "%s:%d-%d %s", chrom, baseStart, baseEnd, thisDecoration->name);
            // add the this sameDecorationNode to the list for the key
            struct hashEl *hel = hashLookup(sameDecoration, key);
            if (hel == NULL)
                {
                hashAdd(sameDecoration, key, NULL);
                hel = hashLookup(sameDecoration, key);
                }
            struct sameDecorationNode *sdn;
            AllocVar(sdn);
            sdn->window = w;
            sdn->item = thisDecoration;
            sdn->ss = itemSs;
            slAddHead(&hel->val, sdn);
            }
        }
    }
return sameDecoration;
}


boolean shouldShowDecoratorLabels(struct track *tg, struct decorator *activeDecorator)
/* Determine whether labels should be drawn for this decorator, based on mode,
 * track visibility, and whether the UI checkbox is checked.  Right now, the
 * MaxLabeledBases check is done elsewhere, but it will likely be moved here in
 * the future.
 */
{
decorationStyle drawStyle = activeDecorator->style;
decoratorMode drawMode = decoratorDrawMode(activeDecorator->decTdb, cart, drawStyle);

boolean showLabelsBoxChecked = decoratorUiShowOverlayLabels(activeDecorator->decTdb, cart);

if (drawStyle == DECORATION_STYLE_BLOCK && drawMode == DECORATOR_MODE_OVERLAY &&
        (tg->visibility == tvPack || tg->visibility == tvFull) && showLabelsBoxChecked)
    {
    return TRUE;
    }
return FALSE;
}


void addToExtents(struct hash *extentHash, struct window *w, char *key, int start, int end)
/* Augment the extent of the given key with a new start and end, expanding if necessary.
 * This is used to track the maximum overall extent of multiple decorations all applied to
 * the same track item, any one of which might extend beyond the bounds of the item. (e.g.
 * to mark a promoter or a missing section of a truncated transcript).
 */
{
if (isEmpty(key))
    {
    errAbort("trying to augment item extent without an item name");
    }
struct decoratedItemExtent *extent = hashFindVal(extentHash, key);
if (extent == NULL)
    {
    AllocVar(extent);
    extent->start = w->insideX + insideWidth;
    extent->end = w->insideX;
    hashAddUnique(extentHash, key, extent);
    }
if (start < extent->start)
    extent->start = start;
if (end > extent->end)
    extent->end = end;
}


void addDecorationsToSpaceSaver(struct spaceSaver *ss, struct window *w, struct track *tg,
                                struct decorator *tgDecorator, char *thisItemName, struct hash *sameDecoration)
/* Run through this item's decorations for this current decorator/window.  If any haven't yet been
 * added to the spaceSaver, then assemble a rangeList from that decoration and any matching decorations
 * in later windows, add that rangelist to the spacesaver, and mark those decorations as done.
 * This function bears a great deal of similarity to portions of the spaceSaver layout in
 * packCountRowsOverflow().
 */
{
decorationStyle drawStyle = tgDecorator->style;
decoratorMode drawMode = decoratorDrawMode(tgDecorator->decTdb, cart, drawStyle);
char *chrom = w->chromName;
MgFont *font = tl.font;
int extraWidth = tl.mWidth;
double scale = scaleForWindow(w->insideWidth, w->winStart, w->winEnd);
struct hashEl *decorationEl = NULL;

long basesInImage = insideWidth*basesPerPixel;

if (drawStyle == DECORATION_STYLE_BLOCK && drawMode == DECORATOR_MODE_OVERLAY &&
        ( !decoratorUiShowOverlayLabels(tgDecorator->decTdb, cart) ||
          basesInImage > decoratorUiMaxLabeledBaseCount(tgDecorator->decTdb, cart)
        )
   )
    {
    // No need to pack labels for an overlay decorator if they're turned off!
    return;
    }

for (decorationEl = hashLookup(tgDecorator->hash, thisItemName); decorationEl != NULL;
            decorationEl = hashLookupNext(decorationEl))
    {
    struct decoration *thisDecoration = decorationEl->val;
    int itemStart, itemEnd;
    decorationGetParentExtent(thisDecoration, NULL, &itemStart, &itemEnd);

    int baseStart = thisDecoration->chromStart;
    int baseEnd = thisDecoration->chromEnd;
    if (baseStart < w->winEnd && baseEnd > w->winStart)
        { // it intersects the window.  Look for the same decoration in other windows
        char key[2048];
        safef(key, sizeof key, "%s:%d-%d %s", chrom, baseStart, baseEnd, thisDecoration->name);
        struct hashEl *hel = hashLookup(sameDecoration, key);
        if (!hel)
            {
            // Something's wrong - if a decoration exists, it should be listed there.  Most likely
            // this would have happened because the track load timed out.  If so, we can just exit
            // and let the primary track error reporting handle the situation.  If not, abort.
            if (tg->networkErrMsg)
                break;
            errAbort("unexpected error: lookup of key [%s] in sameDecoration hash failed.", key);
            }
        struct sameDecorationNode *sdn = (struct sameDecorationNode *)hel->val;
        if (!sdn)
            errAbort("sin empty (hel->val)!");
        if (!sdn->done)
            { // hasn't been done yet!
            slReverse(&hel->val);
            sdn = (struct sameDecorationNode *)hel->val;
            }

        struct window *firstWin = sdn->window;  // first window
        struct window *lastWin = NULL;
        bool foundWork = FALSE;

        struct spaceRange *rangeList=NULL, *range;
        struct spaceNode *nodeList=NULL, *node;
        // int rangeEnd = 0; // where the last piece of this range ends, used for calculating the center
        // currently unused unless we return to centered labels (currently in discussion)
        for(; sdn; sdn=sdn->next)
            {
            if (sdn->window != firstWin && !foundWork)
                break;  // If we have not found work within the first window, we never will.

            if (sdn->done)  // If the node has already been done by a previous pass then skip
                continue;

            if (lastWin == sdn->window)  // If we already did one in this window, skip to the next window
                continue;
            
            sdn->done = TRUE;
            foundWork = TRUE;
            lastWin = sdn->window;

            int winOffset = sdn->window->insideX - fullInsideX;
            struct decoration *item = sdn->item;
            struct window *w = sdn->window;

            int start = getDecorationStartPixel(item, w, scale);
            int end = getDecorationEndPixel(item, w, scale);

            // Old assumption was that if we get here, a decoration must have been
            // determined to be visible.  New plan is that we'll sometimes get coordinates
            // that indicate a decoration is out of range, and we'll reject those here.
            // This could happen e.g. with a glyph defined on a wide BED range, but whose
            // center position (where the glyph is drawn) is outside the draw window.
            if (end < 0 || start > w->insideWidth)
                continue;

            AllocVar(range);
            range->start = start + winOffset;
            range->end = end + winOffset;
            slAddHead(&rangeList, range);
            // rangeEnd = range->end;
            // Here I'm packing decorations, so range->height should always be 1 (1 "row")
            range->height = 1;

            AllocVar(node);
            node->val = item;
            node->parentSs = sdn->ss;
            node->noLabel = FALSE;
            slAddHead(&nodeList, node);
            }

        if (rangeList == NULL) // foundWork calculations get messy when dealing with glyphs that might not
            continue;           // not be drawn in a window even if their range intersects it

        slReverse(&rangeList);
        slReverse(&nodeList);

        // the above calculation works for adjacent blocks and glyphs.  Packing overlay blocks
        // is different; instead we're packing the labels, which have their own
        // pixel extents.  We'll try to draw the label aligned to the left edge of the portion
        // of the decoration that is drawn.
        if (drawStyle == DECORATION_STYLE_BLOCK && drawMode == DECORATOR_MODE_OVERLAY)
            {
            // we make a single range that might not technically be a part of the window
            // we're assigning it to, but close enough?
            range = rangeList;
            int labelSize = mgFontStringWidth(font, thisDecoration->name) + extraWidth;
            // Maybe we'll return to a centered-label idea; see what Mark thinks.
            // int centerPixel =  (range->start + rangeEnd)/2;
            // int startPixel = centerPixel - labelSize/2;
            // if (startPixel < 0)
            //     startPixel = 0;
            int startPixel = range->start; // record where the first instance of this decoration starts

            // now cannibalize the range structure - instead of holding the decoration, it's just holding the label
            range->start = startPixel;
            range->end = startPixel + labelSize;
            range->next = NULL; // memory leak
            nodeList->next = NULL;
            addToExtents(tgDecorator->group->itemExtents, w, thisDecoration->decoratedItem, startPixel, startPixel + labelSize);
            }

        boolean doPadding = TRUE;
        // always allow overflow for decorations
        if (spaceSaverAddOverflowMultiOptionalPadding(
                            ss, rangeList, nodeList, TRUE, doPadding) == NULL)
            break;

        }
    }
}


void doPerItemSpaceSaverLayout(struct track *tg, struct decorator *activeDecorator, char *thisItemName, struct hash *sameDecoration)
/* Do spaceSaver layout for the decorations on one item across all windows it appears in.
 * activeDecorator should be a decorator for a track in the first window.
 */
{
struct spaceSaver *ss = spaceSaverNew(0, fullInsideWidth, MAX_DECORATION_ROWS);
struct window *w;
struct decorator *tgDecorator;

for(w=windows,tgDecorator=activeDecorator; w; w=w->next,tgDecorator=tgDecorator->nextWindow)
    {
    struct spaceSaver *perItemWindowSs = hashMustFindVal(tgDecorator->spaceSavers, thisItemName);
    // Do some basic setup on this local spaceSaver
    perItemWindowSs->vis = tg->visibility;
    if (perItemWindowSs->vis == tvFull)
        perItemWindowSs->vis = tvPack;
    perItemWindowSs->window = w;

    // Run through the item's decorations in this window, packing any that haven't
    // already been packed by being extensions of items from previous windows.
    // This might add more items to perItemWindowSs.
    addDecorationsToSpaceSaver(ss, w, tg, tgDecorator, thisItemName, sameDecoration);

    // Can finish this window's local spaceSaver now; we'll only ever add more items to
    // the local spaceSavers for later windows.
    spaceSaverFinish(perItemWindowSs);
    }

// Assign final rows count at the end so all per-window spaceSavers agree how high they are.
// Do this for each spaceSaver that corresponds to activeDecorator.
for(tgDecorator=activeDecorator; tgDecorator; tgDecorator=tgDecorator->nextWindow)
    {
    struct spaceSaver *perItemWindowSs = hashMustFindVal(tgDecorator->spaceSavers, thisItemName);
    perItemWindowSs->rowCount   = ss->rowCount;
    perItemWindowSs->hasOverflowRow = ss->hasOverflowRow;
    }
spaceSaverFree(&ss);
}


void setupExtents(struct decorator *activeDecorator)
/* Set up the extents for each decorated item in this decorator.  Extents measure two things:
 * how far decorations extend the item on its own line (important for padding the position of the
 * item's label), and how far decorations extend the item overall (important for packing the item).
 *
 * tg should be a track in the first window, and activeDecorator should be the decorator in that
 * track that we're processing.
 *
 * This handles the extents for all windows based on the pixel positions of the decoration
 * blocks, but the decoration labels are another matter - placement of the label depends
 * on multi-region linkages that aren't established yet.  We add the label extents
 * in later after we figure out where they are.
 */
{
struct window *w = windows;
decorationStyle drawStyle = activeDecorator->style;
decoratorMode drawMode = decoratorDrawMode(activeDecorator->decTdb, cart, drawStyle);
struct decorator *decorator = activeDecorator;
for (; w != NULL; w=w->next, decorator=decorator->nextWindow)
    {
    double scale = scaleForWindow(w->insideWidth, w->winStart, w->winEnd);
    struct decoratorGroup *group = decorator->group;
    struct decoration *d = decorator->decorations;
    while (d != NULL)
        {
        int start = getDecorationStartPixel(d, w, scale);
        int end = getDecorationEndPixel(d, w, scale);
        if (drawMode == DECORATOR_MODE_OVERLAY)
            {
            // augment the overlay extent for the item this decoration affects
            addToExtents(group->itemOverlayExtents, w, d->decoratedItem, start, end);
            }
        // augment the overall extent of the item, which might also include the item label
        addToExtents(group->itemExtents, w, d->decoratedItem, start, end);
        d = d->next;
        }
    }
}


void createPerItemSpaceSavers(struct decorator *d, struct hash *itemNames)
/* If an item has a decoration in any window (or at least a decoration that requires a spacesaver),
 * then we need a corresponding spaceSaver for that item in every window to ensure draw height for
 * the item is consistent across all windows.  This creates those spaceSavers.
 * itemNames should be deduplicated.
 */
{
for (; d != NULL; d = d->nextWindow)
    {
    struct hashEl *itemLoopEl;
    struct hashCookie cookie = hashFirst(itemNames);
    while ((itemLoopEl = hashNext(&cookie)) != NULL)
        {
        char *itemName = itemLoopEl->name;
        struct spaceSaver *ss = spaceSaverNew(0, fullInsideWidth, MAX_DECORATION_ROWS);
        hashAddUnique(d->spaceSavers, itemName, ss);
        }
    }
}


void packDecorators (struct track *tg)
/* Set up a spaceSaver for each decorated item.  This will allow us to calculate the additional
 * itemHeight from labels and/or decorations draw in adjacent mode. 
 * Where the hell am I storing all this? Ah, in each decorator in a spaceSavers hash.
 *
 * I need tg because I need to use the tg->nextWindow (or w/e) pointer to track down
 * the same decorator in subsequent windows (for packing things on the same line ala
 * sameItem)
 */
{
// start of section copied (with modification) from packCountRowsOverflow()

// Leaving this sanity check in, but it should never be called since this is only used from
// within packCountRowsOverflow, which has already done this check.
if (trackLoadingInProgress)
    {
    return;
    }

// If there are no decorators, there's nothing to pack (why were we called?)
if (tg->decoratorGroup == NULL || tg->decoratorGroup->decorators == NULL)
    {
    return;
    }

// do not re-calculate if not needed
if (tg->decoratorGroup->itemExtents->elCount != 0)
    {
    return;
    }

// should never happen - again, this is only called by packCountRowsOverflow, and it already checks this
if (currentWindow != windows)  // if not first window
    {
    errAbort("unexpected current window %lu while packing decorations, expected first window %lu", (unsigned long) currentWindow, (unsigned long) windows);
    }

// Now set up to actually pack the items in each decorator

struct decorator *activeDecorator = tg->decoratorGroup->decorators;

// Pack items from one decorator source at a time, so we're not mixing decorations from difference sources
// on the same rows (imagine they're different tracks that we're packing).  Design choice, could be revisited
// later if we favor more density.
while (activeDecorator != NULL)
    {
    // Two loops to perform.  The first is setup, the second is execution of the packing.
    // Setup needs to 1. make a list of all decorated items in the track across all windows;
    // we'll pack those in sequence as effectively each decorated item is its own little
    // browser screen with a series of packed tracks (the decorators).  2. link each
    // decorator structure to the corresponding decorator in the next window (similar to
    // tg->nextWindow).  This is for convenience.
    decorationStyle drawStyle = activeDecorator->style;
    decoratorMode drawMode = decoratorDrawMode(activeDecorator->decTdb, cart, drawStyle);
    addDecoratorNextWindowPointers(tg, activeDecorator);
    setupExtents(activeDecorator);

    if (drawStyle == DECORATION_STYLE_GLYPH && drawMode != DECORATOR_MODE_ADJACENT)
        {
        // Nothing to pack above or below the decorated item in this case.
        // NB: until I change this, overlay glyphs can't contribute to item pixel extent,
        // which means their presence won't pad out the position of the eventual item label draw
        activeDecorator = activeDecorator->next;
        continue;
        }
        if (drawStyle == DECORATION_STYLE_BLOCK && drawMode == DECORATOR_MODE_OVERLAY &&
                !shouldShowDecoratorLabels(tg, activeDecorator))
        {
        // We don't draw labels for overlay blocks if the primary track isn't in pack or full mode
        activeDecorator = activeDecorator->next;
        continue;
        }

    // Setup loop - assemble the full decorated item list across all windows
    struct hash *itemNames = hashNew(0);
    addDecoratorNamesToHash(activeDecorator, itemNames);
    createPerItemSpaceSavers(activeDecorator, itemNames);

    // Now pack the decorations for one decorated item at a time.
    struct hashEl *itemLoopEl;
    struct hashCookie cookie = hashFirst(itemNames);
    while ((itemLoopEl = hashNext(&cookie)) != NULL)
        {
        char *thisItemName = itemLoopEl->name;
        // find out which decorations for this item are shared across windows and belong on the same row
        struct hash *sameDecoration = makeSameDecorationHash(activeDecorator, thisItemName);
            // would be nice to clean up this memory @ end of loop

        // do spaceSaver layout for the decorations on this item
        doPerItemSpaceSaverLayout(tg, activeDecorator, thisItemName, sameDecoration);
        }
    activeDecorator = activeDecorator->next;
    } // end of per-decorator loop
}


void addToDecorator(struct decorator *decorator, struct decoration *decoration, char *mouseOverText)
/* Small function that just gets repeated a lot - add the decoration to the decorator and
 * set its mouseOver text.
 */
{
if (decorator == NULL || decoration == NULL)
    return;
slAddHead(&decorator->decorations, decoration);
hashAdd(decorator->hash, decoration->decoratedItem, decoration);
if (isNotEmpty(mouseOverText))
    {
    decoration->mouseOver = mouseOverText;
    }
}


struct decorator *decoratorListFromBbi(struct trackDb *decTdb, char *chrom, struct bigBedInterval *intervalList, struct bigBedFilter *decoratorFilters, struct bbiFile *bbi, struct mouseOverScheme *mouseScheme)
/* Take an intervalList (as retrieved from a bbi file) and turn it into a list of decorations.
 * The resulting decorator should also include a hash table that is keyed on the decorated
 * items of the primary track
 */
{
struct decorator *newGlyphs = NULL, *newBlocks = NULL;
AllocVar(newGlyphs);
AllocVar(newBlocks);
newGlyphs->style = DECORATION_STYLE_GLYPH;
newBlocks->style = DECORATION_STYLE_BLOCK;
newGlyphs->next = newBlocks->next = NULL;
newGlyphs->hash = hashNew(0);
newBlocks->hash = hashNew(0);
newGlyphs->decorations = newBlocks->decorations = NULL;
newGlyphs->spaceSavers = hashNew(0);
newBlocks->spaceSavers = hashNew(0);
newGlyphs->decTdb = newBlocks->decTdb = decTdb; // I don't think this will cause a destructor problem?
newGlyphs->source = trackDbSetting(decTdb, "bigDataUrl");
newBlocks->source = trackDbSetting(decTdb, "bigDataUrl");

int badStyleCount = 0;
struct bigBedInterval *thisInterval = NULL;
for (thisInterval = intervalList; thisInterval != NULL; thisInterval = thisInterval->next)
    {
    if (decoratorFilters != NULL)
        {
        char *bedRow[bbi->fieldCount];
        char startBuf[16], endBuf[16];
        bigBedIntervalToRow(thisInterval, chrom, startBuf, endBuf, bedRow, ArraySize(bedRow));
        if (!bigBedFilterInterval(bbi, bedRow, decoratorFilters))
            continue;
        }
    struct decoration *newDec = decorationFromInterval(chrom, thisInterval);
    // have to assemble any custom mouseover text here because it may depend on extra bbi fields
    // that won't be included in the decoration structure
    char *mouseOverText = mouseOverGetBbiText(mouseScheme, thisInterval, chrom);
    if (decorationGetStyle(newDec) == DECORATION_STYLE_BLOCK)
        {
        addToDecorator(newBlocks, newDec, mouseOverText);
        }
    else if (decorationGetStyle(newDec) == DECORATION_STYLE_GLYPH)
        {
        addToDecorator(newGlyphs, newDec, mouseOverText);
        }
    else
        badStyleCount++;
    }
if (badStyleCount)
    warn ("%d decorations had unrecognized style values in %s", badStyleCount, decTdb->track);
return slCat(newBlocks, newGlyphs);
}


void decoratorGroupSetExtents (struct decoratorGroup *dg)
/* Update the extents part of the decorator group with the extents of all the decorations
 * from the group.  The resulting mapping takes any chr:start-end:itemName string that
 * defines an item to the earliest decoration start and the last decoration end for
 * decorations that annotate that item within the group.  This information is needed so
 * that we can correctly pack items when some decorations extend past the start or end
 * of their item. */
{
struct decorator *thisDecorator = dg->decorators;
while (thisDecorator != NULL)
    {
    struct decoration *thisDecoration = thisDecorator->decorations;
    while (thisDecoration != NULL)
        {
        struct decoratedItemExtent *extent = hashFindVal(dg->itemExtents, thisDecoration->decoratedItem);
        if (extent == NULL)
        {
            AllocVar(extent);
            extent->start = thisDecoration->chromStart;
            extent->end = thisDecoration->chromEnd;
            hashAdd(dg->itemExtents, thisDecoration->decoratedItem, extent);
        }
        if (thisDecoration->chromStart < extent->start)
            extent->start = thisDecoration->chromStart;
        if (thisDecoration->chromEnd > extent->end)
            extent->end = thisDecoration->chromEnd;
        thisDecoration = thisDecoration->next;
        }
    thisDecorator = thisDecorator->next;
    }
    // might be nice to dispose of the hash if it has nothing in it at this point
}



/***** Height tracking functions, useful for packing primary track items *****/


int decoratorGroupHeight(struct track *tg, char *decoratedItem)
/* Return the combined height in rows of all decorations for the decorated item */
{
int rowCount = 0;
struct decorator *td;
for (td = tg->decoratorGroup->decorators; td != NULL; td = td->next)
    {
    if (decoratorDrawMode(td->decTdb, cart, td->style) == DECORATOR_MODE_HIDE)
        continue; // skip hidden decoration sources
    if (tg->visibility != tvFull && tg->visibility != tvPack)
        {
        // no block overlay labels are drawn in dense or squish mode
        if ((td->style == DECORATION_STYLE_BLOCK) && 
                (decoratorDrawMode(td->decTdb, cart, td->style) == DECORATOR_MODE_OVERLAY))
            continue;
        }
    struct spaceSaver *itemSaver = hashFindVal(td->spaceSavers, decoratedItem);
    if (itemSaver != NULL)
        {
        if ((td->style == DECORATION_STYLE_BLOCK) && 
                (decoratorDrawMode(td->decTdb, cart, td->style) == DECORATOR_MODE_ADJACENT))
            {
            // adjacent blocks are drawn at half-height compared to whatever the track height is,
            // so they only add half a row each to the height of the track.  We do currently
            // draw an overflow row for adjacent decorations.
            int extraRow = 0;
            if (itemSaver->hasOverflowRow)
                extraRow = 1; // overflow half row
            rowCount += (itemSaver->rowCount + extraRow + 1)/2;
            }
        else
            {
            rowCount += itemSaver->rowCount;
            if (itemSaver->hasOverflowRow && td->style == DECORATION_STYLE_GLYPH)
                {
                rowCount += 1; // glyphs still get an overflow row
                }
            }
        }
    }
    return rowCount;
}


int decoratorGroupHeightAbove(struct track *tg, char *decoratedItem)
/* Return the height in rows of the decorations that should appear above the decorated
 * item.  This is used to push the draw of the decorated item further down the page.
 * Right now, only block labels are drawn above an item, and then only in pack
 * or full mode.
 */
{
int rowCount = 0;
struct decorator *td;
if (tg->visibility != tvFull && tg->visibility != tvPack)
    {
    // only draw decoration labels in pack or full mode
    return rowCount;
    }
for (td = tg->decoratorGroup->decorators; td != NULL; td = td->next)
    {
    if ((td->style != DECORATION_STYLE_BLOCK) ||
         (decoratorDrawMode(td->decTdb, cart, td->style) != DECORATOR_MODE_OVERLAY))
        {
        continue; // only block overlay labels result in stuff being drawn above an item
        }
    struct spaceSaver *itemSaver = hashFindVal(td->spaceSavers, decoratedItem);
    if (itemSaver != NULL)
        {
        rowCount += itemSaver->rowCount;
        // probably going to revisit this decision to skip indicating that labels have overflowed
        //  if (itemSaver->hasOverflowRow)
        //      rowCount += 1; // extra row to indicate overflow items are present
        }
    }
    return rowCount;
}


int decoratorGroupGetOverlayExtent(struct track *tg, char *itemName, int *start, int *end)
/* If itemName has overlay extents defined, populate start and end with those values and return 1.
 * If no overlay extents are defined for the item, return 0.
 */
{
if (tg && tg->decoratorGroup && tg->decoratorGroup->itemOverlayExtents)
    {
    struct startEndPair *s = hashFindVal(tg->decoratorGroup->itemOverlayExtents, itemName);
    if (s != NULL)
        {
        if (start != NULL)
            *start = s->start;
        if (end != NULL)
            *end = s->end;
        return 1;
        }
    }
return 0;
}


int decoratorGroupGetExtent(struct track *tg, char *itemName, int *start, int *end)
/* If itemName has extents defined, populate start and end with those values and return 1.
 * If no extents are defined for the item, return 0.
 */
{
if (tg && tg->decoratorGroup && tg->decoratorGroup->itemExtents)
    {
    struct startEndPair *s = hashFindVal(tg->decoratorGroup->itemExtents, itemName);
    if (s != NULL)
        {
        if (start != NULL)
            *start = s->start;
        if (end != NULL)
            *end = s->end;
        return 1;
        }
    }
return 0;
}


/***** Decoration draw code *****/


void addPoint(struct gfxPoly *p, struct point **stack, int x, int y, int middleY)
// Adds a point to a polygon (i.e. sequence of points), but also mirrors the point
// across the horizontal axis at middleY and pushes that mirrored point to the stack.
// The idea is that the points in the stack represent a mirrored return path.
// Useful for setting up to drawing exon/intron block structures as polygons.
{
gfxPolyAddPoint(p, x, y);
struct point *new;
AllocVar(new);
new->x = x;
new->y = 2*middleY-y; // (the middle plus the distance between y and the middle)
slAddHead(stack, new);
}


struct gfxPoly *decorationToPoly(struct decoration *d, double scale, int xOff, int y, int heightPer)
/* Convert a decoration into a polygon suitable for drawing.  This respects exons structure
 * and thickStart/thickEnd.
 */
{
int i, startX, endX, thickStartX, thickEndX, middleY = y+heightPer/2;
if (!scaledBoxToPixelCoords(d->chromStart, d->chromEnd, scale, xOff, &startX, &endX))
    return NULL;  // apparently we don't intersect the window
scaledBoxToPixelCoords(d->thickStart, d->thickEnd, scale, xOff, &thickStartX, &thickEndX);

struct gfxPoly *new = gfxPolyNew();
gfxPolyAddPoint(new, startX, middleY);

struct point *reverseList = NULL; // this is a stack to hold the path back to the start point

for (i=0; i<d->blockCount; i++)
    {
    int x1, x2;
    int exonStart = d->chromStart + d->chromStarts[i];
    int exonStop = d->chromStart + d->chromStarts[i] + d->blockSizes[i];
    if (!scaledBoxToPixelCoords(exonStart, exonStop, scale, xOff, &x1, &x2))
        continue;  // skip exons outside the draw window
    addPoint(new, &reverseList, x1, middleY, middleY);

    if ((exonStart < d->thickStart || exonStart >= d->thickEnd) || d->thickStart == d->thickEnd)
        {
        // start this exon at half height
        addPoint(new, &reverseList, x1, middleY-heightPer/4, middleY);
        }
    else
        {
        // start this exon at full height
        addPoint(new, &reverseList, x1, middleY-heightPer/2, middleY);
        }

    // check if thickStart happens in the middle of this exon
    if (d->thickStart != d->thickEnd && (d->thickStart > exonStart && d->thickStart < exonStop))
        {
        addPoint(new, &reverseList, thickStartX, middleY-heightPer/4, middleY);
        addPoint(new, &reverseList, thickStartX, middleY-heightPer/2, middleY);
        }

    // check if thickEnd happens in the middle of this exon
    if (d->thickStart != d->thickEnd && (d->thickEnd > exonStart && d->thickEnd < exonStop))
        {
        addPoint(new, &reverseList, thickEndX, middleY-heightPer/2, middleY);
        addPoint(new, &reverseList, thickEndX, middleY-heightPer/4, middleY);
        }

    // do the end of the exon - half height if we're not in the thick region
    if ((exonStop < d->thickStart || exonStop > d->thickEnd) || d->thickStart == d->thickEnd)
        { // half height exon
        addPoint(new, &reverseList, x2, middleY-heightPer/4, middleY);
        }
    else
        { // full height exon
        addPoint(new, &reverseList, x2, middleY-heightPer/2, middleY);
        }

    addPoint(new, &reverseList, x2, middleY, middleY);

    }
gfxPolyAddPoint(new, endX, y+heightPer/2);
// now loop reverselist to add the bottom half of the polygon as we return to startX

struct point *thisPt = reverseList;
while (thisPt != NULL)
    {
    gfxPolyAddPoint(new, thisPt->x, thisPt->y);
    thisPt = thisPt->next;
    }
slFreeList(&reverseList);
return new;
}


void drawBlockDecoration(struct decoration *d, struct hvGfx *hvg, double scale, int xOff, int y,
        int heightPer, MgFont *font, boolean filled)
/* Draw a block decoration as a polygon, either as a filled block or just as an outline. */
{
struct gfxPoly *shape = decorationToPoly(d, scale, xOff, y, heightPer);
if (shape == NULL)
    return;

if (filled)
    {
    Color fillBedColor = bedParseColor(d->fillColor);
    hvGfxDrawPoly(hvg, shape, bedColorToGfxColor(fillBedColor), TRUE);
    }
else
    {
    // draw the outline
    Color outlineBedColor = d->color;
    hvGfxDrawPoly(hvg, shape, bedColorToGfxColor(outlineBedColor), FALSE);
    }
}


void drawDecorationLabel(struct decoration *d, struct hvGfx *hvg, double scale, int xOff, int y,
        int heightPer, MgFont *font)
/* Draw a label for the decoration at the provided position. */
{
Color outlineBedColor = d->color;
int x1, x2;
if (scaledBoxToPixelCoords(d->chromStart, d->chromEnd, scale, xOff, &x1, &x2))
    {
    if (d->name && font)
        {
        char *shortLabel = cloneString(d->name);
        // maybe return to this idea for truncating labels by the size of the item they describe
        /*
        int w = x2-x1;
        if (w == 0) // when zoomed out, avoid shinking to nothing
            w = 1;

        // calculate how many characters we can squeeze into the box
        int charsInBox = (w-2) / mgFontCharWidth(font, 'm');
        if (charsInBox < strlen(d->name))
            if (charsInBox > 4)
                strcpy(shortLabel+charsInBox-3, "...");
        if (charsInBox < strlen(shortLabel))
            return;
        */
        struct rgbColor rgb = bedColorToRgb(outlineBedColor);
        Color labelColor = hvGfxFindColorIx(hvg, rgb.r, rgb.g, rgb.b); // labels should be opaque
        hvGfxText(hvg, x1, y, labelColor, font, shortLabel);
        }
    }
}


void drawOverflowLabel(int startBase, int endBase, struct hvGfx *hvg, double scale, int xOff, int y,
        int heightPer, MgFont *font)
/* Draw a composite decoration label indicating there are too many decorations to display
 * individual labels.  Not currently used, but might be brought back soon as we discuss handling
 * overflow situations.
 */
{
int x1, x2;
if (scaledBoxToPixelCoords(startBase, endBase, scale, xOff, &x1, &x2))
    {
    if (font)
        {
        char *shortLabel = cloneString("Too many decorations - zoom in to see more labels");
        int w = x2-x1;
        if (w == 0) // when zoomed out, avoid shinking to nothing
            w = 1;

        /* calculate how many characters we can squeeze into the box */
        int charsInBox = (w-2) / mgFontCharWidth(font, 'm');
        if (charsInBox < strlen(shortLabel))
            if (charsInBox > 4)
                strcpy(shortLabel+charsInBox-3, "...");
        if (charsInBox < strlen(shortLabel))
            return;

        hvGfxTextCentered(hvg, x1+1, y, w-2, heightPer, MG_BLACK, font, shortLabel);
        }
    }
}


void drawGlyphDecoration(struct decoration *d, struct hvGfx *hvg, double scale, int xOff, int y,
        int heightPer, boolean filled)
/* Draw a glyph decoration as a circle/polygon.  If filled, draw as with fillColor,
 * which may have transparency.
 */
{
glyphType glyph = parseGlyphType(d->glyph);
Color outlineColor = bedColorToGfxColor(d->color);
Color fillColor = bedColorToGfxColor(bedParseColor(d->fillColor));
drawScaledGlyph(hvg, d->chromStart, d->chromEnd, scale, xOff, y,
                 heightPer, glyph, filled, outlineColor, fillColor);

}


void drawDecorationMouseover(struct decoration *d, struct trackDb *decTdb, struct track *tg,
        void *item, struct hvGfx *hvg, double scale, int xOff, int y, int heightPer, boolean isOverlay)
/* Draw the mouseover block for a decoration.  For adjacent decorations, this is just mouseOver text.  For overlay
 * decorations, this also includes an onclick link to hgc or hgGene for the primary item (to match the onClick link
 * drawn for the decorated item.  Must be called before the decorated item's mouseOver code, as whichever mouseOver
 * is written to the page first has priority.
 */
{
if (isEmpty(d->mouseOver))
    return; // no point drawing a mouseover with no label; there will already be an hgc/hgGene clickthrough
            // for the item in place (and it will include extra extent if decorations go off the edge).

int startX, endX;

if (decorationGetStyle(d) == DECORATION_STYLE_BLOCK)
    {
    if (!scaledBoxToPixelCoords(d->chromStart, d->chromEnd, scale, xOff, &startX, &endX))
        {
        return;  // apparently we don't intersect the window
        }
    }
else if (decorationGetStyle(d) == DECORATION_STYLE_GLYPH)
    {
    int centeredStart, centeredEnd;
    centeredStart = (d->chromStart+d->chromEnd)/2;
    centeredEnd = (d->chromStart+d->chromEnd+1)/2;
    if (!scaledBoxToPixelCoords(centeredStart, centeredEnd, scale, xOff, &startX, &endX))
        return;  // apparently we don't intersect the window
    double middleX = (startX+endX)/2.0;
    startX = middleX - (heightPer/2.0);
    endX = middleX + (heightPer/2.0);
    }
else
    {
    errAbort ("Trying to draw mouseover for decoration that is neither block nor glyph");
    }

if (theImgBox && curImgTrack)
    {
    // copied from mapBoxHgcOrHgGene
    // This was about Tim's project for dragging the image bringing in content from
    // "off the screen", I think.  That work was never finished.
    #ifdef IMAGEv2_SHORT_MAPITEMS
    if (!revCmplDisp && startX < insideX && endX > insideX)
        startX = insideX;
    else if (revCmplDisp && startX < insideWidth && endX > insideWidth)
        endX = insideWidth - 1;
    #endif//def IMAGEv2_SHORT_MAPITEMS

    if (isOverlay)
        genericMapItem(tg, hvg, item, d->mouseOver, tg->mapItemName(tg, item), tg->itemStart(tg,item),
                tg->itemEnd(tg,item), startX, y, endX-startX, heightPer);
    else
        imgTrackAddMapItem(curImgTrack,"noLink",d->mouseOver, startX, y, endX, y+heightPer, NULL);
    }
else
    {
    hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" class=\"decoration\" title=\"%s\">\n", startX, y, endX, y+heightPer, d->mouseOver);
    }
}


void drawDecoration(struct decoration *d, struct hvGfx *hvg, double scale, int xOff, int y,
        int heightPer, MgFont *font, boolean filled)
/* Draw a decoration at the provided position.  If filled, draw a solid block/glyph in the
 * decoration's fillColor.  If not filled, draw the outline of the block/glyph in the
 * decoration's primary color.
 */
{
if (decorationGetStyle(d) == DECORATION_STYLE_BLOCK)
    drawBlockDecoration(d, hvg, scale, xOff, y, heightPer, font, filled);
if (decorationGetStyle(d) == DECORATION_STYLE_GLYPH)
    drawGlyphDecoration(d, hvg, scale, xOff, y, heightPer, filled);
}
