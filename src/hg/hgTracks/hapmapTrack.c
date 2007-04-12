/* hapmapTrack - Handle HapMap track. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"

#include "hapmapSnps.h"
#include "hapmapAllelesOrtho.h"
#include "hapmapAllelesSummary.h"

char *mixedFilter = HAP_POP_MIXED_DEFAULT;
char *popCountFilter = HAP_POP_COUNT_DEFAULT;
char *observedFilter = HAP_TYPE_DEFAULT;
float minFreqFilter = 0.0;
float maxFreqFilter = 0.5;
float minHetFilter = 0.0;
float maxHetFilter = 0.5;
char *monoFilter = HAP_MONO_DEFAULT;
char *chimpFilter = HAP_CHIMP_DEFAULT;
int chimpQualFilter = 0;
char *macaqueFilter = HAP_MACAQUE_DEFAULT;
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
mixedFilter = cartUsualString(cart, HAP_POP_MIXED, HAP_POP_MIXED_DEFAULT);
popCountFilter = cartUsualString(cart, HAP_POP_COUNT, HAP_POP_COUNT_DEFAULT);
observedFilter = cartUsualString(cart, HAP_TYPE, HAP_TYPE_DEFAULT);
minFreqFilter = sqlFloat(cartUsualString(cart, HAP_MIN_FREQ, HAP_MIN_FREQ_DEFAULT));
if (minFreqFilter < 0.0) 
    {
    minFreqFilter = 0.0;
    cartSetDouble(cart, HAP_MIN_FREQ, 0.0);
    }
maxFreqFilter = sqlFloat(cartUsualString(cart, HAP_MAX_FREQ, HAP_MAX_FREQ_DEFAULT));
if (maxFreqFilter > 0.5) 
    {
    maxFreqFilter = 0.5;
    cartSetDouble(cart, HAP_MAX_FREQ, 0.5);
    }
minHetFilter = sqlFloat(cartUsualString(cart, HAP_MIN_HET, HAP_MIN_HET_DEFAULT));
if (minHetFilter < 0.0) 
    {
    minHetFilter = 0.0;
    cartSetDouble(cart, HAP_MIN_HET, 0.0);
    }
maxHetFilter = sqlFloat(cartUsualString(cart, HAP_MAX_HET, HAP_MAX_HET_DEFAULT));
if (maxHetFilter > 0.5) 
    {
    maxHetFilter = 0.5;
    cartSetDouble(cart, HAP_MAX_HET, 0.5);
    }
monoFilter = cartUsualString(cart, HAP_MONO, HAP_MONO_DEFAULT);
chimpFilter = cartUsualString(cart, HAP_CHIMP, HAP_CHIMP_DEFAULT);
chimpQualFilter = 
    sqlUnsigned(cartUsualString(cart, HAP_CHIMP_QUAL, HAP_CHIMP_QUAL_DEFAULT));
if (chimpQualFilter < 0) 
    {
    chimpQualFilter = 0;
    cartSetInt(cart, HAP_CHIMP_QUAL, 0);
    }
if (chimpQualFilter > 100) 
    {
    chimpQualFilter = 100;
    cartSetInt(cart, HAP_CHIMP_QUAL, 100);
    }

macaqueFilter = cartUsualString(cart, HAP_MACAQUE, HAP_MACAQUE_DEFAULT);
macaqueQualFilter = 
    sqlUnsigned(cartUsualString(cart, HAP_MACAQUE_QUAL, HAP_MACAQUE_QUAL_DEFAULT));
if (macaqueQualFilter < 0) 
    {
    macaqueQualFilter = 0;
    cartSetInt(cart, HAP_MACAQUE_QUAL, 0);
    }
if (macaqueQualFilter > 100) 
    {
    macaqueQualFilter = 100;
    cartSetInt(cart, HAP_MACAQUE_QUAL, 100);
    }
}

boolean allDefaults()
/* return TRUE if all filters are set to default values */
{
if (differentString(mixedFilter, HAP_POP_MIXED_DEFAULT)) return FALSE;
if (differentString(popCountFilter, HAP_POP_COUNT_DEFAULT)) return FALSE;
if (differentString(observedFilter, HAP_TYPE_DEFAULT)) return FALSE;
if (minFreqFilter > sqlFloat(HAP_MIN_FREQ_DEFAULT)) return FALSE;
if (maxFreqFilter < sqlFloat(HAP_MAX_FREQ_DEFAULT)) return FALSE;
if (minHetFilter > sqlFloat(HAP_MIN_HET_DEFAULT)) return FALSE;
if (maxFreqFilter < sqlFloat(HAP_MAX_HET_DEFAULT)) return FALSE;
if (differentString(monoFilter, HAP_MONO_DEFAULT)) return FALSE;
if (differentString(chimpFilter, HAP_CHIMP_DEFAULT)) return FALSE;
if (chimpQualFilter > sqlUnsigned(HAP_CHIMP_QUAL_DEFAULT)) return FALSE;
if (differentString(macaqueFilter, HAP_MACAQUE_DEFAULT)) return FALSE;
if (macaqueQualFilter > sqlUnsigned(HAP_MACAQUE_QUAL_DEFAULT)) return FALSE;
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

struct hapmapSnps *simpleLoadItem, *simpleItemList = NULL;
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    simpleLoadItem = hapmapSnpsLoad(row+rowOffset);
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

boolean simpleMatchSummary(struct hapmapSnps *simpleItem, 
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
float freqCEU = 0.5;
float freqCHB = 0.5;
float freqJPT = 0.5;
float freqYRI = 0.5;
float ret = 0.5;

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

boolean orthoAlleleCheck(struct hapmapAllelesSummary *summaryItem, char *species)
/* return TRUE if disqualified from being ortho mismatch */
{
char *orthoAllele = NULL;

if (sameString(species, "chimp"))
    orthoAllele = cloneString(summaryItem->chimpAllele);
else if (sameString(species, "macaque"))
    orthoAllele = cloneString(summaryItem->macaqueAllele);
else
    return TRUE;

if (isComplexObserved(summaryItem->observed)) 
    {
    if (sameString(orthoAllele, summaryItem->allele1)) return TRUE;
    /* monomorphic are disqualified due to insufficient info; can't prove this is an ortho mismatch */
    if (sameString(summaryItem->allele2, "none")) return TRUE;
    if (sameString(orthoAllele, summaryItem->allele2)) return TRUE;
    }

/* simple case: check for match in observed string */
/* this is inclusive of monomorphic */
char *subString = strstr(summaryItem->observed, orthoAllele);
if (subString) return TRUE;

return FALSE;

}


boolean filterOne(struct hapmapAllelesSummary *summaryItem)
/* return TRUE if summaryItem is disqualified for any reason */
{
boolean complexObserved = isComplexObserved(summaryItem->observed);
boolean transitionObserved = isTransitionObserved(summaryItem->observed);

if (sameString(mixedFilter, "mixed") && sameString(summaryItem->isMixed, "NO")) 
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

if (minFreqFilter > sqlFloat(HAP_MIN_FREQ_DEFAULT))
    {
    /* disqualify mixed items when freq filter is set */
    if (sameString(summaryItem->isMixed, "YES")) return TRUE;
    float minFreq = getMinFreq(summaryItem);
    if (minFreq < minFreqFilter) return TRUE;
    }

if (maxFreqFilter < sqlFloat(HAP_MAX_FREQ_DEFAULT))
    {
    /* disqualify mixed items when freq filter is set */
    if (sameString(summaryItem->isMixed, "YES")) return TRUE;
    float maxFreq = getMaxFreq(summaryItem);
    if (maxFreq > maxFreqFilter) return TRUE;
    }

if (summaryItem->score/1000.0 < minHetFilter) return TRUE;
if (summaryItem->score/1000.0 > maxHetFilter) return TRUE;

int monoCount = getMonoCount(summaryItem);
if (sameString(monoFilter, "all populations") && monoCount < summaryItem->popCount) return TRUE;
if (sameString(monoFilter, "some populations") && monoCount == summaryItem->popCount) return TRUE;
if (sameString(monoFilter, "some populations") && monoCount == 0) return TRUE;
if (sameString(monoFilter, "no populations") && monoCount > 0) return TRUE;

/* orthoAlleles */
/* if ortho data not available, then filters aren't used */
if (differentString(chimpFilter, "no filter") && sameString(summaryItem->chimpAllele, "none"))
    return FALSE;
if (differentString(macaqueFilter, "no filter") && sameString(summaryItem->macaqueAllele, "none")) 
    return FALSE;
if (differentString(chimpFilter, "no filter") && sameString(summaryItem->chimpAllele, "N")) 
    return FALSE;
if (differentString(macaqueFilter, "no filter") && sameString(summaryItem->macaqueAllele, "N")) 
    return FALSE;

/* handle mismatches first - interesting and tricky */
if (sameString(chimpFilter, "matches neither human allele")) return orthoAlleleCheck(summaryItem, "chimp");
if (sameString(macaqueFilter, "matches neither human allele")) return orthoAlleleCheck(summaryItem, "macaque");

/* If mixed, then we can't compare ortho allele. */
if (sameString(chimpFilter, "matches major human allele") && sameString(summaryItem->isMixed, "YES")) 
    return FALSE;
if (sameString(macaqueFilter, "matches major human allele") && sameString(summaryItem->isMixed, "YES")) 
    return FALSE;
if (sameString(chimpFilter, "matches minor human allele") && sameString(summaryItem->isMixed, "YES")) 
    return FALSE;
if (sameString(macaqueFilter, "matches minor human allele") && sameString(summaryItem->isMixed, "YES")) 
    return FALSE;

char *majorAllele = getMajorAllele(summaryItem);
if (sameString(chimpFilter, "matches major human allele") && differentString(summaryItem->chimpAllele, majorAllele)) 
    return TRUE;
if (sameString(macaqueFilter, "matches major human allele") && differentString(summaryItem->macaqueAllele, majorAllele)) 
    return TRUE;

/* can't filter if allele2 is "none" */
if (sameString(chimpFilter, "matches minor human allele") && sameString(summaryItem->allele2, "none"))
    return FALSE;
if (sameString(macaqueFilter, "matches minor human allele") && sameString(summaryItem->allele2, "none"))
    return FALSE;

char *minorAllele = getMinorAllele(summaryItem, majorAllele);
if (sameString(chimpFilter, "matches minor human allele") && differentString(summaryItem->chimpAllele, minorAllele)) 
    return TRUE;
if (sameString(macaqueFilter, "matches minor human allele") && differentString(summaryItem->macaqueAllele, minorAllele)) 
    return TRUE;

if (summaryItem->chimpAlleleQuality < chimpQualFilter) return TRUE;
if (summaryItem->macaqueAlleleQuality < macaqueQualFilter) return TRUE;

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

struct hapmapSnps *filterSimpleList(struct hapmapSnps *simpleList, 
                                    struct hapmapAllelesSummary *summaryList)
/* delete from simpleList based on filters */
/* could use slList and combine filterOrthoList() with filterSimpleList() */
{
struct hapmapSnps *simpleItem = simpleList;
struct hapmapAllelesSummary *summaryItem = summaryList;
struct hapmapSnps *simpleItemClone = NULL;
struct hapmapSnps *ret = NULL;

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

struct hapmapSnps *simpleLoadItem, *simpleItemList = NULL, *simpleItemsFiltered = NULL;
sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    simpleLoadItem = hapmapSnpsLoad(row+rowOffset);
    slAddHead(&simpleItemList, simpleLoadItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&simpleItemList, bedCmp);
simpleItemsFiltered = filterSimpleList(simpleItemList, summaryItemList);
tg->items = simpleItemsFiltered;
}

void hapmapDrawAt(struct track *tg, void *item, struct vGfx *vg, int xOff, int y, double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw the hapmap snps at a given position. */
/* Display allele when zoomed to base level. */
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

if (sameString(tg->mapName, "hapmapAllelesChimp") || sameString(tg->mapName, "hapmapAllelesMacaque"))
    {
    struct hapmapAllelesOrtho *thisItem = item;
    chromStart = thisItem->chromStart;
    chromEnd = thisItem->chromEnd;
    allele = cloneString(thisItem->orthoAllele);
    strand = cloneString(thisItem->strand);
    }
else
    {
    struct hapmapSnps *thisItem = item;
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
int gradient = 0;

if (sameString(tg->mapName, "hapmapAllelesChimp") || sameString(tg->mapName, "hapmapAllelesMacaque"))
    {
    struct hapmapAllelesOrtho *thisItem = item;
    gradient = grayInRange(thisItem->score, 0, 100);
    col = shadesOfBrown[gradient];
    return col;
    }

struct hapmapSnps *thisItem = item;
/* Could also use score, since I've set that based on minor allele frequency. */
totalCount = thisItem->homoCount1 + thisItem->homoCount2;
minorCount = min(thisItem->homoCount1, thisItem->homoCount2);
gradient = grayInRange(minorCount, 0, totalCount/2);

if (gradient < maxShade)
    gradient++;
if (gradient < maxShade)
    gradient++;
col = shadesOfGray[gradient];
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

