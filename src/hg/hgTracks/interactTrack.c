/* interactTrack -- draw interaction between two genomic regions */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hgTracks.h"
#include "bedCart.h"
#include "bigWarn.h"
#include "interact.h"
#include "interactUi.h"

static int interactTotalHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the interactions being displayed */
{
if ( tg->visibility == tvDense)
    return  tl.fontHeight;
int min, max, deflt, current; 
cartTdbFetchMinMaxPixels(cart, tg->tdb, 
                                INTERACT_MINHEIGHT, INTERACT_MAXHEIGHT, atoi(INTERACT_DEFHEIGHT),
                                &min, &max, &deflt, &current);
return tg->height = current;
}

static Color interactItemColor(struct track *tg, void *item, struct hvGfx *hvg, int scoreMin, int scoreMax)
/* Return color to draw an interaction */
{
struct interact *inter = item;
if (tg->colorShades)
    {
    struct bed *bed = (struct bed *)inter;
    adjustBedScoreGrayLevel(tg->tdb, bed, scoreMin, scoreMax);
    return tg->colorShades[grayInRange(inter->score, 0, 1000)];
    }
/*
 There must be a better way..., e.g.:

unsigned red = COLOR_32_RED(inter->color);
unsigned green = COLOR_32_GREEN(inter->color);
unsigned blue = COLOR_32_BLUE(inter->color);
*/
unsigned red = (inter->color & 0xff0000) >> 16;
unsigned green = (inter->color & 0xff00) >> 8;
unsigned blue = inter->color & 0xff;
return hvGfxFindColorIx(hvg, red, green, blue);
}

boolean interactSourceInWindow(struct interact *inter)
/* True if midpoint of source is on screen */
{
unsigned s = interactRegionCenter(inter->sourceStart, inter->sourceEnd);
return (s >= winStart) && (s < winEnd);
}

boolean interactTargetInWindow(struct interact *inter)
/* True if midpoint of target is on screen */
{
unsigned t = interactRegionCenter(inter->targetStart, inter->targetEnd);
return (t >= winStart) && (t < winEnd);
}

static void loadAndFilterItems(struct track *tg)
/* Load all interact items in region */
{
loadSimpleBedWithLoader(tg, (bedItemLoader)interactLoadAndValidate);

if (slCount(tg->items) == 0 && tg->limitedVisSet)
    {
    // too many items to display
    // borrowed behaviors in bamTrack and vcfTrack
    // TODO BRANEY: make this behavior generic for bigBeds
    // (bigBedSelectRange)
    tg->drawItems = bigDrawWarning;
    tg->networkErrMsg = "Too many items in display (zoom in)"; 
    tg->totalHeight = bigWarnTotalHeight;
    return;
    }

// filters
struct interact *inter, *next, *filteredItems = NULL;
int count = slCount(tg->items);

// exclude if missing endpoint(s) in window
char *endsVisible = cartUsualStringClosestToHome(cart, tg->tdb, FALSE,
                            INTERACT_ENDS_VISIBLE, INTERACT_ENDS_VISIBLE_DEFAULT);
for (inter = tg->items; inter; inter = next)
    {
    next = inter->next;
    if (differentString(endsVisible, INTERACT_ENDS_VISIBLE_ANY))
        {
        boolean sOnScreen = interactSourceInWindow(inter);
        boolean tOnScreen = interactTargetInWindow(inter);
        if (sameString(endsVisible, INTERACT_ENDS_VISIBLE_TWO))
            {
            if (!(sOnScreen && tOnScreen))
                continue;
            }
        if (sameString(endsVisible, INTERACT_ENDS_VISIBLE_ONE))
            {
            if (!(sOnScreen || tOnScreen))
                continue;
            }
        }
    slAddHead(&filteredItems, inter);
    }

slReverse(&filteredItems);
// consider sorting by score/value so highest scored items draw last (on top)
if (slCount(filteredItems) != count)
    labelTrackAsFiltered(tg);
tg->items = filteredItems;
}

void interactDrawLeftLabels(struct track *tg, int seqStart, int seqEnd,
    struct hvGfx *hvg, int xOff, int yOff, int width, int height,
    boolean withCenterLabels, MgFont *font,
    Color color, enum trackVisibility vis)
/* Override default */
{
}

