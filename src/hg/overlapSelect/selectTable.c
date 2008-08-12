/* selectTable - module that contains ranges use to select.  This module
 * functions as a global object. */

#include "common.h"
#include "selectTable.h"
#include "rowReader.h"
#include "bits.h"
#include "chromAnn.h"
#include "chromAnnMap.h"
#include "verbose.h"

static struct chromAnnMap* selectMap = NULL; // select object map

static void selectMapEnsure()
/* create select map if it doesn't exist */
{
if (selectMap == NULL)
    selectMap = chromAnnMapNew();
}

struct chromAnnMapIter selectTableFirst()
/* iterator over select table */
{
selectMapEnsure();
return chromAnnMapFirst(selectMap);
}

void selectTableFree()
/* free selectTable structures. */
{
chromAnnMapFree(&selectMap);
}

static void selectDumpChromAnn(struct chromAnn *ca, char *label)
/* print a chromAnn if select by verbose level */
{
if (verboseLevel() >= 2)
    {
    verbose(2, "%s: %s: %s %c %d-%d\n", label, ca->name, ca->chrom,
            ((ca->strand == 0) ? '?' : ca->strand), ca->start, ca->end);
    if (verboseLevel() >= 3)
        {
        struct chromAnnBlk *cab;
        for (cab = ca->blocks; cab != NULL; cab = cab->next)
            verbose(3, "    blk: %d-%d\n", cab->start, cab->end);
        }
    }
}

void selectTableAddRecords(struct chromAnnReader *car)
/* add records to the select table */
{
selectMapEnsure();

struct chromAnn* ca;
while ((ca = car->caRead(car)) != NULL)
    {
    selectDumpChromAnn(ca, "selectAddChromAnn");
    chromAnnMapAdd(selectMap, ca);
    }
}

static boolean isSelfMatch(unsigned opts, struct chromAnn *inCa, struct chromAnn* selCa)
/* check if this is a self record */
{
struct chromAnnBlk *inCaBlk, *selCaBlk;

/* already know we are on same chrom and strand (if check strand) */
if ((inCa->start != selCa->start) || (inCa->end != selCa->end))
    return FALSE;
if (((inCa->name != NULL) && (selCa->name != NULL))
    && !sameString(inCa->name, selCa->name))
    return FALSE;

/* check for identical block structures */
for (inCaBlk = inCa->blocks, selCaBlk = selCa->blocks;
     ((inCaBlk != NULL) && (selCaBlk != NULL));
     inCaBlk = inCaBlk->next, selCaBlk = selCaBlk->next)
    {
    if ((inCaBlk->start != selCaBlk->start) || (inCaBlk->end != selCaBlk->end))
        return FALSE;
    }
if ((inCaBlk != NULL) || (selCaBlk != NULL))
    return FALSE;  /* different lengths */

return TRUE;
}

static boolean passCriteria(unsigned opts, struct chromAnn *inCa, struct chromAnn* selCa)
/* see if the global criteria for overlap are satisfied */
{
if ((opts & selStrand) && (inCa->strand != selCa->strand))
    return FALSE;
if ((opts & selOppositeStrand) && (inCa->strand == selCa->strand))
    return FALSE;
if ((opts & selExcludeSelf) && isSelfMatch(opts, inCa, selCa))
    return FALSE;
if ((opts & selIdMatch) && (inCa->name != NULL) && (selCa->name != NULL)
    && sameString(inCa->name, selCa->name))
    return FALSE;
return TRUE;
}

int selectOverlapBases(struct chromAnn *ca1, struct chromAnn *ca2)
/* determine the number of bases of overlap in two annotations */
{
int overBases = 0;
struct chromAnnBlk *ca1Blk, *ca2Blk;

for (ca1Blk = ca1->blocks; ca1Blk != NULL; ca1Blk = ca1Blk->next)
    {
    for (ca2Blk = ca2->blocks; ca2Blk != NULL; ca2Blk = ca2Blk->next)
        {
        if ((ca1Blk->start < ca2Blk->end) && (ca1Blk->end > ca2Blk->start))
            {
            overBases += min(ca1Blk->end, ca2Blk->end)
                - max(ca1Blk->start, ca2Blk->start);
            }
        }
    }
return overBases;
}

