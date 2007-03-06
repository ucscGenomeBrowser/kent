/* hapmapTrack - Handle HapMap track. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"

#include "hapmapAlleles.h"
#include "hapmapAllelesCombined.h"
#include "hapmapAllelesOrtho.h"

// char *mixedFilter = HA_POP_MIXED_DEFAULT;
// char *popCountFilter = HA_POP_COUNT_DEFAULT;
// char *observedFilter = HA_TYPE_DEFAULT;
// float minFreqFilter = sqlFloat(HA_MIN_FREQ);
// float maxFreqFilter = sqlFloat(HA_MAX_FREQ);
// float minHetFilter = sqlFloat(HA_MIN_HET);
// float maxHetFilter = sqlFloat(HA_MAX_HET);
// boolean monoFilter = HA_MONO_DEFAULT;
// char *chimpFilter = HA_CHIMP_DEFAULT;
// int chimpQualFilter = atoi(HA_CHIMP_QUAL_DEFAULT);
// char *macaqueFilter = HA_MACAQUE_DEFAULT;
// int macaqueQualFilter = atoi(HA_MACAQUE_QUAL_DEFAULT);


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

boolean isTransitionObserved(char *observed)
{
if (sameString(observed, "A/G")) return TRUE;
if (sameString(observed, "C/T")) return TRUE;
return FALSE;
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

// void loadFilters()
// {
// mixedFilter = cartUsualString(cart, HA_POP_MIXED, HA_POP_MIXED_DEFAULT);
// popCountFilter = cartUsualString(cart, HA_POP_COUNT, HA_POP_COUNT_DEFAULT);
// observedFilter = cartUsualString(cart, HA_TYPE, HA_TYPE_DEFAULT);
// minFreqFilter = sqlFloat(cartUsualString(cart, HA_MIN_FREQ, HA_MIN_FREQ_DEFAULT));
// maxFreqFilter = sqlFloat(cartUsualString(cart, HA_MAX_FREQ, HA_MAX_FREQ_DEFAULT));
// minHetFilter = sqlFloat(cartUsualString(cart, HA_MIN_HET, HA_MIN_HET_DEFAULT));
// maxHetFilter = sqlFloat(cartUsualString(cart, HA_MAX_HET, HA_MAX_HET_DEFAULT));
// boolean monoFilter = cartUsualBoolean(cart, HA_MONO, HA_MONO_DEFAULT);
// chimpFilter = cartUsualString(cart, HA_CHIMP, HA_CHIMP_DEFAULT);
// chimpQualFilter = 
    // sqlUnsigned(cartUsualString(cart, HA_CHIMP_QUAL, HA_CHIMP_QUAL_DEFAULT));
// macaqueFilter = cartUsualString(cart, HA_MACAQUE, HA_MACAQUE_DEFAULT);
// macaqueQualFilter = 
    // sqlUnsigned(cartUsualString(cart, HA_MACAQUE_QUAL, HA_MACAQUE_QUAL_DEFAULT));
// }

// boolean allDefaults()
/* return TRUE if all filters are set to default values */
// {
// if (differentString(mixedFilter, HA_POP_MIXED_DEFAULT)) return FALSE;
// if (differentString(popCountFilter, HA_POP_COUNT_DEFAULT)) return FALSE;
// if (differentString(observedFilter, HA_TYPE_DEFAULT)) return FALSE;
// if (minFreqFilter > sqlFloat(HA_MIN_FREQ_DEFAULT)) return FALSE;
// if (maxFreqFilter < sqlFloat(HA_MAX_FREQ_DEFAULT)) return FALSE;
// if (minHetFilter > sqlFloat(HA_MIN_HET_DEFAULT)) return FALSE;
// if (maxFreqFilter < sqlFloat(HA_MAX_HET_DEFAULT)) return FALSE;
// if (monoFilter != HA_MONO_DEFAULT) return FALSE;
// if (differentString(chimpFilter, HA_CHIMP_DEFAULT)) return FALSE;
// if (chimpQualFilter > sqlUnsigned(HA_CHIMP_QUAL_DEFAULT)) return FALSE;
// if (differentString(macaqueFilter, HA_MACAQUE_DEFAULT)) return FALSE;
// if (macaqueQualFilter > sqlUnsigned(HA_MACAQUE_QUAL_DEFAULT)) return FALSE;
// }



// static void hapmapLoadSimple(struct track *tg)
/* load data, no filters */
// {
// struct sqlConnection *conn = hAllocConn();
// struct sqlResult *sr = NULL;
// char **row = NULL;
/* do I need this? */
// int rowOffset = 0;

// if (sameString(tg->mapName, "hapmapAllelesChimp") || sameString(tg->mapName, "hapmapAllelesMacaque"))
// {
    // struct hapmapAllelesOrtho *orthoLoadItem, *orthoItemList = NULL;
    // sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
    // while ((row = sqlNextRow(sr)) != NULL)
        // {
        // orthoLoadItem = hapmapAllelesOrthoLoad(row+rowOffset);
        // slAddHead(&orthoItemList, orthoLoadItem);
        // }
    // sqlFreeResult(&sr);
    // hFreeConn(&conn);
    // slSort(&orthoItemList, bedCmp);
    // tg->items = orthoItemList;
    // return;
// }

// struct hapmapAlleles *simpleLoadItem, *simpleItemList = NULL;
// sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
// while ((row = sqlNextRow(sr)) != NULL)
    // {
    // simpleLoadItem = hapmapAllelesLoad(row+rowOffset);
    // slAddHead(&simpleItemList, simpleLoadItem);
    // }
// sqlFreeResult(&sr);
// hFreeConn(&conn);
// slSort(&simpleItemList, bedCmp);
// tg->items = simpleItemList;
// }


static void hapmapLoad(struct track *tg)
/* load filtered data */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 0;

// loadFilters();

// if (allDefaults()) 
    // {
    // hapmapLoadSimple(tg);
    // return;
    // }

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
/* Draw the hapmap alleles at a given position. */
/* Display major allele when zoomed to base level. */
{

if (!zoomedToBaseLevel)
    {
    bedDrawSimpleAt(tg, item, vg, xOff, y, scale, font, color, vis);
    return;
    }

/* figure out allele to display */
/* allele varies depending on which type of subtrack */

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
    if (thisItem->homoCount1 >= thisItem->homoCount2)
        allele = cloneString(thisItem->allele1);
    else
        allele = cloneString(thisItem->allele2);
    }

/* determine graphics attributes for vgTextCentered */
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
/* Use grayscale based on minor allele score or quality score. */
/* The higher minor allele frequency, the darker the shade. */
/* The higher quality score, the darker the shade. */
{
Color col;
int totalCount = 0;
int minorCount = 0;
int grayLevel = 0;

if (sameString(tg->mapName, "hapmapAllelesChimp") || sameString(tg->mapName, "hapmapAllelesMacaque"))
    {
    struct hapmapAllelesOrtho *thisItem = item;
    grayLevel = grayInRange(thisItem->score, 0, 100);
    }

else if (sameString(tg->mapName, "hapmapAllelesCombined"))
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
    /* Could also use score, since I've set that based on minor allele frequency. */
    totalCount = thisItem->homoCount1 + thisItem->homoCount2;
    minorCount = min(thisItem->homoCount1, thisItem->homoCount2);
    grayLevel = grayInRange(minorCount, 0, totalCount/2);
    }

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

