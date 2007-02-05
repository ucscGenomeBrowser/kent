/* hapmapTrack - Handle HapMap track. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"

#include "hapmapAlleles.h"
#include "hapmapAllelesCombined.h"


static void hapmapLoad(struct track *tg)
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;

struct hapmapAlleles *simpleLoadItem, *simpleItemList = NULL;

if (sameString(tg->mapName, "hapmapAllelesCombined"))
    {
    struct hapmapAllelesCombined *combinedLoadItem, *combinedItemList = NULL;
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        combinedLoadItem = hapmapAllelesCombinedLoad(row+rowOffset);
        slAddHead(&combinedItemList, combinedLoadItem);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    slSort(&combinedItemList, bedCmp);
    tg->items = combinedItemList;
    return;
    }

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    simpleLoadItem = hapmapAllelesLoad(row+rowOffset);
    slAddHead(&simpleItemList, simpleLoadItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&simpleItemList, bedCmp);
tg->items = simpleItemList;
}

void hapmapDrawAt(struct track *tg, void *item, struct vGfx *vg, int xOff, int y, double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw the hapmap at position. */
{

if (!zoomedToBaseLevel)
    {
    bedDrawSimpleAt(tg, item, vg, xOff, y, scale, font, color, vis);
    return;
    }

char *allele = NULL;
int chromStart = 0;
int chromEnd = 0;

if (sameString(tg->mapName, "hapmapAllelesCombined"))
    {
    struct hapmapAllelesCombined *thisItem = item;
    int count1 = 0;
    int count2 = 0;
    chromStart = thisItem->chromStart;
    chromEnd = thisItem->chromEnd;
    count1 = count1 + thisItem->allele1CountCEU;
    count1 = count1 + thisItem->allele1CountCHB;
    count1 = count1 + thisItem->allele1CountJPT;
    count1 = count1 + thisItem->allele1CountYRI;
    count2 = count2 + thisItem->allele2CountCEU;
    count2 = count2 + thisItem->allele2CountCHB;
    count2 = count2 + thisItem->allele2CountJPT;
    count2 = count2 + thisItem->allele2CountYRI;
    if (count1 < count2)
        allele = cloneString(thisItem->allele1);
    else
        allele = cloneString(thisItem->allele2);
    }
else
    {
    struct hapmapAlleles *thisItem = item;
    chromStart = thisItem->chromStart;
    chromEnd = thisItem->chromEnd;
    if (thisItem->allele1Count < thisItem->allele2Count)
        allele = cloneString(thisItem->allele1);
    else
        allele = cloneString(thisItem->allele2);
    }

int heightPer, x1, x2, w;
Color textColor = MG_BLACK;

heightPer = tg->heightPer;
x1 = round((double)((int)chromStart-winStart)*scale) + xOff;
x2 = round((double)((int)chromEnd-winStart)*scale) + xOff;
w = x2-x1;
if (w < 1)
    w = 1;
y += (heightPer >> 1) - 3;

vgTextCentered(vg, x1, y, w, heightPer, textColor, font, allele);

}

boolean isMixed(int allele1CountCEU, int allele1CountCHB, int allele1CountJPT, int allele1CountYRI,
                int allele2CountCEU, int allele2CountCHB, int allele2CountJPT, int allele2CountYRI)
/* return TRUE if different populations have a different major allele */
{
int allele1Count = 0;
int allele2Count = 0;

if (allele1CountCEU >= allele2CountCEU && allele1CountCEU > 0) 
    allele1Count++;
else if (allele2CountCEU > 0)
    allele2Count++;

if (allele1CountCHB >= allele2CountCHB && allele1CountCHB > 0) 
    allele1Count++;
else if (allele2CountCHB > 0)
    allele2Count++;

if (allele1CountJPT >= allele2CountJPT && allele1CountJPT > 0) 
    allele1Count++;
else if (allele2CountJPT > 0)
    allele2Count++;

if (allele1CountYRI >= allele2CountYRI && allele1CountYRI > 0) 
    allele1Count++;
else if (allele2CountYRI > 0)
    allele2Count++;

if (allele1Count > 0 && allele2Count > 0) return TRUE;

return FALSE;
}



static Color hapmapColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw hapmap item */
{
Color col;
int totalCount = 0;
int minorCount = 0;
int grayLevel = 0;

if (sameString(tg->mapName, "hapmapAllelesCombined"))
    {
    struct hapmapAllelesCombined *thisItem = item;
    if (isMixed(thisItem->allele1CountCEU, thisItem->allele1CountCHB,
                thisItem->allele1CountJPT, thisItem->allele1CountYRI,
                thisItem->allele2CountCEU, thisItem->allele2CountCHB,
                thisItem->allele2CountJPT, thisItem->allele2CountYRI))
	{
        col = MG_GREEN;
	return col;
	}
    else
        {
	int allele1Count = thisItem->allele1CountCEU + thisItem->allele1CountCHB +
	                   thisItem->allele1CountJPT + thisItem->allele1CountYRI;
	int allele2Count = thisItem->allele2CountCEU + thisItem->allele2CountCHB +
	                   thisItem->allele2CountJPT + thisItem->allele2CountYRI;
	totalCount = allele1Count + allele2Count;
        minorCount = min(allele1Count, allele2Count);
	}
    }
else
    {
    struct hapmapAlleles *thisItem = item;
    totalCount = thisItem->allele1Count + thisItem->allele2Count;
    minorCount = min(thisItem->allele1Count, thisItem->allele2Count);
    }

grayLevel = grayInRange(minorCount, 0, totalCount/2);
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
tg->drawItemAt = hapmapDrawAt;
tg->loadItems = hapmapLoad;
tg->itemColor = hapmapColor;
tg->itemNameColor = hapmapColor;
tg->itemLabelColor = hapmapColor;

tg->freeItems = hapmapFree;
// tg->colorShades = shadesOfGray;
}

