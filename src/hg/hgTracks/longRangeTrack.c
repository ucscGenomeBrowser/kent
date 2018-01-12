
#include "common.h"
#include "memgfx.h"
#include "hgTracks.h"
#include "longRange.h"


static int longRangeHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the snakes being displayed */
{
if ( tg->visibility == tvDense)
    return  tl.fontHeight;
int min, max, deflt, current; 
cartTdbFetchMinMaxPixels(cart, tg->tdb, LONG_MINHEIGHT, LONG_MAXHEIGHT, atoi(LONG_DEFHEIGHT),
                                &min, &max, &deflt, &current);
return tg->height = current;
}

static Color longRangeItemColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw a long range item */
{
struct longRange *lr = item;
if (lr->hasColor)
    {
    struct rgbColor itemRgb;
    // There must be a better way...
    itemRgb.r = (lr->rgb & 0xff0000) >> 16;
    itemRgb.g = (lr->rgb & 0xff00) >> 8;
    itemRgb.b = lr->rgb & 0xff;
    return hvGfxFindColorIx(hvg, itemRgb.r, itemRgb.g, itemRgb.b);
    }
return tg->ixColor;
}

static void longRangeDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of longTabix structures. */
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
struct bed *beds = tg->items;
unsigned int maxWidth;
struct longRange *longRange;
char buffer[1024];
char itemBuf[2048];
safef(buffer, sizeof buffer, "%s.%s", tg->tdb->track, LONG_MINSCORE);
double minScore = sqlDouble(cartUsualString(cart, buffer, LONG_DEFMINSCORE));
struct longRange *longRangeList = parseLongTabix(beds, &maxWidth, minScore);
slSort(&longRangeList, longRangeCmp);

// Determine if there are mixed inter and intra-chromosomal 
// Suppress interchromosomal labels if they overlap
int nSame = 0, nOther = 0;
int prevLabelEnd = 0, prevLabelStart = 0;
char *prevLabel = 0;
boolean doOtherLabels = TRUE;
for (longRange=longRangeList; longRange; longRange=longRange->next)
    {
    if (sameString(longRange->sChrom, longRange->eChrom))
        nSame++;
    else
        {
        nOther++;
        if (!doOtherLabels)
            continue;
        int labelWidth = vgGetFontStringWidth(hvg->vg, font, longRange->eChrom);
        int sx = ((longRange->s - seqStart) + .5) * scale + xOff; // x coord of center
        int labelStart = sx - labelWidth/2;
        int labelEnd = labelStart + labelWidth - 1;
        if (labelStart <= prevLabelEnd && 
                !(labelStart == prevLabelStart && labelEnd == prevLabelEnd && 
                    sameString(longRange->eChrom, prevLabel)))
            doOtherLabels = FALSE;
        prevLabelStart = labelStart;
        prevLabelEnd = labelEnd;
        prevLabel = longRange->eChrom;
        }
    }
int fontHeight = vgGetFontPixelHeight(hvg->vg, font);
int otherHeight = (nOther) ? 3 * fontHeight : 0;
int sameHeight = (nSame) ? tg->height - otherHeight: 0;