void interactFreeItems(struct track *tg)
/* Free up interact items track */
{
interactFreeList((struct interact **)(&tg->items));
}

static boolean isBedMode(struct track *tg)
/* Pack and squish modes display using BED linked features code */
{
return tg->visibility == tvPack || tg->visibility == tvSquish;
}

void interactLoadItems(struct track *tg)
/* Load interact items in interact format */
{
loadAndFilterItems(tg);
if (isBedMode(tg))
    {
    /* convert to BEDs for linked feature display */
    struct interact *inters = tg->items, *inter;
    struct linkedFeatures *lfs = NULL, *lf;
    for (inter = inters; inter; inter = inter->next)
        {
        struct bed *bed = interactToBed(inter);
        lf = lfFromBed(bed);
        if (!tg->colorShades)
            {
            lf->extra = (void *)USE_ITEM_RGB;   /* signal for coloring */
            lf->filterColor = bed->itemRgb;
            }
        bedFree(&bed);
        // TODO: use lfFromBedExtra with scoreMin, scoreMax
        slAddHead(&lfs, lf);
        }
    slReverse(&lfs);
    tg->items = lfs;
    }
else
    {
    tg->mapsSelf = TRUE;
    tg->totalHeight = interactTotalHeight;
    tg->drawLeftLabels = interactDrawLeftLabels;
    tg->freeItems = interactFreeItems;
    }
}

char *interactMouseover(struct interact *inter, char *otherChrom)
/* Make mouseover text for an interaction */
{
struct dyString *ds = dyStringNew(0);
if (isEmptyTextField(inter->name))
    {
    if (!isEmptyTextField(inter->exp))
        dyStringPrintf(ds, "%s ", inter->exp);
    if (otherChrom)
        dyStringPrintf(ds, "%s", otherChrom);
    else
        {
        char buf[4096];
        sprintLongWithCommas(buf, inter->chromEnd - inter->chromStart);
        dyStringPrintf(ds, "%s bp", buf);
        }
    }
else
    dyStringPrintf(ds, "%s", inter->name);
if (inter->score)
    dyStringPrintf(ds, " %d", inter->score);
if (inter->value != 0.0)
    dyStringPrintf(ds, " %0.2f", inter->value);
return dyStringCannibalize(&ds);
}

int regionFootWidth(int start, int end, double scale)
/* Return half foot width in pixels */
{
    unsigned size = end - start;
    int width = scale * (double)size / 2;
    if (width == 0)
        width = 1;
    return width;
}

int interactSize(struct interact *inter)
/* Compute length of interaction (distance between middle of each region) in bp */
{
int sourceCenter = 0, targetCenter = 0;
interactRegionCenters(inter, &sourceCenter, &targetCenter);
return abs(targetCenter - sourceCenter);
}

int getX(int pos, int seqStart, double scale, int xOff)
/* Get x coordinate of a genomic location. Return -1 if off-screen */
{
if (pos < seqStart)
    return -1;
return ((double)(pos - seqStart + .5) * scale) + xOff;
}

struct interactTrackInfo {
    boolean isDirectional; // source and target are distinct item types
    char *offset;          // which end to draw offset (source or target)
    boolean drawUp;          // draw interactions with peak up (hill)
    boolean doOtherLabels;  // true to suppress labels on other chrom items (prevent overlap)
    int maxSize;        // longest interaction (midpoint to midpoint) in bp
    int fontHeight;
    int sameCount;      // number of same chromosome interactions in window
    int sameHeight;     // vertical space for same chromosome interactions
    int otherCount;     // number of other chromosome interactions in window
    int otherHeight;    // vertical space for other chromosome interactions
} interactTrackInfo;

