/* lrgTrack.c - display Locus Reference Genomic (LRG) sequences mapped to genome assembly */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "hgTracks.h"
#include "bigBed.h"
#include "lrg.h"

static struct linkedFeatures *lrgToLf(struct lrg *lrg)
/* Translate LRG into a linkedFeatures item. */
{
struct linkedFeatures *lf = lfFromBed((struct bed *)lrg);
lf->original = lrg;
return lf;
}

static void lrgLoadItems(struct track *tg)
/* Load LRGs in range, translate to linkedFeatures and store as tg->items. */
{
struct lm *lm = lmInit(0);
struct bigBedInterval *bb, *bbList = bigBedSelectRange(tg, chromName, winStart, winEnd, lm);
for (bb = bbList; bb != NULL; bb = bb->next)
    {
    char *lrgRow[LRG_NUM_COLS];
    char startBuf[16], endBuf[16];
    int bbFieldCount = bigBedIntervalToRow(bb, chromName, startBuf, endBuf, lrgRow,
					   ArraySize(lrgRow));
    if (bbFieldCount != LRG_NUM_COLS)
	errAbort("lrgLoadItems: expected %d columns for row has %d", LRG_NUM_COLS, bbFieldCount);
    struct lrg *lrg = lrgLoad(lrgRow);
    slAddHead(&(tg->items), lrgToLf(lrg));
    }
slReverse(&(tg->items));
lmCleanup(&lm);
}

static char *lrgItemName(struct track *tg, void *item)
/* Return LRG ID and (if available) HUGO/HGNC gene symbol. */
{
struct linkedFeatures *lf = item;
struct lrg *lrg = lf->original;
if (isNotEmpty(lrg->hgncSymbol))
    {
    int nameLen = strlen(lrg->name);
    int symLen = strlen(lrg->hgncSymbol);
    int extraLen = 3;  // " ()"
    int labelSize = nameLen + symLen + extraLen + 1;
    char *label = needMem(labelSize);
    safef(label, labelSize, "%s (%s)", lrg->name, lrg->hgncSymbol);
    return label;
    }
else
    return lf->name;
}

void lrgMethods(struct track *tg)
/* Locus Reference Genomic (bigBed 12 +) handlers. */
{
linkedFeaturesMethods(tg);
tg->canPack = TRUE;
tg->isBigBed = TRUE;
tg->loadItems = lrgLoadItems;
tg->itemName = lrgItemName;
}
