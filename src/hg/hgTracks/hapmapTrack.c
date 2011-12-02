/* hapmapTrack - Handle HapMap track. */

#include "common.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"

#include "hapmapSnps.h"
#include "hapmapAllelesOrtho.h"
#include "hapmapAllelesSummary.h"
#include "hapmapPhaseIIISummary.h"


// These values are all overwritten in loadFilters() below.
char *mixedFilter = HAP_FILTER_DEFAULT;
char *popCountFilter = HAP_FILTER_DEFAULT;
char *observedFilter = HAP_FILTER_DEFAULT;
float minFreqFilter = 0.0;
float maxFreqFilter = 0.5;
float minHetFilter = 0.0;
float maxHetFilter = 0.5; // 1.0 for observed het in PhaseIII version of the track.

char *monoFilter[HAP_PHASEIII_POPCOUNT];
// Indices of HapMap PhaseII pops in there:
#define CEU 1
#define CHB 2
#define JPT 5
#define YRI 10

/* Someday these should come from a trackDb setting: */
char *orthoFilter[HAP_ORTHO_COUNT];
int orthoQualFilter[HAP_ORTHO_COUNT];
#define CHIMP 0
#define MACAQUE 1

static boolean isTransitionObserved(char *observed)
{
if (sameString(observed, "A/G")) return TRUE;
if (sameString(observed, "C/T")) return TRUE;
return FALSE;
}

static boolean isComplexObserved(char *observed)
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

/* This maps new cart variable names to old, for backwards compatibility: */
static char *cartVarNameUpdates[16][2] =
    { { HAP_POP_MIXED, "hapmapSnps.isMixed" },
      { HAP_POP_COUNT, "hapmapSnps.popCount" },
      { HAP_TYPE, "hapmapSnps.polyType" },
      { HAP_MIN_FREQ, "hapmapSnps.minorAlleleFreqMinimum" },
      { HAP_MAX_FREQ, "hapmapSnps.minorAlleleFreqMaximum" },
      { HAP_MIN_HET, "hapmapSnps.hetMinimum" },
      { HAP_MAX_EXPECTED_HET, "hapmapSnps_hetMaximum" },
      { HAP_MAX_EXPECTED_HET, "hapmapSnps.hetMaximum" },
      { HAP_MONO_PREFIX "_CEU", "hapmapSnps.monomorphic.ceu" },
      { HAP_MONO_PREFIX "_CHB", "hapmapSnps.monomorphic.chb" },
      { HAP_MONO_PREFIX "_JPT", "hapmapSnps.monomorphic.jpt" },
      { HAP_MONO_PREFIX "_YRI", "hapmapSnps.monomorphic.yri" },
      { HAP_ORTHO_PREFIX "_Chimp", "hapmapSnps.chimp" },
      { HAP_ORTHO_QUAL_PREFIX "_Chimp", "hapmapSnps.chimpQualScore" },
      { HAP_ORTHO_PREFIX "_Macaque", "hapmapSnps.macaque" },
      { HAP_ORTHO_QUAL_PREFIX "_Macaque", "hapmapSnps.macaqueQualScore" },
    };

static void updateOldCartVars()
/* Detect old names for cart variables, in case they are still hanging around
 * in people's carts.  When found, set the new var to the old val unless it 
 * would clobber something specifically set for the new var.  Delete old 
 * cart var name. */
{
int i;
for (i = 0;  i < ArraySize(cartVarNameUpdates);  i++)
    {
    char *newName = cartVarNameUpdates[i][0];
    char *oldName = cartVarNameUpdates[i][1];
    char *oldVal = cartOptionalString(cart, oldName);
    if (oldVal != NULL)
	{
	if (cartOptionalString(cart, newName) == NULL)
	    cartSetString(cart, newName, oldVal);
	cartRemove(cart, oldName);
	}
    }
}