struct interactTrackInfo *interactGetTrackInfo(struct track *tg, int seqStart, struct hvGfx *hvg,                                                       int xOff, MgFont *font, double scale)
/* Get layout info from interact items in window */
{
struct interactTrackInfo *tInfo = NULL;
AllocVar(tInfo);
tInfo->doOtherLabels = TRUE;
tInfo->isDirectional = interactUiDirectional(tg->tdb);
tInfo->offset = interactUiOffset(tg->tdb);
tInfo->drawUp = trackDbSettingClosestToHomeOn(tg->tdb, INTERACT_UP);

char *otherChrom = NULL;
int prevLabelEnd = 0, prevLabelStart = 0;
char *prevLabel = 0;
struct interact *inter;

for (inter = (struct interact *)tg->items; inter; inter = inter->next)
    {
    otherChrom = interactOtherChrom(inter);
    if (otherChrom == NULL)
        {
        tInfo->sameCount++;
        // determine maximum interaction size, for later use laying out 'peaks'
        int size = interactSize(inter);
        if (size > tInfo->maxSize)
            tInfo->maxSize = size;
        }
    else
        {
        tInfo->otherCount++;
        // suppress interchromosomal labels if they overlap
        if (!tInfo->doOtherLabels)
            continue;
        int labelWidth = vgGetFontStringWidth(hvg->vg, font, otherChrom);
        int x = getX(inter->chromStart, seqStart, scale, xOff);
        int labelStart = round((double)(x - labelWidth)/2);
        int labelEnd = labelStart + labelWidth - 1;
        if (labelStart <= prevLabelEnd && 
                !(labelStart == prevLabelStart && labelEnd == prevLabelEnd && 
                    sameString(otherChrom, prevLabel)))
            tInfo->doOtherLabels = FALSE;
        prevLabelStart = labelStart;
        prevLabelEnd = labelEnd;
        prevLabel = otherChrom;
        }
    }
tInfo->fontHeight = vgGetFontPixelHeight(hvg->vg, font);
tInfo->otherHeight = (tInfo->otherCount) ? 3 * tInfo->fontHeight : 0;
tInfo->sameHeight = (tInfo->sameCount) ? tg->height - tInfo->otherHeight : 0;
return tInfo;
}

static void setYOff(struct track *tg, int yOff)
/* Stash y offset for this track */
{
tg->customInt = yOff;
}

static int getYOff(struct track *tg)
/* Get y offset for this track (stashed by DrawItems) */
{
return tg->customInt;
}

static int flipY(struct track *tg, int y)
/* Invert y coordinate if flipped display is requested */
{
int yOff = getYOff(tg);
int flipped = yOff + tg->height + yOff - y;
return flipped;
}

static void drawFoot(struct track *tg, struct hvGfx *hvg, char *seq, int seqStart, int seqEnd, 
                        int x, int y, int width, Color color, boolean drawUp, struct hash *footHash)
/* Draw interaction end, 2 pixels high.  Force to black if it exactly overlaps another */
{
char buf[256];
safef(buf, sizeof(buf), "%s:%d-%d", seq, seqStart, seqEnd);
char *pos = cloneString(buf);
Color footColor = color;
if (hashLookup(footHash, pos))
    footColor = MG_BLACK;
else
    hashStore(footHash, pos);
if (drawUp)
    y = flipY(tg, y) - 2;
hvGfxBox(hvg, x, y, width, 2, footColor);
}

static void drawLine(struct track *tg, struct hvGfx *hvg, int x1, int y1, int x2, int y2, 
                        Color color, boolean isDashed, boolean drawUp)
/* Draw vertical or horizontal */
{
if (drawUp)
    {
    y1 = flipY(tg, y1);
    y2 = flipY(tg, y2);
    }
if (isDashed)
    hvGfxDottedLine(hvg, x1, y1, x2, y2, color, TRUE);
else
    hvGfxLine(hvg, x1, y1, x2, y2, color);
}

static void drawFootMapbox(struct track *tg, struct hvGfx *hvg, int start, int end, char *item, char *status,
                        int x, int y, int width, Color peakColor, Color highlightColor, boolean drawUp)
/* Draw grab box and add map box */
{
// Add var to identify endpoint ('foot'), or NULL if no name for endpoint */
char *clickArg = NULL;
if (!isEmptyTextField(item))
    {
    char buf[256];
    safef(buf, sizeof(buf),"foot=%s", cgiEncode(item));
    clickArg = cloneString(buf);
    }
char *itemBuf = isEmptyTextField(item) ? status : item;
if (drawUp)
    y = flipY(tg, y) - 3;
hvGfxBox(hvg, x-1, y, 3, 2, peakColor);
hvGfxBox(hvg, x, y, 1, 1, highlightColor);
mapBoxHgcOrHgGene(hvg, start, end, x - width, y, width * 2, 4,
                   tg->track, item, itemBuf, NULL, TRUE, clickArg);
}

