/* overlapFilter - filter overlapping alignments */
#include "common.h"
#include "overlapFilter.h"
#include "cDnaAligns.h"
#include "cDnaStats.h"
#include "verbose.h"
#include "psl.h"

static boolean overlapTest(struct cDnaAlign *aln1,
                           struct cDnaAlign *aln2)
/* test if two alignments overlap on the same strand */
{
return sameString(aln1->psl->tName, aln2->psl->tName)
    && (aln1->psl->tStart < aln2->psl->tEnd)
    && (aln1->psl->tEnd > aln2->psl->tStart)
    && sameString(aln1->psl->strand, aln2->psl->strand);
}

struct alignSimilarities
/* count of bases aligned in the same or different ways */
{
    unsigned same;      /* bases aligned by both to the same locations */
    unsigned diff;      /* bases aligned by both to different locations */
};

static float alignSimilaritiesFrac(struct alignSimilarities as)
/* compute the similarity as a fraction */
{
return ((float)(as.same)/((float)(as.same + as.diff)));
}

static int findOverlappedBlock(unsigned qStart1, unsigned qEnd1,
                               struct psl *psl2)
/* find a psl block that overlapps the query range, or -1 if not found */
{
int iBlk;
for (iBlk = 0; iBlk < psl2->blockCount; iBlk++)
    {
    if (positiveRangeIntersection(qStart1, qEnd1, psl2->qStarts[iBlk],
                                  (psl2->qStarts[iBlk]+psl2->blockSizes[iBlk])))
        return iBlk;
    }
return -1;
}

static unsigned blockSimCount(struct alignSimilarities *alnSim,
                              unsigned qStart1, unsigned qEnd1,
                              unsigned tStart1, struct psl *psl2)
/* given a block range from one psl, update alnSim from the first overlapped
 * block on the other psl.  Return the number block bases check. Called in
 * a loop until a psl block has been checked */
{
unsigned qStart2, qEnd2, tStart2, subSize, consumed = 0;
int iBlk2 = findOverlappedBlock(qStart1, qEnd1, psl2);
if (iBlk2 < 0)
    return (qEnd1-qStart1);  /* consume rest of block */
qStart2 = psl2->qStarts[iBlk2];
qEnd2 = psl2->qStarts[iBlk2]+psl2->blockSizes[iBlk2];
tStart2 = psl2->tStarts[iBlk2];

/* adjust starts until both queries are the same */
if (qStart1 < qStart2)
    {
    unsigned off = qStart2-qStart1;
    consumed += off;
    qStart1 += off;
    tStart1 += off;
    }
else if (qStart1 > qStart2)
    {
    unsigned off = qStart1-qStart2;
    qStart2 += off;
    tStart2 += off;
    }

/* adjust ends so same query range is covered */
if (qEnd1 < qEnd2)
    qEnd2 = qEnd1;
else
    qEnd1 = qEnd2;
assert((qStart1 == qStart2) && (qEnd1 == qEnd2));

/* now have a common query subblock to check */
subSize = (qEnd1-qStart1);
consumed += subSize;
if (tStart1 == tStart2)
    alnSim->same += subSize;
else
    alnSim->diff += subSize;

assert(consumed > 0);
return consumed;
}

static void blockSimCheck(struct alignSimilarities *alnSim,
                          struct psl *psl1, int iBlk1,          
                          struct psl *psl2)
/* collect similarity counts for a single block of one psl against the
 * other psl. */
{
unsigned qStart1 = psl1->qStarts[iBlk1];
unsigned qEnd1 = psl1->qStarts[iBlk1]+psl1->blockSizes[iBlk1];
unsigned tStart1 = psl1->tStarts[iBlk1];

while (qStart1 < qEnd1)
    {
    unsigned consumed = blockSimCount(alnSim, qStart1, qEnd1, tStart1, psl2);
    qStart1 += consumed;
    tStart1 += consumed;
    }
assert(qStart1 == qEnd1);
assert(tStart1 == (psl1->tStarts[iBlk1]+psl1->blockSizes[iBlk1]));
}

static void weirdOverlapCheck(struct cDnaQuery *cdna,
                              struct cDnaAlign *aln1,
                              struct cDnaAlign *aln2)
