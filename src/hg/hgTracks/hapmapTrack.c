/* hapmapTrack - Handle HapMap track. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"

#include "hapmapAlleles.h"
#include "hapmapAllelesCombined.h"
#include "hapmapAllelesOrtho.h"
#include "hapmapAllelesSummary.h"

char *mixedFilter = HA_POP_MIXED_DEFAULT;
char *popCountFilter = HA_POP_COUNT_DEFAULT;
char *observedFilter = HA_TYPE_DEFAULT;
float minFreqFilter = 0.0;
float maxFreqFilter = 0.5;
float minHetFilter = 0.0;
float maxHetFilter = 0.5;
char *monoFilter = HA_MONO_DEFAULT;
char *chimpFilter = HA_CHIMP_DEFAULT;
int chimpQualFilter = 0;
char *macaqueFilter = HA_MACAQUE_DEFAULT;
int macaqueQualFilter = 0;


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

void loadFilters()
{
mixedFilter = cartUsualString(cart, HA_POP_MIXED, HA_POP_MIXED_DEFAULT);
popCountFilter = cartUsualString(cart, HA_POP_COUNT, HA_POP_COUNT_DEFAULT);
observedFilter = cartUsualString(cart, HA_TYPE, HA_TYPE_DEFAULT);
minFreqFilter = sqlFloat(cartUsualString(cart, HA_MIN_FREQ, HA_MIN_FREQ_DEFAULT));
maxFreqFilter = sqlFloat(cartUsualString(cart, HA_MAX_FREQ, HA_MAX_FREQ_DEFAULT));
minHetFilter = sqlFloat(cartUsualString(cart, HA_MIN_HET, HA_MIN_HET_DEFAULT));
maxHetFilter = sqlFloat(cartUsualString(cart, HA_MAX_HET, HA_MAX_HET_DEFAULT));
monoFilter = cartUsualString(cart, HA_MONO, HA_MONO_DEFAULT);
chimpFilter = cartUsualString(cart, HA_CHIMP, HA_CHIMP_DEFAULT);
chimpQualFilter = 
    sqlUnsigned(cartUsualString(cart, HA_CHIMP_QUAL, HA_CHIMP_QUAL_DEFAULT));
macaqueFilter = cartUsualString(cart, HA_MACAQUE, HA_MACAQUE_DEFAULT);
macaqueQualFilter = 
    sqlUnsigned(cartUsualString(cart, HA_MACAQUE_QUAL, HA_MACAQUE_QUAL_DEFAULT));
}

boolean allDefaults()
/* return TRUE if all filters are set to default values */
{
if (differentString(mixedFilter, HA_POP_MIXED_DEFAULT)) return FALSE;
if (differentString(popCountFilter, HA_POP_COUNT_DEFAULT)) return FALSE;
if (differentString(observedFilter, HA_TYPE_DEFAULT)) return FALSE;
if (minFreqFilter > sqlFloat(HA_MIN_FREQ_DEFAULT)) return FALSE;
if (maxFreqFilter < sqlFloat(HA_MAX_FREQ_DEFAULT)) return FALSE;
if (minHetFilter > sqlFloat(HA_MIN_HET_DEFAULT)) return FALSE;
if (maxFreqFilter < sqlFloat(HA_MAX_HET_DEFAULT)) return FALSE;
if (differentString(monoFilter, HA_MONO_DEFAULT)) return FALSE;
if (differentString(chimpFilter, HA_CHIMP_DEFAULT)) return FALSE;
if (chimpQualFilter > sqlUnsigned(HA_CHIMP_QUAL_DEFAULT)) return FALSE;
if (differentString(macaqueFilter, HA_MACAQUE_DEFAULT)) return FALSE;
if (macaqueQualFilter > sqlUnsigned(HA_MACAQUE_QUAL_DEFAULT)) return FALSE;
return TRUE;
}



static void hapmapLoadSimple(struct track *tg)
/* load data, no filters */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 1;

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

boolean orthoMatchSummary(struct hapmapAllelesOrtho *orthoItem, 
                          struct hapmapAllelesSummary *summaryItem)
/* compare an ortho item to a summary item */
/* return FALSE if not matching */
/* probably could use bedCmp to do this */
{
/* don't check chrom, that must match */
if (orthoItem->chromStart != summaryItem->chromStart) return FALSE;
if (orthoItem->chromEnd != summaryItem->chromEnd) return FALSE;
if (differentString(orthoItem->name, summaryItem->name)) return FALSE;
return TRUE;
}

boolean simpleMatchSummary(struct hapmapAlleles *simpleItem, 
                           struct hapmapAllelesSummary *summaryItem)
