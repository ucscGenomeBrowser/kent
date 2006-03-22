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
 * a loop until a psl block has been checked*/
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

static boolean similarAligns(struct cDnaQuery *cdna,
                             struct cDnaAlign *aln1,
                             struct cDnaAlign *aln2)
/* Test if two genes have similar alignments.  Similar means that two
 * alignments of the same cDNA align to the same location, allowing for a
 * small amount of disagreement.  The idea is to not to discard some kind of
 * weird, shifted alignment to the same region. */
{
static float dissimFrac = 0.98;
struct alignSimilarities alnSim;
int iBlk1;
ZeroVar(&alnSim);

for (iBlk1 = 0; iBlk1 < aln1->psl->blockCount; iBlk1++)
    blockSimCheck(&alnSim, aln1->psl, iBlk1, aln2->psl);

/* weird case: they overlap and don't share bases */
if ((alnSim.same + alnSim.diff) == 0)
    return FALSE;

/* check for a high level of similarity */
if (((float)(alnSim.same)/((float)(alnSim.same + alnSim.diff))) < dissimFrac)
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
    return FALSE;
    }
return TRUE;
}

static int overlapCmp(struct cDnaQuery *cdna,
                      struct cDnaAlign *aln1,
                      struct cDnaAlign *aln2)
/* compare two overlapping alignments to see which to keep. Returns 
 * -1 to keep the first, 1 to keep the second, or 0 to keep both */
{
if (!similarAligns(cdna, aln1, aln2))
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
            {
            aln2->drop = TRUE;
            cDnaAlignVerb(3, aln2, "drop: overlap");
            }
        else if (cmp > 0)
            {
            aln->drop = TRUE;
            cDnaAlignVerb(3, aln, "drop: overlap");
            }
        if (cmp != 0)
            cdna->stats->overlapCnts.aligns++;
        }
    }
}

void overlapFilter(struct cDnaQuery *cdna)
/* Remove overlapping alignments, keeping only one by some criteria.  This is
 * designed to be used with windowed alignments, so one alignment might be
 * trucated. */
{
struct cDnaAlign *aln;
for (aln = cdna->alns; aln != NULL; aln = aln->next)
    {
    if (!aln->drop)
        overlapFilterAln(cdna, aln);
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