/* Check if two overlapping cDNAs have dissimilar alignments.  Similar means
 * that two alignments of the same cDNA align to the same location, allowing
 * for a small amount of disagreement.  The idea is to find weird, shifted
 * alignment to the same region.  Flags these cases. */
{
assert(overlapTest(aln1, aln2));
static float dissimFrac = 0.98;
struct alignSimilarities alnSim;
int iBlk1;
ZeroVar(&alnSim);

for (iBlk1 = 0; iBlk1 < aln1->psl->blockCount; iBlk1++)
    blockSimCheck(&alnSim, aln1->psl, iBlk1, aln2->psl);

/* Check for overlap and don't share bases and check for a low level of
 * similarity */
if (((alnSim.same + alnSim.diff) == 0) || (alignSimilaritiesFrac(alnSim) < dissimFrac))
    {
    if (verboseLevel() >= 2)
        {
        if (!aln1->weirdOverlap)
            cDnaAlignVerb(2, aln1, "weird overlap");
        if (!aln2->weirdOverlap)
            cDnaAlignVerb(2, aln2, "weird overlap");
        }
    /* don't double count ones flagged as weird */
    if (!aln1->weirdOverlap)
        cdna->stats->weirdOverCnts.aligns++;
    aln1->weirdOverlap = TRUE;
    if (!aln2->weirdOverlap)
        cdna->stats->weirdOverCnts.aligns++;
    aln2->weirdOverlap = TRUE;
    }
}

static void flagWeird(struct cDnaQuery *cdna,
                      struct cDnaAlign *aln)
/* Flag alignments that have weird overlap with other alignmentst */
{
struct cDnaAlign *aln2;
for (aln2 = aln->next; (aln2 != NULL) && (!aln->drop); aln2 = aln2->next)
    {
    if (overlapTest(aln, aln2))
        weirdOverlapCheck(cdna, aln, aln2);
    }
}

void overlapFilterFlagWeird(struct cDnaQuery *cdna)
/* Flag alignments that have weird overlap with other alignments */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop)
        flagWeird(cdna, aln);
    }
}

static int overlapCmp(struct cDnaQuery *cdna,
                      struct cDnaAlign *aln1,
                      struct cDnaAlign *aln2)
/* compare two overlapping alignments to see which to keep. Returns 
 * -1 to keep the first, 1 to keep the second, or 0 to keep both */
{
if (aln1->weirdOverlap && aln2->weirdOverlap)
    return 0;  /* weird alignment, keep both */

/* next criteria, keep one with the most coverage */
if (aln1->cover > aln2->cover)
    return -1;
else if (aln1->cover < aln2->cover)
    return 1;

/* keep one with highest id */
if (aln1->ident >= aln2->ident)
    return -1;
else
    return 1;
}

static void overlapFilterAln(struct cDnaQuery *cdna,
                             struct cDnaAlign *aln)
/* Filter the given alignment for overlaps against all alignments following it
 * in the list */
{
struct cDnaAlign *aln2;
for (aln2 = aln->next; (aln2 != NULL) && (!aln->drop); aln2 = aln2->next)
    {
    if ((!aln2->drop) && overlapTest(aln, aln2))
        {
        int cmp = overlapCmp(cdna, aln, aln2);
        if (cmp < 0)
            cDnaAlignDrop(aln2, FALSE, &cdna->stats->overlapDropCnts, "overlap");
        else if (cmp > 0)
            cDnaAlignDrop(aln, FALSE, &cdna->stats->overlapDropCnts, "overlap");
        }
    }
}

void overlapFilterOverlapping(struct cDnaQuery *cdna)
/* Remove overlapping alignments, keeping only one by some criteria.  This is
 * designed to be used with overlapping, windowed alignments, so one alignment
 * might be truncated. */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop)
        overlapFilterAln(cdna, aln);
    }
}

static struct cDnaAlign *dropWeirdLowScore(struct cDnaAlign *aln, struct cDnaAlign *aln2)
/* drop the lower scoring of two weirdly overlapping alignments, returning the one to keep */
{
if (aln->score < aln2->score)
    {
    struct cDnaAlign *hold = aln;
    aln = aln2;
    aln2 = hold;
    }
cDnaAlignDrop(aln2, FALSE, &aln2->cdna->stats->weirdDropCnts,  "weird overlap");
return aln;
}

static void dropWeirdOverlapped(struct cDnaQuery *cdna, struct cDnaAlign *aln)
/* overlapping aln, dropping all but the best scoring. */
{
struct cDnaAlign *aln2;
for (aln2 = aln->next; aln2 != NULL; aln2 = aln2->next)
    {
    if (!aln2->drop && aln2->weirdOverlap && overlapTest(aln, aln2))
        aln = dropWeirdLowScore(aln, aln2);
    }
}

void overlapFilterWeirdFilter(struct cDnaQuery *cdna)
/* Filter alignments identified as weirdly overlapping, keeping only the
 * highest scoring ones. */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop && aln->weirdOverlap)
        dropWeirdOverlapped(cdna, aln);
    }
}