/* compare a simple item to a summary item */
/* return FALSE if not matching */
/* probably could use bedCmp to do this */
{
/* don't check chrom, that must match */
if (simpleItem->chromStart != summaryItem->chromStart) return FALSE;
if (simpleItem->chromEnd != summaryItem->chromEnd) return FALSE;
if (differentString(simpleItem->name, summaryItem->name)) return FALSE;
return TRUE;
}

float getMinFreq(struct hapmapAllelesSummary *summaryItem)
{
float freqCEU = 0.0;
float freqCHB = 0.0;
float freqJPT = 0.0;
float freqYRI = 0.0;
float ret = 0.0;

if (summaryItem->totalAlleleCountCEU > 0)
    {
    freqCEU = summaryItem->totalAlleleCountCEU - summaryItem->majorAlleleCountCEU;
    freqCEU = freqCEU / summaryItem->totalAlleleCountCEU;
    }
if (summaryItem->totalAlleleCountCHB > 0)
    {
    freqCHB = summaryItem->totalAlleleCountCHB - summaryItem->majorAlleleCountCHB;
    freqCHB = freqCHB / summaryItem->totalAlleleCountCHB;
    }
if (summaryItem->totalAlleleCountJPT > 0)
    {
    freqJPT = summaryItem->totalAlleleCountJPT - summaryItem->majorAlleleCountJPT;
    freqJPT = freqJPT / summaryItem->totalAlleleCountJPT;
    }
if (summaryItem->totalAlleleCountYRI > 0)
    {
    freqYRI = summaryItem->totalAlleleCountYRI - summaryItem->majorAlleleCountYRI;
    freqYRI = freqYRI / summaryItem->totalAlleleCountYRI;
    }
ret = freqCEU;
if (freqCHB < ret) ret = freqCHB;
if (freqJPT < ret) ret = freqJPT;
if (freqYRI < ret) ret = freqYRI;
return ret;
}

float getMaxFreq(struct hapmapAllelesSummary *summaryItem)
{
float freqCEU = 0.0;
float freqCHB = 0.0;
float freqJPT = 0.0;
float freqYRI = 0.0;
float ret = 0.0;

if (summaryItem->totalAlleleCountCEU > 0)
    {
    freqCEU = summaryItem->totalAlleleCountCEU - summaryItem->majorAlleleCountCEU;
    freqCEU = freqCEU / summaryItem->totalAlleleCountCEU;
    }
if (summaryItem->totalAlleleCountCHB > 0)
    {
    freqCHB = summaryItem->totalAlleleCountCHB - summaryItem->majorAlleleCountCHB;
    freqCHB = freqCHB / summaryItem->totalAlleleCountCHB;
    }
if (summaryItem->totalAlleleCountJPT > 0)
    {
    freqJPT = summaryItem->totalAlleleCountJPT - summaryItem->majorAlleleCountJPT;
    freqJPT = freqJPT / summaryItem->totalAlleleCountJPT;
    }
if (summaryItem->totalAlleleCountYRI > 0)
    {
    freqYRI = summaryItem->totalAlleleCountYRI - summaryItem->majorAlleleCountYRI;
    freqYRI = freqYRI / summaryItem->totalAlleleCountYRI;
    }
ret = freqCEU;
if (freqCHB > ret) ret = freqCHB;
if (freqJPT > ret) ret = freqJPT;
if (freqYRI > ret) ret = freqYRI;
return ret;
}

int getMonoCount(struct hapmapAllelesSummary *summaryItem)
/* return count of monomorphic populations */
{
int count = 0;
if (summaryItem->totalAlleleCountCEU > 0 && 
    summaryItem->majorAlleleCountCEU == summaryItem->totalAlleleCountCEU)
    count++;
if (summaryItem->totalAlleleCountCHB > 0 && 
    summaryItem->majorAlleleCountCHB == summaryItem->totalAlleleCountCHB)
    count++;
if (summaryItem->totalAlleleCountJPT > 0 && 
    summaryItem->majorAlleleCountJPT == summaryItem->totalAlleleCountJPT)
    count++;
if (summaryItem->totalAlleleCountYRI > 0 && 
    summaryItem->majorAlleleCountYRI == summaryItem->totalAlleleCountYRI)
    count++;
return count;
}

char *getMajorAllele(struct hapmapAllelesSummary *summaryItem)
{
if (sameString(summaryItem->isMixed, "YES"))
    return NULL;
if (summaryItem->totalAlleleCountCEU > 0)
    return summaryItem->majorAlleleCEU;
if (summaryItem->totalAlleleCountCHB > 0)
    return summaryItem->majorAlleleCHB;
if (summaryItem->totalAlleleCountJPT > 0)
    return summaryItem->majorAlleleJPT;
return summaryItem->majorAlleleYRI;
}

