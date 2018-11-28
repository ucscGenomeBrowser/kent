/* lollipopTrack -- draw lollipopion between two genomic regions */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hgTracks.h"
#include "bedCart.h"
#include "bigWarn.h"
#include "lollipop.h"

void lollipopDrawItems(struct track *tg, int seqStart, int seqEnd,
        struct hvGfx *hvg, int xOff, int yOff, int width, 
        MgFont *font, Color color, enum trackVisibility vis)
/* Draw a list of lollipop structures. */
{
}

void lollipopLoadItems(struct track *tg)
    /* Load interact items in interact format */
{
//loadAndFilterItems(tg);
}

void lollipopMethods(struct track *tg)
/* Interact track type methods */
{
tg->bedSize = 12;
linkedFeaturesMethods(tg);         // for most vis and mode settings
tg->loadItems = lollipopLoadItems;
//tg->itemRightPixels = lollipopRightPixels;
tg->drawItems = lollipopDrawItems;
}
