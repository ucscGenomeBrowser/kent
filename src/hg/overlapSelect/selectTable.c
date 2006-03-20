/* selectTable - module that contains ranges use to select */

#include "common.h"
#include "selectTable.h"
#include "linefile.h"
#include "binRange.h"
#include <bits.h>
#include "hash.h"
#include "chromAnn.h"
#include "verbose.h"

/* tables are never freed; these should be real objects  */
static struct hash* selectChromHash = NULL;  /* hash per-chrom binKeeper of
                                              * chromAnn objects */
static struct binKeeper* selectGetChromBins(char* chrom, boolean createMissing,
                                            char** key)
/* get the bins for a chromsome, optionally creating if it doesn't exist.
 * Return key if requested so string can be reused. */
{
struct hashEl* hel;

if (selectChromHash == NULL)
    selectChromHash = hashNew(10);  /* first time */
hel = hashLookup(selectChromHash, chrom);
if ((hel == NULL) && createMissing)
    {
    struct binKeeper* bins = binKeeperNew(0, 300000000);
    hel = hashAdd(selectChromHash, chrom, bins);
    }
if (hel == NULL)
    return NULL;
if ((key != NULL) && (hel != NULL))
    *key = hel->name;
return (struct binKeeper*)hel->val;
}

static void selectAddChromAnn(struct chromAnn *ca)
/* Add a chromAnn to the select table */
{
struct binKeeper* bins = selectGetChromBins(ca->chrom, TRUE, NULL);
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

void selectAddPsls(unsigned opts, struct lineFile *pslLf)
/* add records from a psl file to the select table */
{
char *line;

while (lineFileNextReal(pslLf, &line))
    selectAddChromAnn(chromAnnFromPsl(getChomAnnOpts(opts), pslLf, line));
}

void selectAddGenePreds(unsigned opts, struct lineFile *genePredLf)
/* add blocks from a genePred file to the select table */
{
char *line;
while (lineFileNextReal(genePredLf, &line))
    selectAddChromAnn(chromAnnFromGenePred(getChomAnnOpts(opts), genePredLf, line));
}

void selectAddBeds(unsigned opts, struct lineFile* bedLf)
/* add records from a bed file to the select table */
{
char *line;

while (lineFileNextReal(bedLf, &line))
    selectAddChromAnn(chromAnnFromBed(getChomAnnOpts(opts), bedLf, line));
}

void selectAddCoordCols(unsigned opts, struct lineFile *tabLf, struct coordCols* cols)
/* add records with coordiates at a specified column */
{
char *line;
unsigned caOpts = getChomAnnOpts(opts);

while (lineFileNextReal(tabLf, &line))
    selectAddChromAnn(chromAnnFromCoordCols(caOpts, tabLf, line, cols));
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

static int overlappedBases(struct chromAnn *ca1, struct chromAnn *ca2)
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

float selectFracOverlap(struct chromAnn *inCa, struct chromAnn *selCa)
/* get the fraction of inCa overlapped by selCa */
{
return ((float)overlappedBases(inCa, selCa)) / ((float)inCa->totalSize);
}

static boolean isOverlapped(unsigned opts, struct chromAnn *inCa, struct chromAnn* selCa,
                            float overlapThreshold, float overlapSimilarity)
/* see if a chromAnn objects overlap base on thresholds.  If thresholds are zero,
 * any overlap will select. */
{
if (overlapSimilarity > 0.0)
    {
    /* bi-directional */
    return (selectFracOverlap(inCa, selCa) >= overlapSimilarity)
        && (selectFracOverlap(selCa, inCa) >= overlapSimilarity);
    }
else
    {
    /* uni-directional */
    if (overlapThreshold <= 0.0)
        return (selectFracOverlap(inCa, selCa) > 0.0); /* any overlap */
    else
        return (selectFracOverlap(inCa, selCa) >= overlapThreshold);
    }
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
                                      struct chromAnn* selCa,
                                      float overlapThreshold, float overlapSimilarity)
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
    overlapped = isOverlapped(opts, inCa, selCa, overlapThreshold, overlapSimilarity);
    verbose(2, "\toverlapping: leave %s: %s\n", selCa->name,
            (overlapped ? "yes" : "no"));
    }
return overlapped;
}

static boolean selectWithOverlapping(unsigned opts, struct chromAnn *inCa,
                                     struct binElement* overlapping,
                                     float overlapThreshold, float overlapSimilarity,
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
    if (selectOverlappingEntry(opts, inCa, selCa, overlapThreshold, overlapSimilarity))
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
                           float overlapThreshold, float overlapSimilarity,
                           struct slRef **overlappingRecs)
/* Determine if a range is overlapped.  If overlappingRecs is not null, a list
 * of the of selected records is returned.  Free with slFreelList. */
{
boolean hit = FALSE;
struct binKeeper* bins = selectGetChromBins(inCa->chrom, FALSE, NULL);
verbose(2, "selectIsOverlapping: enter %s: %s %d-%d, %c\n", inCa->name, inCa->chrom, inCa->start, inCa->end,
        ((inCa->strand == '\0') ? '?' : inCa->strand));
if (bins != NULL)
    {
    struct binElement* overlapping = binKeeperFind(bins, inCa->start, inCa->end);
    hit = selectWithOverlapping(opts, inCa, overlapping, overlapThreshold,
                                overlapSimilarity, overlappingRecs);
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
                                    struct overlapStats *stats)
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

struct overlapStats selectAggregateOverlap(unsigned opts, struct chromAnn *inCa)
/* Compute the aggregate overlap of a chromAnn */
{
struct binKeeper* bins = selectGetChromBins(inCa->chrom, FALSE, NULL);
struct overlapStats stats;
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