void drawPeakMapbox(struct track *tg, struct hvGfx *hvg, int seqStart, int seqEnd, char *item, char *status,
                        int x, int y, Color peakColor, Color highlightColor, boolean drawUp)
/* Draw grab box and add map box */
{
if (drawUp)
    y = flipY(tg, y);
hvGfxBox(hvg, x-1, y-1, 3, 3, peakColor);
hvGfxBox(hvg, x, y, 1, 1, highlightColor);
mapBoxHgcOrHgGene(hvg, seqStart, seqEnd, x-1, y-1, 3, 3,
                   tg->track, item, status, NULL, TRUE, NULL);
}

static void drawInteractItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of interact structures. */
{
#define DRAW_LINE       0
#define DRAW_CURVE      1
#define DRAW_ELLIPSE    2

// Determine drawing mode
int draw = DRAW_LINE;
boolean doDashes = FALSE;
if (vis != tvDense)
    {
    char *drawMode = cartUsualStringClosestToHome(cart, tg->tdb, FALSE,
                                INTERACT_DRAW, INTERACT_DRAW_DEFAULT);
    if (sameString(drawMode, INTERACT_DRAW_CURVE))
        draw = DRAW_CURVE;
    else if (sameString(drawMode, INTERACT_DRAW_ELLIPSE))
    draw = DRAW_ELLIPSE;
doDashes = cartUsualBooleanClosestToHome(cart, tg->tdb, FALSE,
                                INTERACT_DIRECTION_DASHES, INTERACT_DIRECTION_DASHES_DEFAULT);
    }
double scale = scaleForWindow(width, seqStart, seqEnd);
struct interact *inter = NULL;
char buffer[1024];
char itemBuf[2048];

// Gather info for layout
struct interactTrackInfo *tInfo = interactGetTrackInfo(tg, seqStart, hvg, xOff, font, scale);
setYOff(tg, yOff);      // TODO: better to stash this in tInfo, and save that in track struct */
int highlightColor = MG_WHITE;
boolean drawUp = trackDbSettingClosestToHomeOn(tg->tdb, INTERACT_UP) && vis == tvFull;

// Get spectrum range
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tg->tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tg->tdb, "scoreMax", "1000"));