// Draw items
for (longRange=longRangeList; longRange; longRange=longRange->next)
    {
    safef(itemBuf, sizeof itemBuf, "%d", longRange->id);
    struct dyString *ds = dyStringNew(0);
    if (!longRange->hasColor)
        dyStringPrintf(ds, "%g ", longRange->score);
    dyStringPrintf(ds, "%s:%d %s:%d", longRange->sChrom, longRange->s, longRange->eChrom, longRange->e);
    char *statusBuf = dyStringCannibalize(&ds);

    color = longRangeItemColor(tg, longRange, hvg);
    int sx = ((longRange->s - seqStart) + .5) * scale + xOff; // x coord of center (lower region)
    int sFootWidth = scale * (double)longRange->sw / 2;       // width in pixels of half foot (lower)
    int ex = ((longRange->e - seqStart) + .5) * scale + xOff;
    int eFootWidth = scale * (double)longRange->ew / 2;

    if (differentString(longRange->sChrom, longRange->eChrom))
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
        hvGfxLine(hvg, sx, yOffOther, sx, yPos, color);
        if (tg->visibility == tvFull)
            {
            mapBoxHgcOrHgGene(hvg, longRange->s, longRange->s, sx - 2, yOffOther, 4, height,
                                   tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);

            safef(buffer, sizeof buffer, "%s",  longRange->eChrom);
            if (doOtherLabels)
                {
                hvGfxTextCentered(hvg, sx, yPos + 2, 4, 4, MG_BLUE, font, buffer);
                int width = vgGetFontStringWidth(hvg->vg, font, buffer);
                mapBoxHgcOrHgGene(hvg, longRange->s, longRange->s, sx - width/2, yPos, 
                                width, fontHeight, tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
                }
            }
        continue;
        }

    // draw same chromosome interaction
    boolean sOnScreen = (longRange->s >= seqStart) && (longRange->s < seqEnd);
    boolean eOnScreen = (longRange->e >= seqStart) && (longRange->e < seqEnd);
    if (!(sOnScreen || eOnScreen))
        continue;       // current schema doesn't support display of these

    double longRangeWidth = longRange->e - longRange->s;
    int peakHeight = (sameHeight - 15) * ((double)longRangeWidth / maxWidth) + 10;
    int peak = yOff + peakHeight;
    if (tg->visibility == tvDense)
        peak = yOff + tg->height;

    if (sOnScreen)
        {
        // draw foot of first region
        hvGfxLine(hvg, sx - sFootWidth, yOff, sx + sFootWidth, yOff, color);

        // draw vertical
        if (!eOnScreen || draw == DRAW_LINE)
            hvGfxLine(hvg, sx, yOff, sx, peak, color);
        }
    if (eOnScreen)
        {
        // draw foot of second region
        hvGfxLine(hvg, ex - eFootWidth, yOff, ex + eFootWidth, yOff, color);

        // draw vertical
        if (!sOnScreen || draw == DRAW_LINE)
            hvGfxLine(hvg, ex, yOff, ex, peak, color); //OLD
        }
    if (tg->visibility == tvFull)
        {
        if (sOnScreen && eOnScreen && draw != DRAW_LINE)
            {
            if (draw == DRAW_CURVE)
                hvGfxCurve(hvg, sx, yOff, (ex-sx+1)/2 + sx, peak+60, ex, yOff, color);
            else if (draw == DRAW_ELLIPSE)
                {
                int yLeft = yOff + peakHeight;
                int yTop = yOff - peakHeight;
                hvGfxEllipseDraw(hvg, sx, yLeft, ex, yTop, color, ELLIPSE_BOTTOM); // demo
                }
            // TODO: map boxes for curves -- perhaps a colored dot at peak ?
            }
        else
            {
            // draw link horizontal line between regions (dense mode just shows feet ??)
            unsigned ePeak = eOnScreen ? ex : xOff + width;
            unsigned sPeak = sOnScreen ? sx : xOff;
            hvGfxLine(hvg, sPeak, peak, ePeak, peak, color);

            // map box on horizontal line
            mapBoxHgcOrHgGene(hvg, longRange->s, longRange->e, sPeak, peak-2, ePeak - sPeak, 4,
                               tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);

            // map boxes on verticals
            if (sOnScreen)
                mapBoxHgcOrHgGene(hvg, longRange->s, longRange->e, sx - 2, yOff, 4, peak - yOff,
                                       tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
            if (eOnScreen)
                mapBoxHgcOrHgGene(hvg, longRange->s, longRange->e, ex - 2, yOff, 4, peak - yOff,
                                       tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
            }
        }
    }
}

void longRangeDrawLeftLabels(struct track *tg, int seqStart, int seqEnd,
    struct hvGfx *hvg, int xOff, int yOff, int width, int height,
    boolean withCenterLabels, MgFont *font,
    Color color, enum trackVisibility vis)
{
}

void longRangeMethods(struct track *tg, struct trackDb *tdb)
{
tg->drawLeftLabels = longRangeDrawLeftLabels;
tg->drawItems = longRangeDraw;
tg->totalHeight = longRangeHeight; 
tg->mapsSelf = TRUE;
}