char *getMinorAllele(struct hapmapAllelesSummary *summaryItem, char *majorAllele)
{
if (!majorAllele) return NULL;
if (sameString(summaryItem->isMixed, "YES"))
    return NULL;
if (sameString(summaryItem->allele1, majorAllele))
    return summaryItem->allele2;
return summaryItem->allele1;

}


boolean filterOne(struct hapmapAllelesSummary *summaryItem)
/* return TRUE if summaryItem matches any filters */
{
boolean complexObserved = isComplexObserved(summaryItem->observed);
boolean transitionObserved = isTransitionObserved(summaryItem->observed);

if (sameString(mixedFilter, "only mixed") && sameString(summaryItem->isMixed, "NO")) 
    return TRUE;
if (sameString(mixedFilter, "not mixed") && sameString(summaryItem->isMixed, "YES")) 
    return TRUE;

if (sameString(popCountFilter, "all 4 populations") && summaryItem->popCount != 4) 
    return TRUE;
if (sameString(popCountFilter, "1-3 populations") && summaryItem->popCount == 4)
    return TRUE;

if (sameString(observedFilter, "complex") && !complexObserved) return TRUE;
if (sameString(observedFilter, "bi-alleleic") && complexObserved) return TRUE;
if (sameString(observedFilter, "transition") && complexObserved) return TRUE;
if (sameString(observedFilter, "transition") && !transitionObserved) return TRUE;
if (sameString(observedFilter, "transversion") && complexObserved) return TRUE;
if (sameString(observedFilter, "transversion") && transitionObserved) return TRUE;

float minFreq = getMinFreq(summaryItem);
if (minFreq/1000.0 < minFreqFilter) return TRUE;
float maxFreq = getMaxFreq(summaryItem);
if (maxFreq/1000.0 > maxFreqFilter) return TRUE;

if (summaryItem->score/1000.0 < minHetFilter) return TRUE;
if (summaryItem->score/1000.0 > maxHetFilter) return TRUE;

int monoCount = getMonoCount(summaryItem);
if (sameString(monoFilter, "all") && monoCount < summaryItem->popCount) return TRUE;
if (sameString(monoFilter, "some") && monoCount == summaryItem->popCount) return TRUE;
if (sameString(monoFilter, "some") && monoCount == 0) return TRUE;
if (sameString(monoFilter, "none") && monoCount > 0) return TRUE;

if (sameString(chimpFilter, "available") && sameString(summaryItem->chimpAllele, "none")) 
	return TRUE;
if (sameString(chimpFilter, "available") && sameString(summaryItem->chimpAllele, "N")) 
	return TRUE;

/* If mixed, then we can't compare ortho allele. */
if (sameString(chimpFilter, "matches major allele") && summaryItem->isMixed) 
    return TRUE;
if (sameString(chimpFilter, "matches minor allele") && summaryItem->isMixed) 
    return TRUE;
if (sameString(chimpFilter, "complex") && summaryItem->isMixed) 
    return TRUE;

char *majorAllele = getMajorAllele(summaryItem);
if (sameString(chimpFilter, "matches major allele") && differentString(summaryItem->chimpAllele, majorAllele)) 
    return TRUE;
char *minorAllele = getMinorAllele(summaryItem, majorAllele);
/* if monomorphic, minorAllele will be "none" */
if (sameString(chimpFilter, "matches minor allele") && sameString(minorAllele, "none")) 
    return TRUE;
if (sameString(chimpFilter, "matches minor allele") && differentString(summaryItem->chimpAllele, minorAllele)) 
    return TRUE;
/* complex means doesn't match major or minor allele */
if (sameString(chimpFilter, "complex") && sameString(summaryItem->chimpAllele, majorAllele)) 
    return TRUE;
if (sameString(chimpFilter, "complex") && sameString(summaryItem->chimpAllele, minorAllele) &&
    differentString(minorAllele, "none")) 
    return TRUE;

if (summaryItem->chimpAlleleQuality < chimpQualFilter) 
    return TRUE;

if (sameString(chimpFilter, "available") && sameString(summaryItem->chimpAllele, "none")) 
    return TRUE;
if (sameString(chimpFilter, "available") && sameString(summaryItem->chimpAllele, "N")) 
    return TRUE;

/* same filters for macaque as for chimp */
if (sameString(macaqueFilter, "available") && sameString(summaryItem->macaqueAllele, "none")) 
    return TRUE;
if (sameString(macaqueFilter, "available") && sameString(summaryItem->macaqueAllele, "N")) 
    return TRUE;

/* If mixed, then we can't compare ortho allele. */
if (sameString(macaqueFilter, "matches major allele") && summaryItem->isMixed) 
    return TRUE;
if (sameString(macaqueFilter, "matches minor allele") && summaryItem->isMixed) 
    return TRUE;
if (sameString(macaqueFilter, "complex") && summaryItem->isMixed) 
    return TRUE;

majorAllele = getMajorAllele(summaryItem);
if (sameString(macaqueFilter, "matches major allele") && differentString(summaryItem->macaqueAllele, majorAllele)) 
    return TRUE;
minorAllele = getMinorAllele(summaryItem, majorAllele);
/* if monomorphic, minorAllele will be "none" */
if (sameString(macaqueFilter, "matches minor allele") && sameString(minorAllele, "none")) 
    return TRUE;
if (sameString(macaqueFilter, "matches minor allele") && differentString(summaryItem->macaqueAllele, minorAllele)) 
    return TRUE;
/* complex means doesn't match major or minor allele */
if (sameString(macaqueFilter, "complex") && sameString(summaryItem->macaqueAllele, majorAllele)) 
    return TRUE;
if (sameString(macaqueFilter, "complex") && sameString(summaryItem->macaqueAllele, minorAllele) &&
    differentString(minorAllele, "none")) 
    return TRUE;

if (summaryItem->macaqueAlleleQuality < macaqueQualFilter) 
    return TRUE;

return FALSE;
}


