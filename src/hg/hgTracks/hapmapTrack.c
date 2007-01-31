/* hapmapTrack - Handle HapMap track. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"

#include "hapmapAlleles.h"


static void hapmapLoad(struct track *tg)
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;
struct hapmapAlleles *loadItem, *itemList = NULL;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    loadItem = hapmapAllelesLoad(row+rowOffset);
    slAddHead(&itemList, loadItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&itemList, bedCmp);
tg->items = itemList;
}

static Color hapmapColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw hapmap item */
{
struct hapmapAlleles *thisItem = item;
Color col;
int grayLevel = 0;
int alleleCount = thisItem->allele1Count + thisItem->allele2Count;
int minorCount = min(thisItem->allele1Count, thisItem->allele2Count);

grayLevel = grayInRange(minorCount, 0, alleleCount/2);
if (grayLevel < maxShade)
    grayLevel++;
if (grayLevel < maxShade)
    grayLevel++;
col = shadesOfGray[grayLevel];

return col;
}

static void hapmapFree(struct track *tg)
/* Free up hapmap items. */
{
slFreeList(&tg->items);
}


void hapmapMethods(struct track *tg)
/* Make hapmap track.  */
{
bedMethods(tg);
tg->loadItems = hapmapLoad;
tg->itemColor = hapmapColor;
tg->itemNameColor = hapmapColor;
tg->itemLabelColor = hapmapColor;

tg->freeItems = hapmapFree;
// tg->colorShades = shadesOfGray;
}