// Draw items
struct hash *footHash = hashNew(0);     // track feet so we can override color to black for overlapping
struct hash *footHashOther = hashNew(0);  // has for items on other chrom
for (inter = (struct interact *)tg->items; inter; inter = inter->next)
    {
    char *otherChrom = interactOtherChrom(inter);
    safef(itemBuf, sizeof itemBuf, "%s", inter->name);
    char *statusBuf = interactMouseover(inter, otherChrom);

    // Pick colors

    #define MG_LIGHT_MAGENTA    0xffffbbff
    #define MG_LIGHT_GRAY         0xff909090
    color = interactItemColor(tg, inter, hvg, scoreMin, scoreMax);
    if (vis == tvDense && otherChrom && color == MG_BLACK)
        // use highlight color for other chrom items in dense mode
        color = MG_LIGHT_MAGENTA;
    int peakColor = (color == MG_BLACK || tg->colorShades) ? MG_LIGHT_MAGENTA : MG_LIGHT_GRAY;

    
    if (otherChrom)
        {
        // different chromosomes
        //      draw below same chrom items, if any
        int height = 0;
        int yOffOther = yOff;
        if (vis == tvDense)
            {
            height = tg->height;
            }
        else
            {
            height = tInfo->otherHeight/2;
            yOffOther = yOff + tInfo->sameHeight;
            }
        unsigned r = interactRegionCenter(inter->chromStart, inter->chromEnd);
        int x = getX(r, seqStart, scale, xOff); 
        int footWidth = regionFootWidth(inter->chromStart, inter->chromEnd, scale);
        unsigned yPos = yOffOther + height;

        // draw the foot (2 pixels high)
        drawFoot(tg, hvg, inter->chrom, inter->chromStart, inter->chromEnd, 
                x - footWidth, yOffOther, footWidth + footWidth + 1, color, drawUp, footHashOther);

        // draw the vertical
        boolean isReversed = tInfo->isDirectional && differentString(inter->chrom, inter->sourceChrom);
        drawLine(tg, hvg, x, yOffOther, x, yPos, color, isReversed && doDashes, drawUp);
        
        if (vis == tvDense)
            continue;

        // add map box to foot

        char *nameBuf = (inter->chromStart == inter->sourceStart ?      
                        inter->sourceName : inter->targetName);
        drawFootMapbox(tg, hvg, inter->chromStart, inter->chromEnd, nameBuf, statusBuf, 
                        x - footWidth, yOffOther, footWidth, peakColor, highlightColor, drawUp);

        // add map box to vertical
        mapBoxHgcOrHgGene(hvg, inter->chromStart, inter->chromEnd, x - 2, yOffOther, 4, 
                            height, tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
        if (tInfo->doOtherLabels)
            {
            // draw label
            safef(buffer, sizeof buffer, "%s", sameString(inter->chrom, inter->sourceChrom) ?
                                        inter->targetChrom : inter->sourceChrom);
            if (drawUp)
                yPos = flipY(tg, yPos);
            hvGfxTextCentered(hvg, x, yPos + 2, 4, 4, MG_BLUE, font, buffer);
            int labelWidth = vgGetFontStringWidth(hvg->vg, font, buffer);

            // add map box to label
            mapBoxHgcOrHgGene(hvg, inter->chromStart, inter->chromEnd, x - labelWidth/2, 
                    yPos, labelWidth, tInfo->fontHeight, tg->track, itemBuf, statusBuf, 
                    NULL, TRUE, NULL);
            }
        continue;
        }

    // Draw same chromosome interaction

    // source region
    unsigned s = interactRegionCenter(inter->sourceStart, inter->sourceEnd);
    int sX = getX(s, seqStart, scale, xOff); 
    int sWidth = regionFootWidth(inter->sourceStart, inter->sourceEnd, scale);
    boolean sOnScreen = (s >= seqStart) && (s< seqEnd);

    // target region
    unsigned t = interactRegionCenter(inter->targetStart, inter->targetEnd);
    int tX = getX(t, seqStart, scale, xOff);
    int tWidth = regionFootWidth(inter->targetStart,inter->targetEnd, scale);
    boolean tOnScreen = (t >= seqStart) && (t< seqEnd);

    boolean isReversed = (tInfo->isDirectional && t < s);
    int interSize = abs(t - s);
    int peakHeight = (tInfo->sameHeight - 15) * ((double)interSize / tInfo->maxSize) + 10;
    int peak = yOff + peakHeight;
    if (vis == tvDense)
        peak = yOff + tg->height;

    // NOTE: until time permits, force to rectangle when in reversed strand mode.

    int yTarget = yOff;
    int ySource = yOff;
    if (tInfo->offset && draw != DRAW_ELLIPSE)
        // ellipse code doesn't support assymetrical ends
        {
        int yOffset = tg->height/10 + 1;
        if (sameString(tInfo->offset, INTERACT_OFFSET_TARGET))
            yTarget = yOff + yOffset;
        else if (sameString(tInfo->offset, INTERACT_OFFSET_SOURCE))
            ySource = yOff + yOffset;
        }
    unsigned footColor = color;

    if (sOnScreen)
        {
        drawFoot(tg, hvg, inter->sourceChrom, inter->sourceStart, inter->sourceEnd,
                            sX - sWidth, ySource, sWidth + sWidth + 1, footColor, drawUp, footHash);
        if (vis == tvDense || !tOnScreen || draw == DRAW_LINE || hvg->rc)
            {
            // draw vertical from foot to peak
            drawLine(tg, hvg, sX, ySource, sX, peak, color, isReversed && doDashes, drawUp);
            }
        }
    if (tOnScreen)
        {
        drawFoot(tg, hvg, inter->targetChrom, inter->targetStart, inter->targetEnd,
                            tX - tWidth, yTarget, tWidth + tWidth + 1, footColor, drawUp, footHash);
        if (vis == tvDense || !sOnScreen || draw == DRAW_LINE || hvg->rc)
            {
            // draw vertical from foot to peak
            drawLine(tg, hvg, tX, yTarget, tX, peak, color, isReversed && doDashes, drawUp);
            }
        }
    if (vis == tvDense)
        continue;

    // Full mode: add map boxes and draw interaction

    if (sOnScreen)
        {
        // draw grab box and map box to source region
        drawFootMapbox(tg, hvg, inter->chromStart, inter->chromEnd, inter->sourceName, 
                            statusBuf, sX, ySource, sWidth, peakColor, highlightColor, drawUp);
        }
    if (tOnScreen)
        {
        // draw grab box and add map box to target region
        drawFootMapbox(tg, hvg, inter->chromStart, inter->chromEnd, inter->targetName, 
                        statusBuf, tX, yTarget, tWidth, 
                        tInfo->isDirectional ? MG_MAGENTA : peakColor, highlightColor, drawUp);
        }
    if ((s < seqStart && t < seqStart) || (s > seqEnd && t > seqEnd))
        continue;

    // Draw interaction and map boxes
    int lowerX = 0, upperX = 0;
    if (s < t)
        {
        lowerX = sOnScreen ? sX : xOff;
        upperX = tOnScreen ? tX : xOff + width;
        }
    else
        {
        lowerX = tOnScreen ? tX : xOff;
        upperX = sOnScreen ? sX : xOff + width;
        }
    if (draw == DRAW_LINE || !sOnScreen || !tOnScreen || hvg->rc)
        {
        // draw horizontal line between region centers at 'peak' height
        drawLine(tg, hvg, lowerX, peak, upperX, peak, color, isReversed && doDashes, drawUp);

        // draw grab box and map box on mid-point of horizontal line
        int xMap = lowerX + (double)(upperX-lowerX)/2;
        drawPeakMapbox(tg, hvg, inter->chromStart, inter->chromEnd, itemBuf, statusBuf,
                            xMap, peak, peakColor, highlightColor, drawUp);
        continue;
        }
    // Draw curves
    if (draw == DRAW_CURVE)
        {
        int peakX = ((upperX - lowerX + 1) / 2) + lowerX;
        int fudge = 30;
        int peakY = peak + fudge; // admittedly a hack (obscure how to define ypeak of curve)
        int y1 = isReversed ? yTarget : ySource;
        int y2 = isReversed ? ySource : yTarget;
        if (drawUp)
            {
            y1 = flipY(tg, y1);
            y2 = flipY(tg, y2);
            peakY = flipY(tg, peakY);
            }
        int maxY = hvGfxCurve(hvg, lowerX, y1, peakX, peakY, upperX, y2, color, isReversed && doDashes);
        // curve drawer does not use peakY as expected, so it returns actual max Y used
        // draw grab box and map box on peak
        if (drawUp)
            maxY = (maxY - peakY)/2 + tg->customInt;
        drawPeakMapbox(tg, hvg, inter->chromStart, inter->chromEnd, inter->name, statusBuf,
                            peakX, maxY, peakColor, highlightColor, drawUp);
        }
    else if (draw == DRAW_ELLIPSE)
        {
        // can not support offsets
        int yLeft = yOff + peakHeight;
        int yTop = yOff - peakHeight;
        int ellipseOrient = ELLIPSE_BOTTOM;
        if (drawUp)
            {
            ellipseOrient = ELLIPSE_TOP;
            yLeft = yOff + tg->height - peakHeight;
            yTop = yOff + tg->height + peakHeight;
            }
        hvGfxEllipseDraw(hvg, lowerX, yLeft, upperX, yTop, color, ellipseOrient,
                                isReversed && doDashes);
        // draw grab box and map box on peak
        int maxY = peakHeight + yOff;
        int peakX = ((upperX - lowerX + 1) / 2) + lowerX;
        drawPeakMapbox(tg, hvg, inter->chromStart, inter->chromEnd, inter->name, statusBuf,
                            peakX, maxY, peakColor, highlightColor, drawUp);
        }
    }
}

void interactDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of interact structures. */
{
if (isBedMode(tg))
    {
    tg->drawItemAt = linkedFeaturesDrawAt;
    linkedFeaturesDraw(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
    }
else
    drawInteractItems(tg, seqStart, seqEnd, hvg, xOff, yOff, width, font, color, vis);
}

void interactMethods(struct track *tg)
/* Interact track type methods */
{
tg->bedSize = 12;
linkedFeaturesMethods(tg);         // for pack and squish modes 
tg->loadItems = interactLoadItems;
tg->drawItems = interactDrawItems;
}

void interactCtMethods(struct track *tg)
/* Interact track methods for custom track */
{
interactMethods(tg);
}

