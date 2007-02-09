/* hapmapTrack - Handle HapMap track. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"

#include "hapmapAlleles.h"
#include "hapmapAllelesCombined.h"
#include "hapmapAllelesOrtho.h"

boolean isMixed(int allele1CountCEU, int allele1CountCHB, int allele1CountJPT, int allele1CountYRI,
                int allele2CountCEU, int allele2CountCHB, int allele2CountJPT, int allele2CountYRI)
/* return TRUE if different populations have a different major allele */
/* don't need to consider heteroCount */
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

boolean isAllPops(int allele1CountCEU, int allele1CountCHB, int allele1CountJPT, int allele1CountYRI,
                  int allele2CountCEU, int allele2CountCHB, int allele2CountJPT, int allele2CountYRI)
/* return TRUE if data available for all populations */
/* once combined table has heteroCount, add that here */
{
if (allele1CountCEU == 0 && allele2CountCEU == 0) return FALSE;
if (allele1CountCHB == 0 && allele2CountCHB == 0) return FALSE;
if (allele1CountJPT == 0 && allele2CountJPT == 0) return FALSE;
if (allele1CountYRI == 0 && allele2CountYRI == 0) return FALSE;

return TRUE;
}

boolean isComplexObserved(char *observed)
/* return TRUE if not simple bi-allelic A/C, A/G, A/T, etc. */
{
if (sameString(observed, "A/C")) return FALSE;
if (sameString(observed, "A/G")) return FALSE;
if (sameString(observed, "A/T")) return FALSE;
if (sameString(observed, "C/G")) return FALSE;
if (sameString(observed, "C/T")) return FALSE;
if (sameString(observed, "G/T")) return FALSE;
return TRUE;
}


static void hapmapLoad(struct track *tg)
/* load filtered data */
/* currently mixed and geno filters apply only to combined track, could extend that */
/* mixed and geno filters calibrated to all populations, regardless of what is displayed, could change that */
/* observed filter applies to everything but orthos, could add observed to ortho table */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;
char optionScoreStr[128];
int optionScore;
char extraWhere[128];

char *observedFilter = cartCgiUsualString(cart, HA_OBSERVED, "don't care");
boolean complexObserved = FALSE;

safef(optionScoreStr, sizeof(optionScoreStr), "%s.scoreFilter", tg->mapName);
optionScore = cartUsualInt(cart, optionScoreStr, 0);
if (sameString(tg->mapName, "hapmapAllelesCombined"))
    {
    struct hapmapAllelesCombined *combinedLoadItem, *combinedItemList = NULL;
    char *mixedFilter = cartCgiUsualString(cart, HA_POP_MIXED, "don't care");
    char *genoFilter = cartCgiUsualString(cart, HA_GENO_AVAIL, "don't care");
    boolean mixed = FALSE;
    boolean popsAll = TRUE;
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        combinedLoadItem = hapmapAllelesCombinedLoad(row+rowOffset);

        mixed = isMixed(combinedLoadItem->allele1CountCEU, combinedLoadItem->allele1CountCHB,
                combinedLoadItem->allele1CountJPT, combinedLoadItem->allele1CountYRI,
                combinedLoadItem->allele2CountCEU, combinedLoadItem->allele2CountCHB,
                combinedLoadItem->allele2CountJPT, combinedLoadItem->allele2CountYRI);
	if (sameString(mixedFilter, "not mixed") && mixed) continue;
	if (sameString(mixedFilter, "only mixed") && !mixed) continue;

        popsAll = isAllPops(combinedLoadItem->allele1CountCEU, combinedLoadItem->allele1CountCHB,
                combinedLoadItem->allele1CountJPT, combinedLoadItem->allele1CountYRI,
                combinedLoadItem->allele2CountCEU, combinedLoadItem->allele2CountCHB,
                combinedLoadItem->allele2CountJPT, combinedLoadItem->allele2CountYRI);
	if (sameString(genoFilter, "all populations") && !popsAll) continue;
	if (sameString(genoFilter, "subset of populations") && popsAll) continue;

        complexObserved = isComplexObserved(combinedLoadItem->observed);
	if (sameString(observedFilter, "complex") && !complexObserved) continue;
	if (sameString(observedFilter, "bi-alleleic") && complexObserved) continue;

        slAddHead(&combinedItemList, combinedLoadItem);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    slSort(&combinedItemList, bedCmp);
    tg->items = combinedItemList;
    return;
    }

