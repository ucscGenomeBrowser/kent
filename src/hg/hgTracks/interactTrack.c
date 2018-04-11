/* interactTrack -- draw interaction between two genomic regions */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hgTracks.h"
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

void interactLoadItems(struct track *tg)
/* Load all interact items in region */
{
loadSimpleBedWithLoader(tg, (bedItemLoader)interactLoad);

if (tg->limitedVisSet)
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

// filter on score
char buf[1024];
safef(buf, sizeof buf, "%s.%s", tg->tdb->track, INTERACT_MINSCORE);
int minScore = cartUsualInt(cart, buf, 0);
struct interact *inter, *next, *filteredItems = NULL;
int count = slCount(tg->items);
for (inter = tg->items; inter; inter = next)
    {
    next = inter->next;
    if (inter->score < minScore)
        continue;
    slAddHead(&filteredItems, inter);
    }

slReverse(&filteredItems);
// consider sorting by score/value so highest scored items draw last (on top)
if (slCount(filteredItems) != count)
    labelTrackAsFiltered(tg);
tg->items = filteredItems;
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

void interactRegionCenters(struct interact *inter, int *sourceCenter, int *targetCenter)
/* Return genomic position of endpoint centers */
{
assert(sourceCenter);
assert(targetCenter);
*sourceCenter = interactRegionCenter(inter->sourceStart, inter->sourceEnd);
*targetCenter = interactRegionCenter(inter->targetStart, inter->targetEnd);
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
    boolean isDirectional;
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

static void interactDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of interact structures. */
{
#define DRAW_LINE       0
#define DRAW_CURVE      1
#define DRAW_ELLIPSE    2

// Determine drawing mode
int draw = DRAW_LINE;
if (vis != tvDense)
    {
    char *drawMode = cartUsualStringClosestToHome(cart, tg->tdb, FALSE,
                                INTERACT_DRAW, INTERACT_DRAW_DEFAULT);
    if (sameString(drawMode, INTERACT_DRAW_CURVE))
        draw = DRAW_CURVE;
    else if (sameString(drawMode, INTERACT_DRAW_ELLIPSE))
        draw = DRAW_ELLIPSE;
    }

double scale = scaleForWindow(width, seqStart, seqEnd);
struct interact *inter = NULL;
char buffer[1024];
char itemBuf[2048];

// Gather info for layout
struct interactTrackInfo *tInfo = interactGetTrackInfo(tg, seqStart, hvg, xOff, font, scale);

// Get spectrum range
int scoreMin = atoi(trackDbSettingClosestToHomeOrDefault(tg->tdb, "scoreMin", "0"));
int scoreMax = atoi(trackDbSettingClosestToHomeOrDefault(tg->tdb, "scoreMax", "1000"));

// Draw items
for (inter = (struct interact *)tg->items; inter; inter = inter->next)
    {
    char *otherChrom = interactOtherChrom(inter);
    safef(itemBuf, sizeof itemBuf, "%s", inter->name);
    char *statusBuf = interactMouseover(inter, otherChrom);

    // Pick colors

    color = interactItemColor(tg, inter, hvg, scoreMin, scoreMax);
    if (vis == tvDense && otherChrom && color == MG_BLACK)
        // use highlight color for other chrom items in dense mode
        color = MG_MAGENTA;
    int peakColor = (color == MG_BLACK || tg->colorShades) ? MG_MAGENTA : MG_GRAY;
    
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

        // draw the foot
        hvGfxLine(hvg, x - footWidth, yOffOther, x + footWidth, yOffOther, color);

        // draw the vertical
        if (tInfo->isDirectional && differentString(inter->chrom, inter->sourceChrom))
            hvGfxDottedLine(hvg, x, yOffOther, x, yPos, color, TRUE);
        else
            hvGfxLine(hvg, x, yOffOther, x, yPos, color);
        
        if (vis == tvDense)
            continue;

        // add map box to foot
        char *nameBuf = (inter->chromStart == inter->sourceStart ?      
                        inter->sourceName : inter->targetName);
        if (isEmptyTextField(nameBuf))
            nameBuf = statusBuf;
        int chromStart = inter->chromStart;
        int chromEnd = inter->chromEnd;
        mapBoxHgcOrHgGene(hvg, chromStart, chromEnd,
                        x - footWidth, yOffOther, footWidth * 2, 4,
                        tg->track, itemBuf, nameBuf, NULL, TRUE, NULL);

        // add map box to vertical
        mapBoxHgcOrHgGene(hvg, chromStart, chromEnd, x - 2, yOffOther, 4, 
                            height, tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
        if (tInfo->doOtherLabels)
            {
            // draw label
            safef(buffer, sizeof buffer, "%s", sameString(inter->chrom, inter->sourceChrom) ?
                                        inter->targetChrom : inter->sourceChrom);
            hvGfxTextCentered(hvg, x, yPos + 2, 4, 4, MG_BLUE, font, buffer);
            int labelWidth = vgGetFontStringWidth(hvg->vg, font, buffer);

            // add map box to label
            mapBoxHgcOrHgGene(hvg, chromStart, chromEnd, x - labelWidth/2, 
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

    if (sOnScreen)
        {
        // draw foot of source region
        hvGfxLine(hvg, sX - sWidth, yOff, sX + sWidth, yOff, color);
        if (vis == tvDense || !tOnScreen || draw == DRAW_LINE)
            {
            // draw vertical
            if (isReversed)
                hvGfxDottedLine(hvg, sX, yOff, sX, peak, color, TRUE);
            else
                hvGfxLine(hvg, sX, yOff, sX, peak, color);
            }
        }
    if (tOnScreen)
        {
if (sOnScreen)
    //warn("interaction: %s", inter->name);
        // draw foot of target region
        hvGfxLine(hvg, tX - tWidth, yOff, tX + tWidth, yOff, color);
        if (vis == tvDense || !sOnScreen || draw == DRAW_LINE)
            {
            // draw vertical
            if (isReversed)
                hvGfxDottedLine(hvg, tX, yOff, tX, peak, color, TRUE);
            else
                hvGfxLine(hvg, tX, yOff, tX, peak, color);
            }
        }
    if (vis == tvDense)
        continue;

    // Full mode: add map boxes and draw interaction
    int chromStart = inter->chromStart;
    int chromEnd = inter->chromEnd;
    char *nameBuf = NULL;
    if (sOnScreen)
        {
        // add map box to source region
        nameBuf = isEmptyTextField(inter->sourceName) ? statusBuf : inter->sourceName;
        hvGfxBox(hvg, sX-1, yOff, 3, 1, peakColor);
        hvGfxBox(hvg, sX, yOff, 1, 1, MG_WHITE);
        mapBoxHgcOrHgGene(hvg, chromStart, chromEnd, 
                           sX - sWidth, yOff, sWidth * 2, 3,
                           tg->track, itemBuf, nameBuf, NULL, TRUE, NULL);
        }
    if (tOnScreen)
        {
        // add map box to target region
        nameBuf = isEmptyTextField(inter->targetName) ? statusBuf : inter->targetName;
        hvGfxBox(hvg, tX-1, yOff, 3, 1, peakColor);
        hvGfxBox(hvg, tX, yOff, 1, 1, MG_WHITE);
        mapBoxHgcOrHgGene(hvg, chromStart, chromEnd, 
                        tX - tWidth, yOff, tWidth * 2, 3,
                        tg->track, itemBuf, nameBuf, NULL, TRUE, NULL);
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
    if (draw == DRAW_LINE || !sOnScreen || !tOnScreen)
        {
        // draw horizontal line between region centers at 'peak' height
        if (isReversed)
            hvGfxDottedLine(hvg, lowerX, peak, upperX, peak, color, TRUE);
        else
            hvGfxLine(hvg, lowerX, peak, upperX, peak, color);

        // map box on mid-point of horizontal line
        int xMap = lowerX + (double)(upperX-lowerX)/2;
        int yMap = peak-1;
        hvGfxBox(hvg, xMap, peak-1, 1, 3, peakColor);
        hvGfxBox(hvg, xMap, peak, 1, 1, MG_WHITE);
        mapBoxHgcOrHgGene(hvg, chromStart, chromEnd, xMap-1, yMap, 3, 3,
                           tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
        continue;
        }
    // Draw curves
    if (draw == DRAW_CURVE)
        {
        int peakX = ((upperX - lowerX + 1) / 2) + lowerX;
        int peakY = peak + 30; // admittedly a hack (obscure how to define ypeak of curve)
        int maxY = hvGfxCurve(hvg, lowerX, yOff, peakX, peakY, upperX, yOff, color, isReversed);
        // curve drawer does not use peakY as expected, so it returns actual max Y used
        // draw map box on peak
        hvGfxBox(hvg, peakX-1, maxY, 3, 1, peakColor);
        hvGfxBox(hvg, peakX, maxY, 1, 1, MG_WHITE);
        mapBoxHgcOrHgGene(hvg, chromStart, chromEnd, peakX, maxY, 3, 1,
                       tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
        }
    else if (draw == DRAW_ELLIPSE)
        {
        int yLeft = yOff + peakHeight;
        int yTop = yOff - peakHeight;
//warn("hgTracks ellipse: left point: (%d,%d), top point: (%d,%d)", lowerX, yLeft, upperX, yTop);
        hvGfxEllipseDraw(hvg, lowerX, yLeft, upperX, yTop, color, ELLIPSE_BOTTOM, isReversed);
        // draw map box on peak
        int maxY = peakHeight + yOff;
        int peakX = ((upperX - lowerX + 1) / 2) + lowerX;
        hvGfxBox(hvg, peakX-1, maxY, 3, 1, peakColor);
        hvGfxBox(hvg, peakX, maxY, 1, 1, MG_WHITE);
        mapBoxHgcOrHgGene(hvg, chromStart, chromEnd, peakX, maxY, 3, 1,
                       tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
        }
    }
}

void interactDrawLeftLabels(struct track *tg, int seqStart, int seqEnd,
    struct hvGfx *hvg, int xOff, int yOff, int width, int height,
    boolean withCenterLabels, MgFont *font,
    Color color, enum trackVisibility vis)
/* Override default */
{
}

void interactMethods(struct track *tg)
/* Interact track type methods */
{
tg->loadItems = interactLoadItems;
tg->drawItems = interactDrawItems;
tg->drawLeftLabels = interactDrawLeftLabels;
tg->totalHeight = interactTotalHeight;
tg->mapsSelf = TRUE;
}

void interactCtMethods(struct track *tg)
/* Interact track methods for custom track */
{
interactMethods(tg);
}

