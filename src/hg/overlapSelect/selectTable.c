/* selectTable - module that contains ranges use to select */

#include "common.h"
#include "selectTable.h"
#include "rowReader.h"
#include "chromBins.h"
#include "binRange.h"
#include "bits.h"
#include "hash.h"
#include "chromAnn.h"
#include "verbose.h"

/* tables are never freed  */
static struct chromBins* selectBins = NULL;  /* per-chrom binKeepers of
                                              * chromAnn objects */
static struct binKeeper *selectBinsGet(char *chrom, boolean create)
/* get chromosome binKeeper, optionally creating if it doesn't exist */
{
if (selectBins == NULL)
    {
    if (!create)
        return NULL;
    selectBins = chromBinsNew((chromBinsFreeFunc*)chromAnnFree);
    }
return chromBinsGet(selectBins, chrom, create);
}

static void selectAddChromAnn(struct chromAnn *ca)
/* Add a chromAnn to the select table */
{
struct binKeeper* bins = selectBinsGet(ca->chrom, TRUE);
if (verboseLevel() >= 2)
    {
    verbose(2, "selectAddChromAnn: %s: %s %c %d-%d\n", ca->name, ca->chrom,
            ((ca->strand == 0) ? '?' : ca->strand), ca->start, ca->end);
    if (verboseLevel() >= 3)
        {
        struct chromAnnBlk *cab;
        for (cab = ca->blocks; cab != NULL; cab = cab->next)
            verbose(3, "    blk: %d-%d\n", cab->start, cab->end);
        }
    }
/* don't add zero-length, they can't select */
if (ca->start < ca->end)
    binKeeperAdd(bins, ca->start, ca->end, ca);
else
    chromAnnFree(&ca);
}

static unsigned getChomAnnOpts(unsigned selOpts)
/* determine chromAnn options from selOps */
{
unsigned caOpts = 0;
if (selOpts & selSelectCds)
    caOpts |= chromAnnCds;
if (selOpts & selSelectRange)
    caOpts |= chromAnnRange;
if (selOpts & selSaveLines)
    caOpts |= chromAnnSaveLines;
return caOpts;
}

void selectAddPsls(unsigned opts, struct rowReader *rr)
/* add psl records to the select table */
{
while (rowReaderNext(rr))
    selectAddChromAnn(chromAnnFromPsl(getChomAnnOpts(opts), rr));
}

void selectAddGenePreds(unsigned opts,  struct rowReader *rr)
/* add genePred records to the select table */
{
while (rowReaderNext(rr))
    selectAddChromAnn(chromAnnFromGenePred(getChomAnnOpts(opts), rr));
}

void selectAddBeds(unsigned opts,  struct rowReader *rr)
/* add bed records to the select table */
{
while (rowReaderNext(rr))
    selectAddChromAnn(chromAnnFromBed(getChomAnnOpts(opts), rr));
}

void selectAddCoordCols(unsigned opts, struct coordCols* cols, struct rowReader *rr)
/* add records with coordiates at a specified column */
{
unsigned caOpts = getChomAnnOpts(opts);

while (rowReaderNext(rr))
    selectAddChromAnn(chromAnnFromCoordCols(caOpts, cols, rr));
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
if ((opts & selUseStrand) && (inCa->strand != selCa->strand))
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
    // bi-directional
    if ((selectFracOverlap(inCa, overBases) >= criteria->similarity)
        && (selectFracOverlap(selCa, overBases) >= criteria->similarity))
        overlapped = TRUE;
    anyCriteria = TRUE;
    }
if (criteria->similarityCeil <= 1.0)
    {
    // bi-directional ceiling
    if ((selectFracOverlap(inCa, overBases) >= criteria->similarityCeil)
        && (selectFracOverlap(selCa, overBases) >= criteria->similarityCeil))
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

static void addOverlapRecs(struct slRef **overlappingRecs, struct slRef *newRecs)
/* add overlapping records that are not dups */
{
struct slRef *orl;
for (orl = newRecs; orl != NULL; orl = orl->next)
    {
    if (!refOnList(*overlappingRecs, orl->val))
        refAdd(overlappingRecs, orl->val);
    }
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
                                     struct binElement* overlapping,
                                     struct overlapCriteria *criteria,
                                     struct slRef **overlappingRecs)
/* given a list of overlapping elements, see if inCa is selected, optionally returning
 * the list of selected records */
{
boolean anyHits = FALSE;
struct slRef *curOverRecs = NULL;  /* don't add til; the end */
struct binElement* o;

/* check each overlapping chomAnn */
for (o = overlapping; o != NULL; o = o->next)
    {
    struct chromAnn* selCa = o->val;
    if (selectOverlappingEntry(opts, inCa, selCa, criteria))
        {
        anyHits = TRUE;
        if (overlappingRecs != NULL)
            refAdd(&curOverRecs, selCa);
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
                           struct slRef **overlappingRecs)
/* Determine if a range is overlapped.  If overlappingRecs is not null, a list
 * of the of selected records is returned.  Free with slFreelList. */
{
boolean hit = FALSE;
struct binKeeper* bins = selectBinsGet(inCa->chrom, FALSE);
verbose(2, "selectIsOverlapping: enter %s: %s %d-%d, %c\n", inCa->name, inCa->chrom, inCa->start, inCa->end,
        ((inCa->strand == '\0') ? '?' : inCa->strand));
if (bins != NULL)
    {
    struct binElement* overlapping = binKeeperFind(bins, inCa->start, inCa->end);
    hit = selectWithOverlapping(opts, inCa, overlapping, criteria, overlappingRecs);
    slFreeList(&overlapping);
    }
verbose(2, "selectIsOverlapping: leave %s %s\n", inCa->name, (hit ? "yes" : "no"));
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
                                    struct binElement* overlapping,
                                    struct overlapAggStats *stats)
/* Compute the aggregate overlap */
{

int mapOff = inCa->start;
int mapLen = (inCa->end - inCa->start);
Bits *overMap;
struct binElement* o;
assert(mapLen >= 0);
if (mapLen == 0)
    return;  /* no CDS */

overMap = bitAlloc(mapLen);

for (o = overlapping; o != NULL; o = o->next)
    {
    struct chromAnn* selCa = o->val;
    if (passCriteria(opts, inCa, selCa))
        addToAggregateMap(overMap, mapOff, inCa, selCa);
    }
stats->inOverBases = bitCountRange(overMap, 0, mapLen);
bitFree(&overMap);
stats->inOverlap = ((float)stats->inOverBases) / ((float)inCa->totalSize);
}

struct overlapAggStats selectAggregateOverlap(unsigned opts, struct chromAnn *inCa)
/* Compute the aggregate overlap of a chromAnn */
{
struct binKeeper* bins = selectBinsGet(inCa->chrom, FALSE);
struct overlapAggStats stats;
ZeroVar(&stats);
stats.inBases = inCa->totalSize;
if (bins != NULL)
    {
    struct binElement* overlapping = binKeeperFind(bins, inCa->start, inCa->end);
    computeAggregateOverlap(opts, inCa, overlapping, &stats);
    slFreeList(&overlapping);
    }
verbose(2, "selectAggregateOverlap: %s: %s %d-%d, %c => %0.3g\n", inCa->name, inCa->chrom, inCa->start, inCa->end,
        ((inCa->strand == '\0') ? '?' : inCa->strand), stats.inOverlap);
return stats;
}