if (sameString(tg->mapName, "hapmapAllelesChimp") || sameString(tg->mapName, "hapmapAllelesMacaque"))
{
    struct hapmapAllelesOrtho *orthoLoadItem, *orthoItemList = NULL;
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        orthoLoadItem = hapmapAllelesOrthoLoad(row+rowOffset);
        slAddHead(&orthoItemList, orthoLoadItem);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    slSort(&orthoItemList, bedCmp);
    tg->items = orthoItemList;
    return;
}

struct hapmapAlleles *simpleLoadItem, *simpleItemList = NULL;
if (optionScore > 0)
    {
    safef(extraWhere, sizeof(extraWhere), "score >= %d", optionScore);
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, extraWhere, &rowOffset);
    }
else
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    simpleLoadItem = hapmapAllelesLoad(row+rowOffset);

    complexObserved = isComplexObserved(simpleLoadItem->observed);
    if (sameString(observedFilter, "complex") && !complexObserved) continue;
    if (sameString(observedFilter, "bi-alleleic") && complexObserved) continue;

    slAddHead(&simpleItemList, simpleLoadItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&simpleItemList, bedCmp);
tg->items = simpleItemList;
}

void hapmapDrawAt(struct track *tg, void *item, struct vGfx *vg, int xOff, int y, double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw the hapmap alleles at a given position. */
/* Display major allele when zoomed to base level. */
{

if (!zoomedToBaseLevel)
    {
    bedDrawSimpleAt(tg, item, vg, xOff, y, scale, font, color, vis);
    return;
    }

char *allele = NULL;
char *strand = NULL;
int chromStart = 0;
int chromEnd = 0;

if (sameString(tg->mapName, "hapmapAllelesCombined"))
    {
    /* this needs to be normalized */
    struct hapmapAllelesCombined *thisItem = item;
    int count1 = 0;
    int count2 = 0;
    chromStart = thisItem->chromStart;
    chromEnd = thisItem->chromEnd;
    strand = cloneString(thisItem->strand);
    count1 = count1 + thisItem->allele1CountCEU;
    count1 = count1 + thisItem->allele1CountCHB;
    count1 = count1 + thisItem->allele1CountJPT;
    count1 = count1 + thisItem->allele1CountYRI;
    count2 = count2 + thisItem->allele2CountCEU;
    count2 = count2 + thisItem->allele2CountCHB;
    count2 = count2 + thisItem->allele2CountJPT;
    count2 = count2 + thisItem->allele2CountYRI;
    if (count1 >= count2)
        allele = cloneString(thisItem->allele1);
    else
        allele = cloneString(thisItem->allele2);
    }
else if (sameString(tg->mapName, "hapmapAllelesChimp") || sameString(tg->mapName, "hapmapAllelesMacaque"))
    {
    struct hapmapAllelesOrtho *thisItem = item;
    chromStart = thisItem->chromStart;
    chromEnd = thisItem->chromEnd;
    allele = cloneString(thisItem->orthoAllele);
    strand = cloneString(thisItem->strand);
    }
else
    {
    struct hapmapAlleles *thisItem = item;
    chromStart = thisItem->chromStart;
    chromEnd = thisItem->chromEnd;
    strand = cloneString(thisItem->strand);
    if (thisItem->allele1Count >= thisItem->allele2Count)
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

/* might possibly reverse and then reverse again, but this keeps the logic simple */
if (sameString(strand, "-"))
    reverseComplement(allele, 1);
if (cartUsualBoolean(cart, COMPLEMENT_BASES_VAR, FALSE))
    reverseComplement(allele, 1);

vgTextCentered(vg, x1, y, w, heightPer, textColor, font, allele);

}



static Color hapmapColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw hapmap item */
/* Use grayscale based on minor allele. */
/* The higher minor allele frequency, the darker the shade. */
/* Could also use score, since I've set that based on minor allele frequency. */
{
Color col;
int totalCount = 0;
int minorCount = 0;
int grayLevel = 0;

if (sameString(tg->mapName, "hapmapAllelesChimp") || sameString(tg->mapName, "hapmapAllelesMacaque"))
    return MG_BLACK;

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
    /* this should be normalized */
    int allele1Count = thisItem->allele1CountCEU + thisItem->allele1CountCHB +
	                   thisItem->allele1CountJPT + thisItem->allele1CountYRI;
    int allele2Count = thisItem->allele2CountCEU + thisItem->allele2CountCHB +
	                   thisItem->allele2CountJPT + thisItem->allele2CountYRI;
    totalCount = allele1Count + allele2Count;
    minorCount = min(allele1Count, allele2Count);
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

