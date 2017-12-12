
#include "common.h"
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
double scale = scaleForWindow(width, seqStart, seqEnd);
struct bed *beds = tg->items;
unsigned int maxWidth;
struct longRange *longRange;
char buffer[1024];
char itemBuf[2048];
safef(buffer, sizeof buffer, "%s.%s", tg->tdb->track, LONG_MINSCORE);
double minScore = sqlDouble(cartUsualString(cart, buffer, LONG_DEFMINSCORE));
struct longRange *longRangeList = parseLongTabix(beds, &maxWidth, minScore);

for(longRange=longRangeList; longRange; longRange=longRange->next)
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
        // FIXME: Most of this code should be shared with same chrom display

        // draw the foot
        int footWidth = sFootWidth;
        hvGfxLine(hvg, sx - footWidth, yOff, sx + footWidth, yOff, color);

        int height = tg->height/2;
        if (tg->visibility == tvDense)
            height = tg->height;
        unsigned yPos = yOff + height;
        hvGfxLine(hvg, sx, yOff, sx, yPos, color);
        if (tg->visibility == tvFull)
            {
            mapBoxHgcOrHgGene(hvg, longRange->s, longRange->s, sx - 2, yOff, 4, tg->height/2,
                                   tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);

            safef(buffer, sizeof buffer, "%s:%d",  longRange->eChrom, longRange->e);
            hvGfxTextCentered(hvg, sx, yPos + 2, 4, 4, MG_BLUE, font, buffer);
            int width = vgGetFontStringWidth(hvg->vg, font, buffer);
            int height = vgGetFontPixelHeight(hvg->vg, font);
            mapBoxHgcOrHgGene(hvg, longRange->s, longRange->s, sx - width/2, yPos, width, height,
                                   tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
            }
        continue;
        }

    // same chromosome
    boolean sOnScreen = (longRange->s >= seqStart) && (longRange->s < seqEnd);
    boolean eOnScreen = (longRange->e >= seqStart) && (longRange->e < seqEnd);
    if (!(sOnScreen || eOnScreen))
        continue;

    double longRangeWidth = longRange->e - longRange->s;
    int peak = (tg->height - 15) * ((double)longRangeWidth / maxWidth) + yOff + 10;
    if (tg->visibility == tvDense)
        peak = yOff + tg->height;

    if (sOnScreen)
        {
        // draw foot of 'lower' region
        hvGfxLine(hvg, sx - sFootWidth, yOff, sx + sFootWidth, yOff, color);

        // draw vertical
        hvGfxLine(hvg, sx, yOff, sx, peak, color);
        }
    if (eOnScreen)
        {
        // draw foot of 'upper' region
        hvGfxLine(hvg, ex - eFootWidth, yOff, ex + eFootWidth, yOff, color);

        // draw vertical
        hvGfxLine(hvg, ex, yOff, ex, peak, color); //OLD
        }

    if (tg->visibility == tvFull)
        {
        // draw link between regions (dense mode just shows feet ??)
        unsigned ePeak = eOnScreen ? ex : xOff + width;
        unsigned sPeak = sOnScreen ? sx : xOff;
        hvGfxLine(hvg, sPeak, peak, ePeak, peak, color);

        if (sOnScreen)
            mapBoxHgcOrHgGene(hvg, longRange->s, longRange->e, sx - 2, yOff, 4, peak - yOff,
                                   tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
        if (eOnScreen)
            mapBoxHgcOrHgGene(hvg, longRange->s, longRange->e, ex - 2, yOff, 4, peak - yOff,
                                   tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);

        mapBoxHgcOrHgGene(hvg, longRange->s, longRange->e, sPeak, peak-2, ePeak - sPeak, 4,
                               tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
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
