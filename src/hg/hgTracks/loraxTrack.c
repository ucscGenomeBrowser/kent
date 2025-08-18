/* loraxTrack -- handlers for a track that interfaces with a local instance of the Lorax app. */
/* See https://github.com/pratikkatte/lorax */

/* Copyright (C) 2025 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "bbiFile.h"
#include "bed.h"
#include "hgTracks.h"


static void loraxLoadItems(struct track *tg)
/* Load Lorax/tskit tree regions items in bigBed format */
{
struct lm *lm = lmInit(0);
struct bbiFile *bbi = fetchBbiForTrack(tg);
struct bigBedInterval *bb, *bbList = NULL;
bbList = bigBedSelectRange(tg, chromName, winStart, winEnd, lm);
char *bedRow[bbi->fieldCount];
char startBuf[16], endBuf[16];
struct bed *list = NULL;
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    bigBedIntervalToRow(bb, chromName, startBuf, endBuf, bedRow, ArraySize(bedRow));
    struct bed *bed = bedLoad3(bedRow);
    slAddHead(&list, bed);
    }
lmCleanup(&lm);
slReverse(&list);
tg->items = list;
}


static int loraxTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return configured height of track. */
{
return tg->lineHeight;
}


static void loraxDrawItems(struct track *tg, int seqStart, int seqEnd,
                    struct hvGfx *hvg, int xOff, int yOff, int width,
                    MgFont *font, Color color, enum trackVisibility vis)
/* Draw Lorax/tskit tree regions. */
{
static boolean alreadyIncludedJs = FALSE;
if (!alreadyIncludedJs)
    {
    jsIncludeFile("lorax.js", NULL);
    alreadyIncludedJs = TRUE;
    }
double scale = scaleForWindow(width, seqStart, seqEnd);
struct bed *bedList = tg->items, *bed;
for (bed = bedList;  bed != NULL;  bed = bed->next)
    {
    int height = loraxTotalHeight(tg, vis);
    int x1 = round((double)((int)bed->chromStart-winStart)*scale) + xOff;
    int x2 = round((double)((int)bed->chromEnd-winStart)*scale) + xOff;
    int y1 = yOff;
    int y2 = yOff + height - 1;

    // For now just draw an empty box.
    hvGfxLine(hvg, x1, y1, x1, y2, color);
    hvGfxLine(hvg, x2, y1, x2, y2, color);
    hvGfxLine(hvg, x1, y1, x2, y1, color);
    hvGfxLine(hvg, x1, y2, x2, y2, color);

    // Make own map item here (tg->mapsSelf is TRUE)
    genericMapItem(tg, hvg, bed, "", "", bed->chromStart, bed->chromEnd, x1, y1, (x2 - x1), height);
    }
}


static void loraxDoLeftLabels(struct track *tg, int seqStart, int seqEnd, struct hvGfx *hvg,
                              int xOff, int yOff, int width, int height, boolean withCenterLabels,
                              MgFont *font, Color color, enum trackVisibility vis)
/* For now, draw no left labels. */
{
return;
}

static void loraxFreeItems(struct track *tg)
/* Free track items (bed list). */
{
bedFreeList((struct bed **)(tg->items));
}


void loraxMethods(struct track *tg)
/* Lorax track type methods */
{
tg->loadItems = loraxLoadItems;
tg->drawItems = loraxDrawItems;
tg->totalHeight = loraxTotalHeight;
tg->drawLeftLabels = loraxDoLeftLabels;
tg->mapsSelf = TRUE;
tg->freeItems = loraxFreeItems;
tg->mapsSelf = 1;
}
