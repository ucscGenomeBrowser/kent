/* makeItemsTrack.c - supports tracks of type makeItems.  Users can drag to create an item
 * and click to edit one. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "bed.h"

void makeItemsLoadItems(struct track *tg)
/* Load up items in track already.  Also make up a pseudo-item that is
 * where you drag to create an item. */
{
struct bed *firstItem;
AllocVar(firstItem);
firstItem->chrom = cloneString(chromName);
firstItem->chromStart = winStart;
firstItem->chromEnd = winEnd;
firstItem->name = cloneString("Drag here to create a new item");
tg->items = firstItem;
}

void makeItemsMethods(struct track *track)
/* Set up special methods for makeItems type tracks. */
{
bedMethods(track);
track->loadItems = makeItemsLoadItems;
track->mapsSelf = TRUE;
track->canPack = TRUE;
}
