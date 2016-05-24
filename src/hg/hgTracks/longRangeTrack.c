
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
        int sx = (longRange->s - seqStart) * scale + xOff;
        int ex = (longRange->e - seqStart) * scale + xOff;
        double longRangeWidth = longRange->e - longRange->s;
        int height = (tg->height - 15) * ((double)longRangeWidth / maxWidth) + yOff + 10;
        int tsx = sx;
        int tex = ex;

        if (tsx > tex)
            {
            tsx = sx;
            tex = ex;
            }
        
        hvGfxLine(hvg, sx, yOff, tsx, height, color);
        hvGfxLine(hvg, ex, yOff, tex, height, color);

        if (tg->visibility == tvFull)
            {
            hvGfxLine(hvg, tsx, height, tex, height, color);
            char statusBuf[2048];
            safef(statusBuf, sizeof statusBuf, "%g %s:%d", longRange->score, longRange->eChrom, longRange->e);
            char itemBuf[2048];
            safef(itemBuf, sizeof itemBuf, "%d", longRange->id);

            mapBoxHgcOrHgGene(hvg, longRange->s, longRange->e,tsx, height-2, tex-tsx, 4,
                                   tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
            mapBoxHgcOrHgGene(hvg, longRange->s, longRange->e,tsx - 2, yOff, 4, height - yOff,
                                   tg->track, itemBuf, statusBuf, NULL, TRUE, NULL);
            mapBoxHgcOrHgGene(hvg, longRange->s, longRange->e,tex - 2, yOff, 4, height - yOff,
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
