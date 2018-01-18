/* interactTrack -- draw interaction between two genomic regions */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hgTracks.h"
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
//uglyf("IN totalHeight=%d. ", current);
// FIXME DEBUG
//return 300;
return tg->height = current;
}

static Color interactItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw an interaction */
{
struct interact *inter = item;
struct rgbColor itemRgb;
// There must be a better way...
itemRgb.r = (inter->color & 0xff0000) >> 16;
itemRgb.g = (inter->color & 0xff00) >> 8;
itemRgb.b = inter->color & 0xff;
//uglyf("IN color=%X", hvGfxFindColorIx(hvg, itemRgb.r, itemRgb.g, itemRgb.b));
return hvGfxFindColorIx(hvg, itemRgb.r, itemRgb.g, itemRgb.b);
}

void interactLoadItems(struct track *tg)
/* Load all interact items in region */
{
loadSimpleBedWithLoader(tg, (bedItemLoader)interactLoad);
}

static void interactDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of interact structures. */
{
/* Choose drawing style by cart var (at least for now) */
#define DRAW_LINE       0
#define DRAW_CURVE      1
#define DRAW_ELLIPSE    2
char *drawMode = cartUsualString(cart, "interaction", "line");
int draw  = DRAW_LINE;
if (sameString(drawMode, "curve"))
    draw = DRAW_CURVE;
else if (sameString(drawMode, "ellipse"))
    draw = DRAW_ELLIPSE;

double scale = scaleForWindow(width, seqStart, seqEnd);
struct interact *inters = tg->items;
//uglyf("Found %d items. ", slCount(inters));
unsigned int maxWidth = 0;
struct interact *inter;
char buffer[1024];
char itemBuf[2048];
safef(buffer, sizeof buffer, "%s.%s", tg->tdb->track, INTERACT_MINSCORE);
//double minScore = sqlDouble(cartUsualString(cart, buffer, INTERACT_DEFMINSCORE));

// Determine if there are mixed inter and intra-chromosomal 
// Suppress interchromosomal labels if they overlap
int nSame = 0, nOther = 0;
int prevLabelEnd = 0, prevLabelStart = 0;
char *prevLabel = 0;
boolean doOtherLabels = TRUE;
for (inter=inters; inter; inter=inter->next)
    {
    int width = inter->chromEnd - inter->chromStart;
    if (width > maxWidth)
        maxWidth = width;
    }
//uglyf("Max width is %d. ", maxWidth);
for (inter=inters; inter; inter=inter->next)
    {
    if (sameString(inter->chrom1, inter->chrom2))
        nSame++;
    else
        {
        nOther++;
        if (!doOtherLabels)
            continue;
        int labelWidth = vgGetFontStringWidth(hvg->vg, font, inter->chrom2);
        // TODO: simplify now that center approach is abandoned
        int s = (inter->chromEnd1 - inter->chromStart1 + .5) / 2;
        int sx = ((s - seqStart) + .5) * scale + xOff; // x coord of center
        int labelStart = sx - labelWidth/2;
        int labelEnd = labelStart + labelWidth - 1;
        if (labelStart <= prevLabelEnd && 
                !(labelStart == prevLabelStart && labelEnd == prevLabelEnd && 
                    sameString(inter->chrom1, prevLabel)))
            doOtherLabels = FALSE;
        prevLabelStart = labelStart;
        prevLabelEnd = labelEnd;
        prevLabel = inter->chrom2;
        }
    }
int fontHeight = vgGetFontPixelHeight(hvg->vg, font);
int otherHeight = (nOther) ? 3 * fontHeight : 0;
int sameHeight = (nSame) ? tg->height - otherHeight: 0;

// Draw items
//uglyf("IN seqStart=%d", seqStart);
for (inter=inters; inter; inter=inter->next)
    {
    safef(itemBuf, sizeof itemBuf, "%s", inter->name);
    struct dyString *ds = dyStringNew(0);
    dyStringPrintf(ds, "%s", inter->name);
    if (inter->score)
        dyStringPrintf(ds, " %d", inter->score);
    char *statusBuf = dyStringCannibalize(&ds);
//uglyf("statusBuf: %s. ", statusBuf);

    color = interactItemColor(tg, inter, hvg);
    
    // TODO: simplify by using start/end instead of center and width
    // This is a holdover from longRange track implementation

    // set lower and upper regions from item1 and item2, which may be ordered by type rather
    // than genomic position (e.g. item1 are snp's, item2 are genes)
    unsigned lowStart, lowEnd, highStart, highEnd;
    if (inter->chromStart1 < inter->chromStart2)
        {
        lowStart = inter->chromStart1;
        lowEnd = inter->chromEnd1;
        highStart = inter->chromStart2;
        highEnd = inter->chromEnd2;
        }
    else
        {
        lowStart = inter->chromStart2;
        lowEnd = inter->chromEnd2;
        highStart = inter->chromStart1;
        highEnd = inter->chromEnd1;
        }
    unsigned s = lowStart + ((double)(lowEnd - lowStart + .5) / 2);
    int sx = ((s - seqStart) + .5) * scale + xOff; // x coord of center (lower region)
//uglyf("<br>IN seqStart=%d, start=%d, start1=%d, end1=%d, start2=%d, end2=%d, end=%d, s=%d, sx=%d. ", 
        //seqStart, inter->chromStart, inter->chromStart1, inter->chromEnd1, 
        //inter->chromStart2, inter->chromEnd2, inter->chromEnd, s, sx);
    unsigned sw = lowEnd - lowStart;
    int sFootWidth = scale * (double)sw / 2;       // width in pixels of half foot (lower)
    if (sFootWidth == 0)
        sFootWidth = 1;

    unsigned e = highStart + (double)(highEnd - highStart + .5) / 2;
    int ex = ((e - seqStart) + .5) * scale + xOff;
    unsigned ew = highEnd - highStart;
    int eFootWidth = scale * (double)ew / 2;
    if (eFootWidth == 0)
        eFootWidth = 1;
//uglyf("<br>IN s=%d, sx=%d, sw=%d, sFootWidth=%d.   e=%d, ex=%d, ew=%d, eFootWidth=%d, chromStart=%d",
                //s, sx, sw, sFootWidth, e, ex, ew, eFootWidth, inter->chromStart);
//uglyf("<br>&nbsp;&nbsp;IN start1=%d, end1=%d, start2=%d, end2=%d.",
                //inter->chromStart1, inter->chromEnd1, inter->chromStart2, inter->chromEnd2);

    if (differentString(inter->chrom1, inter->chrom2))
        {
        // different chromosomes
        //      draw below same chrom items, if any
        unsigned yPos = 0;
        int height = 0;
        int yOffOther = yOff;
        if (tg->visibility == tvDense)
            {
            height = tg->height;
            }
        else
            {
            height = otherHeight/2;
            yOffOther = yOff + sameHeight;
            }
        yPos = yOffOther + height;

        // draw the foot
        int footWidth = sFootWidth;
        hvGfxLine(hvg, sx - footWidth, yOffOther, sx + footWidth, yOffOther, color);

        // draw the vertical
        if (inter->strand[0] == '+')
            hvGfxLine(hvg, sx, yOffOther, sx, yPos, color);
        else
            hvGfxDottedLine(hvg, sx, yOffOther, sx, yPos, color);
        if (tg->visibility == tvFull)
            {
            mapBoxHgcOrHgGene(hvg, s, s, sx - 2, yOffOther, 4, height,
                                   tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);

            safef(buffer, sizeof buffer, "%s", inter->chrom2);
            if (doOtherLabels)
                {
                hvGfxTextCentered(hvg, sx, yPos + 2, 4, 4, MG_BLUE, font, buffer);
                int width = vgGetFontStringWidth(hvg->vg, font, buffer);
                mapBoxHgcOrHgGene(hvg, s, s, sx - width/2, yPos, 
                                width, fontHeight, tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
                }
            }
        continue;
        }

    // draw same chromosome interaction
    boolean sOnScreen = (s >= seqStart) && (s < seqEnd);
    boolean eOnScreen = (e >= seqStart) && (e < seqEnd);
//uglyf("<br>IN s=%d, e=%d, sOn: %d, eOn: %d. ", s, e, sOnScreen, eOnScreen);
//if (s>e)
    //uglyf("<br>   IN s>e start=%d, start1=%d, end1=%d, start2=%d, end2=%d, end=%d, s=%d, sx=%d. ", 
        //inter->chromStart, inter->chromStart1, inter->chromEnd1, 
        //inter->chromStart2, inter->chromEnd2, inter->chromEnd, s, sx);

    double interWidth = e - s;
    int peakHeight = (sameHeight - 15) * ((double)interWidth / maxWidth) + 10;
    int peak = yOff + peakHeight;
    if (tg->visibility == tvDense)
        peak = yOff + tg->height;

    if (sOnScreen)
        {
        // draw foot of first region and mapbox
        hvGfxLine(hvg, sx - sFootWidth, yOff, sx + sFootWidth, yOff, color);
        mapBoxHgcOrHgGene(hvg, inter->chromStart, inter->chromEnd, 
                            sx - sFootWidth - 2, yOff, sx + sFootWidth + 2, 4,
                            tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
        // draw vertical
        if (!eOnScreen || draw == DRAW_LINE)
            {
            if (inter->strand[0] == '-')
                hvGfxDottedLine(hvg, sx, yOff, sx, peak, color);
            else
                hvGfxLine(hvg, sx, yOff, sx, peak, color);
            }
        }
    if (eOnScreen)
        {
        // draw foot and mapbox of second region
        hvGfxLine(hvg, ex - eFootWidth, yOff, ex + eFootWidth, yOff, color);
        mapBoxHgcOrHgGene(hvg, inter->chromStart, inter->chromEnd, 
                            ex - eFootWidth - 2, yOff, ex + eFootWidth + 2, 4,
                            tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);

        // draw vertical
        if (!sOnScreen || draw == DRAW_LINE)
            {
            if (inter->strand[0] == '-')
                hvGfxDottedLine(hvg, ex, yOff, ex, peak, color); //OLD
            else
                hvGfxLine(hvg, ex, yOff, ex, peak, color); //OLD
            }
        }
    if (tg->visibility == tvFull)
        {
        if (sOnScreen && eOnScreen && draw != DRAW_LINE)
            {
            boolean isDotted = (inter->strand[0] == '-');
            if (draw == DRAW_CURVE)
                {
                int peakX = ((ex - sx + 1) / 2) + sx;
                int peakY = peak + 60;
                hvGfxCurve(hvg, sx, yOff, peakX, peakY, ex, yOff, color, isDotted);
                // map box on peak
                // FIXME: not working
                /*mapBoxHgcOrHgGene(hvg, inter->chromStart, inter->chromEnd,
                                    peakX - 2, peakY - 2, 4, 4,
                                    tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
*/
                }
            else if (draw == DRAW_ELLIPSE)
                {
                int yLeft = yOff + peakHeight;
                int yTop = yOff - peakHeight;
                hvGfxEllipseDraw(hvg, sx, yLeft, ex, yTop, color, ELLIPSE_BOTTOM, isDotted);
                // map box on peak
                // FIXME: not working
                /*mapBoxHgcOrHgGene(hvg, inter->chromStart, inter->chromEnd,
                                    sx - sFootWidth - 2, yOff + peakHeight, 4, 4,
                                    tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
*/
                }
            }
        else
            {
            // draw link horizontal line between regions (dense mode just shows feet ??)
            unsigned ePeak = eOnScreen ? ex : xOff + width;
            unsigned sPeak = sOnScreen ? sx : xOff;
            if (inter->strand[0] == '-')
                hvGfxDottedLine(hvg, sPeak, peak, ePeak, peak, color);
            else
                hvGfxLine(hvg, sPeak, peak, ePeak, peak, color);

            // map box on horizontal line
            mapBoxHgcOrHgGene(hvg, s, e, sPeak, peak-2, ePeak - sPeak, 4,
                               tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);

            // map boxes on verticals
            if (sOnScreen)
                mapBoxHgcOrHgGene(hvg, s, e, sx - 2, yOff, 4, peak - yOff,
                                       tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
            if (eOnScreen)
                mapBoxHgcOrHgGene(hvg, s, e, ex - 2, yOff, 4, peak - yOff,
                                       tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
            }
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
//tg->bedSize = 6;
//bedMethods(tg);
tg->loadItems = interactLoadItems;
tg->drawItems = interactDrawItems;
tg->drawLeftLabels = interactDrawLeftLabels;
tg->totalHeight = interactTotalHeight;
tg->mapsSelf = TRUE;
}