static void loadFilters(boolean isPhaseIII)
/* Translate filter cart settings into global variables. */
{
updateOldCartVars();
mixedFilter = cartUsualString(cart, HAP_POP_MIXED, HAP_FILTER_DEFAULT);
popCountFilter = cartUsualString(cart, HAP_POP_COUNT, HAP_FILTER_DEFAULT);
observedFilter = cartUsualString(cart, HAP_TYPE, HAP_FILTER_DEFAULT);
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
if (isPhaseIII)
    {
    maxHetFilter = sqlFloat(cartUsualString(cart, HAP_MAX_OBSERVED_HET,
					    HAP_MAX_OBSERVED_HET_DEFAULT));
    float defaultMaxHetF = atof(HAP_MAX_OBSERVED_HET_DEFAULT);
    if (maxHetFilter > defaultMaxHetF)
	{
	maxHetFilter = defaultMaxHetF;
	cartSetDouble(cart, HAP_MAX_OBSERVED_HET, defaultMaxHetF);
	}
    }
else
    {
    maxHetFilter = sqlFloat(cartUsualString(cart, HAP_MAX_EXPECTED_HET,
					    HAP_MAX_EXPECTED_HET_DEFAULT));
    float defaultMaxHetF = atof(HAP_MAX_EXPECTED_HET_DEFAULT);
    if (maxHetFilter > defaultMaxHetF)
	{
	maxHetFilter = defaultMaxHetF;
	cartSetDouble(cart, HAP_MAX_EXPECTED_HET, defaultMaxHetF);
	}
    }
char cartVar[128];
int i;
for (i = 0;  i < HAP_PHASEIII_POPCOUNT;  i++)
    {
    safef(cartVar, sizeof(cartVar), "%s_%s", HAP_MONO_PREFIX, hapmapPhaseIIIPops[i]);
    monoFilter[i] = cartUsualString(cart, cartVar, HAP_FILTER_DEFAULT);
    }
for (i = 0; i < HAP_ORTHO_COUNT;  i++)
    {
    safef(cartVar, sizeof(cartVar), "%s_%s", HAP_ORTHO_PREFIX, hapmapOrthoSpecies[i]);
    orthoFilter[i] = cartUsualString(cart, cartVar, HAP_FILTER_DEFAULT);
    safef(cartVar, sizeof(cartVar), "%s_%s", HAP_ORTHO_QUAL_PREFIX, hapmapOrthoSpecies[i]);
    orthoQualFilter[i] = cartUsualInt(cart, cartVar, atoi(HAP_ORTHO_QUAL_DEFAULT));
    if (orthoQualFilter[i] < 0)
	{
	orthoQualFilter[i] = 0;
	cartSetInt(cart, cartVar, 0);
	}
    if (orthoQualFilter[i] > 100)
	{
	orthoQualFilter[i] = 100;
	cartSetInt(cart, cartVar, 100);
	}
    }
}

static boolean allDefaults(boolean isPhaseIII)
/* return TRUE if all filters are set to default values */
{
if (differentString(mixedFilter, HAP_FILTER_DEFAULT)) return FALSE;
if (differentString(popCountFilter, HAP_FILTER_DEFAULT)) return FALSE;
if (differentString(observedFilter, HAP_FILTER_DEFAULT)) return FALSE;
if (minFreqFilter > sqlFloat(HAP_MIN_FREQ_DEFAULT)) return FALSE;
if (maxFreqFilter < sqlFloat(HAP_MAX_FREQ_DEFAULT)) return FALSE;
if (minHetFilter > sqlFloat(HAP_MIN_HET_DEFAULT)) return FALSE;
char *defaultMaxHet = isPhaseIII ? HAP_MAX_OBSERVED_HET_DEFAULT : HAP_MAX_EXPECTED_HET_DEFAULT;
if (maxHetFilter < sqlFloat(defaultMaxHet)) return FALSE;
int i;
for (i = 0;  i < HAP_PHASEIII_POPCOUNT;  i++)
    if (differentString(monoFilter[i], HAP_FILTER_DEFAULT)) return FALSE;
for (i = 0;  i < HAP_ORTHO_COUNT;  i++)
    {
    if (differentString(orthoFilter[i], HAP_FILTER_DEFAULT)) return FALSE;
    if (orthoQualFilter[i] > sqlUnsigned(HAP_ORTHO_QUAL_DEFAULT)) return FALSE;
    }
return TRUE;
}


static void hapmapLoadSimple(struct track *tg)
/* load data, no filters */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset = 1;
int i;