float selectFracOverlap(struct chromAnn *ca, int overBases)
/* get the fraction of ca overlapped give number of overlapped bases */
{
return ((float)overBases) / ((float)ca->totalSize);
}

float selectFracSimilarity(struct chromAnn *ca1, struct chromAnn *ca2,
                           int overBases)
/* get the fractions similarity betten two annotations, give number of
 * overlapped bases */
{
return ((float)(2*overBases)) / ((float)(ca1->totalSize+ca2->totalSize));
}

static boolean isOverlapped(unsigned opts, struct chromAnn *inCa, struct chromAnn* selCa,
                            struct overlapCriteria *criteria)
/* see if a chromAnn objects overlap base on thresholds.  If thresholds are zero,
 * any overlap will select. */
{
boolean anyCriteria = FALSE;
boolean overlapped = FALSE;
boolean notOverlapped = FALSE;  // negative crieria (ceiling)
unsigned overBases = selectOverlapBases(inCa, selCa);
if (criteria->similarity > 0.0)
    {
    // similarity
    if (selectFracSimilarity(inCa, selCa, overBases) >= criteria->similarity)
        overlapped = TRUE;
    anyCriteria = TRUE;
    }
if (criteria->similarityCeil <= 1.0)
    {
    // bi-directional ceiling
    if (selectFracSimilarity(inCa, selCa, overBases) >= criteria->similarityCeil)
        notOverlapped = TRUE;
    anyCriteria = TRUE;
    }
if (criteria->threshold > 0.0)
    {
    // uni-directional
    if (selectFracOverlap(inCa, overBases) >= criteria->threshold)
        overlapped = TRUE;
    anyCriteria = TRUE;
    }
if (criteria->thresholdCeil <= 1.0)
    {
    // uni-directional ceiling
    if (selectFracOverlap(inCa, overBases) >= criteria->thresholdCeil)
        notOverlapped = TRUE;
    anyCriteria = TRUE;
    }
if (criteria->bases >= 0)
    {
    // base overlap
    if (overBases >= criteria->bases)
        overlapped = TRUE;
    anyCriteria = TRUE;
    }

if (!anyCriteria)
    {
    // test for any overlap
    if (overBases > 0)
        overlapped = TRUE;
    }
return overlapped && !notOverlapped;
}

static void addOverlapRecs(struct chromAnnRef **overlappingRecs, struct chromAnnRef *newRecs)
/* add overlapping records that are not dups */
{
struct chromAnnRef *orl;
for (orl = newRecs; orl != NULL; orl = orl->next)
    chromAnnRefAdd(overlappingRecs, orl->ref);
}
static boolean selectOverlappingEntry(unsigned opts, struct chromAnn *inCa,
                                      struct chromAnn* selCa, struct overlapCriteria *criteria)
/* select based on a single select chromAnn */
{
boolean overlapped = FALSE;
verbose(2, "\toverlapping: enter %s: %d-%d, %c\n", selCa->name, selCa->start, selCa->end,
        ((selCa->strand == '\0') ? '?' : selCa->strand));
if (!passCriteria(opts, inCa, selCa))
    {
    verbose(2, "\toverlapping: leave %s: fail criteria\n", selCa->name);
    }
else
    {
    overlapped = isOverlapped(opts, inCa, selCa, criteria);
    verbose(2, "\toverlapping: leave %s: %s\n", selCa->name,
            (overlapped ? "yes" : "no"));
    }
return overlapped;
}

static boolean selectWithOverlapping(unsigned opts, struct chromAnn *inCa,
                                     struct chromAnnRef* overlapping,
                                     struct overlapCriteria *criteria,
                                     struct chromAnnRef **overlappingRecs)
