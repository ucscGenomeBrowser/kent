
#include "common.h"
#include "hgTracks.h"
#include "longRange.h"


static int longRangeHeight(struct track *tg, enum trackVisibility vis)
/* calculate height of all the snakes being displayed */
{
if ( tg->visibility == tvDense)
    return  tl.fontHeight;
char buffer[1024];
safef(buffer, sizeof buffer, "%s.%s", tg->tdb->track, LONG_HEIGHT );
return tg->height = sqlUnsigned(cartUsualString(cart, buffer, LONG_DEFHEIGHT));
}


static void longRangeDraw(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
{
double scale = scaleForWindow(width, seqStart, seqEnd);
struct bed *beds = tg->items;
unsigned int maxWidth;
struct longRange *longRange;
char buffer[1024];

safef(buffer, sizeof buffer, "%s.%s", tg->tdb->track, LONG_MINSCORE);
double minScore = sqlDouble(cartUsualString(cart, buffer, LONG_DEFMINSCORE));
struct longRange *longRangeList = parseLongTabix(beds, &maxWidth, minScore);

for(longRange=longRangeList; longRange; longRange=longRange->next)
    {
    if (sameString(longRange->sChrom, longRange->eChrom))
        {
        boolean sOnScreen = (longRange->s >= seqStart) && (longRange->s < seqEnd);
        boolean eOnScreen = (longRange->e >= seqStart) && (longRange->e < seqEnd);

        if (!(sOnScreen || eOnScreen))
            continue;

        unsigned sx = 0, ex = 0;
        if (sOnScreen)
            sx = (longRange->s - seqStart) * scale + xOff;
        if (eOnScreen)
            ex = (longRange->e - seqStart) * scale + xOff;

        double longRangeWidth = longRange->e - longRange->s;
        int peak = (tg->height - 15) * ((double)longRangeWidth / maxWidth) + yOff + 10;
        
        if (sOnScreen)
            hvGfxLine(hvg, sx, yOff, sx, peak, color);
        if (eOnScreen)
            hvGfxLine(hvg, ex, yOff, ex, peak, color);

        if (tg->visibility == tvFull)
            {
            unsigned sPeak = sOnScreen ? sx : xOff;
            unsigned ePeak = eOnScreen ? ex : xOff + width;

            hvGfxLine(hvg, sPeak, peak, ePeak, peak, color);
            char statusBuf[2048];
            safef(statusBuf, sizeof statusBuf, "%g %s:%d %s:%d", longRange->score, longRange->sChrom, longRange->s, longRange->eChrom, longRange->e);
            char itemBuf[2048];
            safef(itemBuf, sizeof itemBuf, "%d", longRange->id);

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