char orthoTable[HDB_MAX_TABLE_STRING];
for (i = 0;  i < HAP_ORTHO_COUNT;  i++)
    {
    if (endsWith(tg->table, "PhaseII"))
	safef(orthoTable, sizeof(orthoTable), "hapmapAlleles%sPhaseII", hapmapOrthoSpecies[i]);
    else
	safef(orthoTable, sizeof(orthoTable), "hapmapAlleles%s", hapmapOrthoSpecies[i]);
    if (sameString(tg->table, orthoTable))
	{
	struct hapmapAllelesOrtho *orthoLoadItem, *orthoItemList = NULL;
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
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
    }

struct hapmapSnps *simpleLoadItem, *simpleItemList = NULL;
sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
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

boolean orthoMatchSummaryPhaseII(struct hapmapAllelesOrtho *orthoItem, 
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

boolean simpleMatchSummaryPhaseII(struct hapmapSnps *simpleItem, 
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

float getMinFreqPhaseII(struct hapmapAllelesSummary *summaryItem)
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

float getMaxFreqPhaseII(struct hapmapAllelesSummary *summaryItem)
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


char *getMajorAllelePhaseII(struct hapmapAllelesSummary *summaryItem)
{
char *allele = NULL;

if (sameString(summaryItem->isMixed, "YES"))
    return cloneString("none");

if (summaryItem->totalAlleleCountCEU > 0)
    allele = cloneString(summaryItem->majorAlleleCEU);
if (summaryItem->totalAlleleCountCHB > 0)
    allele = cloneString(summaryItem->majorAlleleCHB);
if (summaryItem->totalAlleleCountJPT > 0)
    allele = cloneString(summaryItem->majorAlleleJPT);
if (summaryItem->totalAlleleCountYRI > 0)
    allele = cloneString(summaryItem->majorAlleleYRI);

if (!allele)
    return cloneString("none");
return allele;
}

char *getMinorAllelePhaseII(struct hapmapAllelesSummary *summaryItem, char *majorAllele)
/* return the allele that isn't the majorAllele */
{
char *allele = NULL;
if (!majorAllele) return cloneString("none");
if (sameString(summaryItem->isMixed, "YES"))
    return cloneString("none");
if (sameString(summaryItem->allele1, majorAllele))
    allele = cloneString(summaryItem->allele2);
else
    allele = cloneString(summaryItem->allele1);
if (!allele)
    return cloneString("none");
return allele;
}

boolean orthoAlleleCheckPhaseII(struct hapmapAllelesSummary *summaryItem, char *species)
/* return TRUE if disqualified from being ortho mismatch */
/* orthoAllele is adjusted for strand; population allele is not */
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

static boolean filterObserved(boolean complexObserved, boolean transitionObserved)
/* Return TRUE if observedFilter disqualifies the observed types. */
{
if (sameString(observedFilter, "complex") && !complexObserved) return TRUE;
if (sameString(observedFilter, "bi-alleleic") && complexObserved) return TRUE;
if (sameString(observedFilter, "transition") && complexObserved) return TRUE;
if (sameString(observedFilter, "transition") && !transitionObserved) return TRUE;
if (sameString(observedFilter, "transversion") && complexObserved) return TRUE;
if (sameString(observedFilter, "transversion") && transitionObserved) return TRUE;
return FALSE;
}

static boolean filterFreq(boolean isMixed, float minFreq, float maxFreq)
/* Return TRUE if {min,max}FreqFilter disqualifies item. */
{
if (minFreqFilter > atof(HAP_MIN_FREQ_DEFAULT))
    {
    /* disqualify mixed items when freq filter is set */
    if (isMixed) return TRUE;
    if (minFreq < minFreqFilter) return TRUE;
    }

if (maxFreqFilter < atof(HAP_MAX_FREQ_DEFAULT))
    {
    /* disqualify mixed items when freq filter is set */
    if (isMixed) return TRUE;
    if (maxFreq > maxFreqFilter) return TRUE;
    }
return FALSE;
}


static boolean filterOnePhaseII(struct hapmapAllelesSummary *summaryItem)
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

if (filterObserved(complexObserved, transitionObserved))
    return TRUE;

if (filterFreq(sameString(summaryItem->isMixed, "YES"),
	       getMinFreqPhaseII(summaryItem), getMaxFreqPhaseII(summaryItem)))
    return TRUE;

if (summaryItem->score/1000.0 < minHetFilter) return TRUE;
if (summaryItem->score/1000.0 > maxHetFilter) return TRUE;

if (summaryItem->totalAlleleCountCEU > 0)
    {
    if (sameString(monoFilter[CEU], "yes") && summaryItem->majorAlleleCountCEU != summaryItem->totalAlleleCountCEU) return TRUE;
    if (sameString(monoFilter[CEU], "no") && summaryItem->majorAlleleCountCEU == summaryItem->totalAlleleCountCEU) return TRUE;
    }
if (summaryItem->totalAlleleCountCHB > 0)
    {
    if (sameString(monoFilter[CHB], "yes") && summaryItem->majorAlleleCountCHB != summaryItem->totalAlleleCountCHB) return TRUE;
    if (sameString(monoFilter[CHB], "no") && summaryItem->majorAlleleCountCHB == summaryItem->totalAlleleCountCHB) return TRUE;
    }
if (summaryItem->totalAlleleCountJPT > 0)
    {
    if (sameString(monoFilter[JPT], "yes") && summaryItem->majorAlleleCountJPT != summaryItem->totalAlleleCountJPT) return TRUE;
    if (sameString(monoFilter[JPT], "no") && summaryItem->majorAlleleCountJPT == summaryItem->totalAlleleCountJPT) return TRUE;
    }
if (summaryItem->totalAlleleCountYRI > 0)
    {
    if (sameString(monoFilter[YRI], "yes") && summaryItem->majorAlleleCountYRI != summaryItem->totalAlleleCountYRI) return TRUE;
    if (sameString(monoFilter[YRI], "no") && summaryItem->majorAlleleCountYRI == summaryItem->totalAlleleCountYRI) return TRUE;
    }

/* ortho quality */
if (summaryItem->chimpAlleleQuality < orthoQualFilter[CHIMP]) return TRUE;
if (summaryItem->macaqueAlleleQuality < orthoQualFilter[MACAQUE]) return TRUE;

/* orthoAlleles */
/* if any filter set, data must be available */
if (differentString(orthoFilter[CHIMP], "no filter") && sameString(summaryItem->chimpAllele, "none"))
    return TRUE;
if (differentString(orthoFilter[MACAQUE], "no filter") && sameString(summaryItem->macaqueAllele, "none"))
    return TRUE;
if (differentString(orthoFilter[CHIMP], "no filter") && sameString(summaryItem->chimpAllele, "N"))
    return TRUE;
if (differentString(orthoFilter[MACAQUE], "no filter") && sameString(summaryItem->macaqueAllele, "N")) 
    return TRUE;

/* If mixed, then we can't tell if we have a match, so must filter out. */
if (sameString(orthoFilter[CHIMP], "matches major human allele") && sameString(summaryItem->isMixed, "YES")) 
    return TRUE;
if (sameString(orthoFilter[MACAQUE], "matches major human allele") && sameString(summaryItem->isMixed, "YES")) 
    return TRUE;
if (sameString(orthoFilter[CHIMP], "matches minor human allele") && sameString(summaryItem->isMixed, "YES")) 
    return TRUE;
if (sameString(orthoFilter[MACAQUE], "matches minor human allele") && sameString(summaryItem->isMixed, "YES")) 
    return TRUE;

char *majorAllele = getMajorAllelePhaseII(summaryItem);
if (sameString(orthoFilter[CHIMP], "matches major human allele") && differentString(summaryItem->chimpAllele, majorAllele)) 
    return TRUE;
if (sameString(orthoFilter[MACAQUE], "matches major human allele") && differentString(summaryItem->macaqueAllele, majorAllele)) 
    return TRUE;

char *minorAllele = getMinorAllelePhaseII(summaryItem, majorAllele);
if (sameString(orthoFilter[CHIMP], "matches minor human allele") && differentString(summaryItem->chimpAllele, minorAllele)) 
    return TRUE;
if (sameString(orthoFilter[MACAQUE], "matches minor human allele") && differentString(summaryItem->macaqueAllele, minorAllele)) 
    return TRUE;

/* mismatches */
if (sameString(orthoFilter[CHIMP], "matches neither human allele") && orthoAlleleCheckPhaseII(summaryItem, "chimp"))
    return TRUE;
if (sameString(orthoFilter[MACAQUE], "matches neither human allele") && orthoAlleleCheckPhaseII(summaryItem, "macaque"))
    return TRUE;

return FALSE;
}


struct hapmapAllelesOrtho *filterOrthoListPhaseII(struct hapmapAllelesOrtho *orthoList, 
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
    if (!orthoMatchSummaryPhaseII(orthoItem, summaryItem))
       {
       summaryItem = summaryItem->next;
       continue;
       }
    if (!filterOnePhaseII(summaryItem))
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

struct hapmapSnps *filterSimpleListPhaseII(struct hapmapSnps *simpleList, 
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
    if (!simpleMatchSummaryPhaseII(simpleItem, summaryItem))
       {
       summaryItem = summaryItem->next;
       continue;
       }
    if (!filterOnePhaseII(summaryItem))
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


static void hapmapLoadPhaseII(struct track *tg)
/* load filtered data from HapMap PhaseII (4 populations) */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr = NULL;
char **row = NULL;
int rowOffset;
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

int i;
char orthoTable[HDB_MAX_TABLE_STRING];
for (i = 0;  i < HAP_ORTHO_COUNT;  i++)
    {
    if (endsWith(tg->table, "PhaseII"))
	safef(orthoTable, sizeof(orthoTable), "hapmapAlleles%sPhaseII", hapmapOrthoSpecies[i]);
    else
	safef(orthoTable, sizeof(orthoTable), "hapmapAlleles%s", hapmapOrthoSpecies[i]);
    if (sameString(tg->table, orthoTable))
	{
	struct hapmapAllelesOrtho *orthoLoadItem, *orthoItemList = NULL;
	struct hapmapAllelesOrtho *orthoItemsFiltered = NULL;
	sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    orthoLoadItem = hapmapAllelesOrthoLoad(row+rowOffset);
	    slAddHead(&orthoItemList, orthoLoadItem);
	    }
	sqlFreeResult(&sr);
	hFreeConn(&conn);
	slSort(&orthoItemList, bedCmp);
	orthoItemsFiltered = filterOrthoListPhaseII(orthoItemList, summaryItemList);
	tg->items = orthoItemsFiltered;
	return;
	}
    }

struct hapmapSnps *simpleLoadItem, *simpleItemList = NULL, *simpleItemsFiltered = NULL;
sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    simpleLoadItem = hapmapSnpsLoad(row+rowOffset);
    slAddHead(&simpleItemList, simpleLoadItem);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&simpleItemList, bedCmp);
simpleItemsFiltered = filterSimpleListPhaseII(simpleItemList, summaryItemList);
tg->items = simpleItemsFiltered;
}

struct hash *loadSummaryHashPhaseIII(struct sqlConnection *conn)
/* Load PhaseIII summary info into a hash (key = SNP rsId). */
{
struct hash *summaryHash = hashNew(14);
char **row;
int rowOffset;
struct sqlResult *sr = hRangeQuery(conn, "hapmapPhaseIIISummary",
				   chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct hapmapPhaseIIISummary *s = hapmapPhaseIIISummaryLoad(row+rowOffset);
    hashAdd(summaryHash, s->name, s);
    }
sqlFreeResult(&sr);
return summaryHash;
}

boolean filterOnePhaseIII(struct hapmapPhaseIIISummary *summary)
/* return TRUE if summary is disqualified for any reason */
{
boolean complexObserved = isComplexObserved(summary->observed);
boolean transitionObserved = isTransitionObserved(summary->observed);

if (sameString(mixedFilter, "mixed") && !summary->isMixed) 
    return TRUE;
if (sameString(mixedFilter, "not mixed") && summary->isMixed) 
    return TRUE;

if (sameString(popCountFilter, "all 11 Phase III populations") && summary->popCount != 11) 
    return TRUE;
if (sameString(popCountFilter, "all 4 Phase II populations") && summary->phaseIIPopCount != 4)
    return TRUE;

if (filterObserved(complexObserved, transitionObserved))
    return TRUE;

if (filterFreq(summary->isMixed, summary->minFreq, summary->maxFreq))
    return TRUE;

if (summary->score/1000.0 < minHetFilter || summary->score/1000.0 > maxHetFilter) return TRUE;

int i;
for (i = 0;  i < HAP_PHASEIII_POPCOUNT;  i++)
    {
    if (summary->foundInPop[i])
	{
	if (sameString(monoFilter[i], "yes") && !summary->monomorphicInPop[i]) return TRUE;
	if (sameString(monoFilter[i], "no")  &&  summary->monomorphicInPop[i]) return TRUE;
	}
    }

/* ortho quality and alleles */
for (i = 0;  i < HAP_ORTHO_COUNT; i++)
    {
    if (summary->orthoQuals[i] < orthoQualFilter[i]) return TRUE;
    /* if any filter set, data must be available */
    if (differentString(orthoFilter[i], "no filter") && summary->orthoAlleles[i] == 'N')
	return TRUE;
    /* If mixed, then we can't tell if we have a match, so must filter out. */
    if (summary->isMixed &&
	(sameString(orthoFilter[i], "matches major human allele") ||
	 sameString(orthoFilter[i], "matches minor human allele")))
	return TRUE;
    if (sameString(orthoFilter[i], "matches major human allele") &&
	summary->orthoAlleles[i] != summary->overallMajorAllele)
	return TRUE;
    if (sameString(orthoFilter[i], "matches minor human allele") &&
	summary->maxFreq > 0 &&  // make sure minor allele has been observed
	summary->orthoAlleles[i] != summary->overallMinorAllele)
	return TRUE;
    /* mismatches */
    if (sameString(orthoFilter[i], "matches neither human allele"))
	{
	char observed[128];
	safecpy(observed, sizeof(observed), summary->observed);
	#define MAX_OBS_ALS 10
	char *obsAls[MAX_OBS_ALS];
	int alCount = chopByChar(observed, '/', obsAls, ArraySize(obsAls));
	if (alCount > MAX_OBS_ALS)
	    errAbort("Too many observed alleles for HapMap SNP %s", summary->name);
	int j;
	for (j = 0;  j < alCount;  j++)
	    if (strlen(obsAls[j]) == 1 && summary->orthoAlleles[i] == obsAls[j][0]) return TRUE;
	}
    }
return FALSE;
}

boolean loadOrthosPhaseIII(struct track *tg, struct sqlConnection *conn, struct hash *summaryHash)
/* If tg->table is a hapmapAlleles table, load & filter into tg->items and return TRUE. */
{
int i;
char orthoTable[HDB_MAX_TABLE_STRING];
for (i = 0;  i < HAP_ORTHO_COUNT;  i++)
    {
    safef(orthoTable, sizeof(orthoTable), "hapmapAlleles%s", hapmapOrthoSpecies[i]);
    if (sameString(tg->table, orthoTable))
	{
	struct hapmapAllelesOrtho *item, *itemList = NULL;
	struct hapmapPhaseIIISummary *summary;
	int rowOffset;
	struct sqlResult *sr = hRangeQuery(conn, tg->table,
					   chromName, winStart, winEnd, NULL, &rowOffset);
	char **row;
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    item = hapmapAllelesOrthoLoad(row+rowOffset);
	    summary = (struct hapmapPhaseIIISummary *)hashMustFindVal(summaryHash, item->name);
	    if (!filterOnePhaseIII(summary))
		slAddHead(&itemList, item);
	    }
	sqlFreeResult(&sr);
	slSort(&itemList, bedCmp);
	tg->items = itemList;
	return TRUE;
	}
    }
return FALSE;
}