/* given a list of overlapping elements, see if inCa is selected, optionally returning
 * the list of selected records */
{
boolean anyHits = FALSE;
struct chromAnnRef *curOverRecs = NULL;  /* don't add til; the end */
struct chromAnnRef *selCa;

/* check each overlapping chomAnn */
for (selCa = overlapping; selCa != NULL; selCa = selCa->next)
    {
    if (selectOverlappingEntry(opts, inCa, selCa->ref, criteria))
        {
        anyHits = TRUE;
        selCa->ref->used = TRUE;
        if (overlappingRecs != NULL)
            chromAnnRefAdd(&curOverRecs, selCa->ref);
        else
            break;  /* only need one overlap */
        }
    }
/* n.b. delayed adding to list so minCoverage can some day be implemented */
if (overlappingRecs != NULL)
    {
    if (anyHits)
        addOverlapRecs(overlappingRecs, curOverRecs);
    slFreeList(&curOverRecs);
    }
return anyHits;
}

boolean selectIsOverlapped(unsigned opts, struct chromAnn *inCa,
                           struct overlapCriteria *criteria,
                           struct chromAnnRef **overlappingRecs)
/* Determine if a range is overlapped.  If overlappingRecs is not null, a list
 * of the of selected records is returned.  Free with slFreelList. */
{
selectMapEnsure();
selectDumpChromAnn(inCa, "selectIsOverlapped");
boolean hit = FALSE;
struct chromAnnRef *overlapping = chromAnnMapFindOverlap(selectMap, inCa);
if (overlapping != NULL)
    {
    hit = selectWithOverlapping(opts, inCa, overlapping, criteria, overlappingRecs);
    slFreeList(&overlapping);
    }
verbose(2, "selectIsOverlapped: leave %s %s\n", inCa->name, (hit ? "yes" : "no"));
return hit;
}

static void addToAggregateMap(Bits *overMap, int mapOff, struct chromAnn *ca1,
                              struct chromAnn *ca2)
/* set bits based on overlap between two chromAnn */
{
struct chromAnnBlk *ca1Blk, *ca2Blk;

for (ca1Blk = ca1->blocks; ca1Blk != NULL; ca1Blk = ca1Blk->next)
    {
    for (ca2Blk = ca2->blocks; ca2Blk != NULL; ca2Blk = ca2Blk->next)
        {
        if ((ca1Blk->start < ca2Blk->end) && (ca1Blk->end > ca2Blk->start))
            {
            int start = max(ca1Blk->start, ca2Blk->start);
            int end = min(ca1Blk->end, ca2Blk->end);
            bitSetRange(overMap, start-mapOff, end-start);
            }
        }
    }
}

static void computeAggregateOverlap(unsigned opts, struct chromAnn *inCa,
                                    struct chromAnnRef* overlapping,
                                    struct overlapAggStats *stats)
/* Compute the aggregate overlap */
{

int mapOff = inCa->start;
int mapLen = (inCa->end - inCa->start);
assert(mapLen >= 0);
if (mapLen == 0)
    return;  /* no CDS */

Bits *overMap = bitAlloc(mapLen);
struct chromAnnRef *selCa;
for (selCa = overlapping; selCa != NULL; selCa = selCa->next)
    {
    if (passCriteria(opts, inCa, selCa->ref))
        addToAggregateMap(overMap, mapOff, inCa, selCa->ref);
    }
stats->inOverBases = bitCountRange(overMap, 0, mapLen);
bitFree(&overMap);
stats->inOverlap = ((float)stats->inOverBases) / ((float)inCa->totalSize);
}

struct overlapAggStats selectAggregateOverlap(unsigned opts, struct chromAnn *inCa)
/* Compute the aggregate overlap of a chromAnn */
{
selectMapEnsure();
struct overlapAggStats stats;
ZeroVar(&stats);
stats.inBases = inCa->totalSize;
struct chromAnnRef *overlapping = chromAnnMapFindOverlap(selectMap, inCa);
computeAggregateOverlap(opts, inCa, overlapping, &stats);
slFreeList(&overlapping);
verbose(2, "selectAggregateOverlap: %s: %s %d-%d, %c => %0.3g\n", inCa->name, inCa->chrom, inCa->start, inCa->end,
        ((inCa->strand == '\0') ? '?' : inCa->strand), stats.inOverlap);
return stats;
}