struct hapmapAllelesOrtho *filterOrthoList(struct hapmapAllelesOrtho *orthoList, 
                                           struct hapmapAllelesSummary *summaryList)
/* delete from orthoList based on filters */
/* could use slList and combine filterOrthoList() with filterSimpleList() */
{
struct hapmapAllelesOrtho *orthoItem = orthoList;
struct hapmapAllelesSummary *summaryItem = summaryList;
struct hapmapAllelesOrtho *orthoItemClone = NULL;
struct hapmapAllelesOrtho *ret = NULL;

while (orthoItem)
    {
    if (!orthoMatchSummary(orthoItem, summaryItem))
       {
       summaryItem = summaryItem->next;
       continue;
       }
    if (!filterOne(summaryItem))
        {
	orthoItemClone = CloneVar(orthoItem);
	orthoItemClone->next = NULL;
        slSafeAddHead(&ret, orthoItemClone);
	}
    orthoItem = orthoItem->next;
    summaryItem = summaryItem->next;
    }
slSort(&ret, bedCmp);
return ret;
}

struct hapmapAlleles *filterSimpleList(struct hapmapAlleles *simpleList, 
                                       struct hapmapAllelesSummary *summaryList)
/* delete from simpleList based on filters */
/* could use slList and combine filterOrthoList() with filterSimpleList() */
{
struct hapmapAlleles *simpleItem = simpleList;
struct hapmapAllelesSummary *summaryItem = summaryList;
struct hapmapAlleles *simpleItemClone = NULL;
struct hapmapAlleles *ret = NULL;

while (simpleItem)
    {
    if (!simpleMatchSummary(simpleItem, summaryItem))
       {
       summaryItem = summaryItem->next;
       continue;
       }
    if (!filterOne(summaryItem))
        {
	simpleItemClone = CloneVar(simpleItem);
	simpleItemClone->next = NULL;
        slSafeAddHead(&ret, simpleItemClone);
	}
    simpleItem = simpleItem->next;
    summaryItem = summaryItem->next;
    }
slSort(&ret, bedCmp);
return ret;
}



static void hapmapLoad(struct track *tg)
/* load filtered data */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset;

loadFilters();

if (allDefaults()) 
    {
    hapmapLoadSimple(tg);
    return;
    }

/* first load summary data */
struct hapmapAllelesSummary *summaryLoadItem, *summaryItemList = NULL;
sr = hRangeQuery(conn, "hapmapAllelesSummary", chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    summaryLoadItem = hapmapAllelesSummaryLoad(row+rowOffset);
    slAddHead(&summaryItemList, summaryLoadItem);
    }
sqlFreeResult(&sr);
slSort(&summaryItemList, bedCmp);

if (sameString(tg->mapName, "hapmapAllelesChimp") || sameString(tg->mapName, "hapmapAllelesMacaque"))
{
    struct hapmapAllelesOrtho *orthoLoadItem, *orthoItemList = NULL, *orthoItemsFiltered = NULL;
    sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        orthoLoadItem = hapmapAllelesOrthoLoad(row+rowOffset);
        slAddHead(&orthoItemList, orthoLoadItem);
        }
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    slSort(&orthoItemList, bedCmp);
    orthoItemsFiltered = filterOrthoList(orthoItemList, summaryItemList);
    tg->items = orthoItemsFiltered;
    return;
}

struct hapmapAlleles *simpleLoadItem, *simpleItemList = NULL, *simpleItemsFiltered = NULL;
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    simpleLoadItem = hapmapAllelesLoad(row+rowOffset);
    slAddHead(&simpleItemList, simpleLoadItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&simpleItemList, bedCmp);
simpleItemsFiltered = filterSimpleList(simpleItemList, summaryItemList);
tg->items = simpleItemsFiltered;
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