static void hapmapLoadPhaseIII(struct track *tg)
/* load filtered data from HapMap PhaseIII (11 populations) */
{
struct sqlConnection *conn = hAllocConn(database);
char **row = NULL;
int rowOffset;
struct hash *summaryHash = loadSummaryHashPhaseIII(conn);

if (loadOrthosPhaseIII(tg, conn, summaryHash))
    return;

struct hapmapSnps *item, *itemList = NULL;
struct hapmapPhaseIIISummary *summary;
struct sqlResult *sr = hRangeQuery(conn, tg->table, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    item = hapmapSnpsLoad(row+rowOffset);
    summary = (struct hapmapPhaseIIISummary *)hashMustFindVal(summaryHash, item->name);
    if (!filterOnePhaseIII(summary))
	slAddHead(&itemList, item);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slSort(&itemList, bedCmp);
tg->items = itemList;
}


static void hapmapLoad(struct track *tg)
/* load filtered data */
{
boolean isPhaseIII = sameString(trackDbSettingOrDefault(tg->tdb, "hapmapPhase", "II"), "III");
loadFilters(isPhaseIII);
if (allDefaults(isPhaseIII) ||
    (isPhaseIII && !hTableExists(database, "hapmapPhaseIIISummary")) ||
    (!isPhaseIII & !hTableExists(database, "hapmapAllelesSummary")))
    hapmapLoadSimple(tg);
else if (isPhaseIII)
    hapmapLoadPhaseIII(tg);
else
    hapmapLoadPhaseII(tg);
}

void hapmapDrawAt(struct track *tg, void *item, struct hvGfx *hvg, int xOff, int y, double scale, MgFont *font, Color color, enum trackVisibility vis)
/* Draw the hapmap snps at a given position. */
/* Display allele when zoomed to base level. */
{

if (!zoomedToBaseLevel)
    {
    bedDrawSimpleAt(tg, item, hvg, xOff, y, scale, font, color, vis);
    return;
    }

/* figure out allele to display */
/* allele varies depending on which type of subtrack */

char *allele = NULL;
char *strand = NULL;
int chromStart = 0;
int chromEnd = 0;
boolean isOrtho = FALSE;
int i;
char orthoTable[HDB_MAX_TABLE_STRING];
for (i = 0;  i < HAP_ORTHO_COUNT;  i++)
    {
    if (endsWith(tg->table, "PhaseII"))
	safef(orthoTable, sizeof(orthoTable), "hapmapAlleles%sPhaseII", hapmapOrthoSpecies[i]);
    else
	safef(orthoTable, sizeof(orthoTable), "hapmapAlleles%s", hapmapOrthoSpecies[i]);
    if (sameString(tg->table, orthoTable))
	{
	struct hapmapAllelesOrtho *thisItem = item;
	chromStart = thisItem->chromStart;
	chromEnd = thisItem->chromEnd;
	allele = cloneString(thisItem->orthoAllele);
	strand = cloneString(thisItem->strand);
	isOrtho = TRUE;
	break;
	}
    }
if (!isOrtho)
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
if (sameString(strand, "-"))
    reverseComplement(allele, 1);

/* determine graphics attributes for vgTextCentered */
int x1, x2, w;
x1 = round((double)((int)chromStart-winStart)*scale) + xOff;
x2 = round((double)((int)chromEnd-winStart)*scale) + xOff;
w = x2-x1;
if (w < 1)
    w = 1;

int fontHeight = mgFontLineHeight(font);
int heightPer = tg->heightPer;
y += heightPer / 2 - fontHeight / 2;

if (cartUsualBooleanDb(cart, database, COMPLEMENT_BASES_VAR, FALSE))
    reverseComplement(allele, 1);

Color textColor = MG_BLACK;
hvGfxTextCentered(hvg, x1, y, w, heightPer, textColor, font, allele);

}



static Color hapmapColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw hapmap item */
/* Use grayscale based on minor allele score or quality score. */
/* The higher minor allele frequency, the darker the shade. */
/* The higher quality score, the darker the shade. */
{
Color col;
int totalCount = 0;
int minorCount = 0;
int gradient = 0;

int i;
char orthoTable[HDB_MAX_TABLE_STRING];
for (i = 0;  i < HAP_ORTHO_COUNT;  i++)
    {
    if (endsWith(tg->table, "PhaseII"))
	safef(orthoTable, sizeof(orthoTable), "hapmapAlleles%sPhaseII", hapmapOrthoSpecies[i]);
    else
	safef(orthoTable, sizeof(orthoTable), "hapmapAlleles%s", hapmapOrthoSpecies[i]);
    if (sameString(tg->table, orthoTable))
	{
	struct hapmapAllelesOrtho *thisItem = item;
	gradient = grayInRange(thisItem->score, 0, 100);
	col = shadesOfBrown[gradient];
	return col;
	}
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

