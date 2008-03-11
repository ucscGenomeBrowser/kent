/* pslTransMap - transitive mapping of an alignment another sequence, via a
 * common alignment */
#include "common.h"
#include "pslTransMap.h"
#include "psl.h"

/*
 * Notes:
 *  - This code is used with both large and small mapping psls.  Large
 *    psls used for doing cross-species mappings and small psl are used for
 *    doing protein to mRNA mappings.  This introduces some speed issues.  For
 *    chain mapping, a large amount of time is spent in getBlockMapping()
 *    searching for the starting point of a mapping.  However an optimization
 *    to find the starting point, such as a binKeeper, could be inefficient
 *    for a large number of small psls.  Implementing such an optimzation
 *    would have to be dependent on the type of mapping.  The code was made
 *    16x faster for genome mappings by remembering the current location in
 *    the mapping psl between blocks (iMapBlkPtr arg).  This will do for a
 *    while.
 */


struct block
/* coordinates of a block */
{
    int qStart;          /* Query start position. */
    int qEnd;            /* Query end position. */
    int tStart;          /* Target start position. */
    int tEnd;            /* Target end position. */
};

static void pslProtToNA(struct psl *psl)
/* convert a protein/NA alignment to a NA/NA alignment */
{
int iBlk;

psl->qStart *= 3;
psl->qEnd *= 3;
psl->qSize *= 3;
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    psl->blockSizes[iBlk] *= 3;
    psl->qStarts[iBlk] *= 3;
    }
}

static void pslNAToProt(struct psl *psl)
/* undo pslProtToNA */
{
int iBlk;

psl->qStart /= 3;
psl->qEnd /= 3;
psl->qSize /= 3;
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    psl->blockSizes[iBlk] /= 3;
    psl->qStarts[iBlk] /= 3;
    }
}

static struct psl* createMappedPsl(struct psl* inPsl, struct psl *mapPsl,
                                   int mappedPslMax)
/* setup a PSL for the output alignment */
{
char strand[3];
assert(pslTStrand(inPsl) == pslQStrand(mapPsl));

/* strand can be taken from both alignments, since common sequence is in same
 * orientation. */
strand[0] = pslQStrand(inPsl);
strand[1] = pslTStrand(mapPsl);
strand[2] = '\n';

return pslNew(inPsl->qName, inPsl->qSize, 0, 0,
              mapPsl->tName, mapPsl->tSize, 0, 0,
              strand, mappedPslMax, 0);
}

static struct block blockFromPslBlock(struct psl* psl, int iBlock)
/* fill in a block object from a psl block */
{
struct block block;
block.qStart = psl->qStarts[iBlock];
block.qEnd = psl->qStarts[iBlock] + psl->blockSizes[iBlock];
block.tStart = psl->tStarts[iBlock];
block.tEnd = psl->tStarts[iBlock] + psl->blockSizes[iBlock];
return block;
}

static void addPslBlock(struct psl* psl, struct block* blk, int* pslMax)
/* add a block to a psl */
{
unsigned newIBlk = psl->blockCount;

assert((blk->qEnd - blk->qStart) == (blk->tEnd - blk->tStart));
if (newIBlk >= *pslMax)
    pslGrow(psl, pslMax);
psl->qStarts[newIBlk] = blk->qStart;
psl->tStarts[newIBlk] = blk->tStart;
psl->blockSizes[newIBlk] = blk->qEnd - blk->qStart;
/* lie about match counts. */
psl->match += psl->blockSizes[newIBlk];
psl->blockCount++;
}

static void setPslBounds(struct psl* mappedPsl)
/* set sequences bounds on mapped PSL */
{
int lastBlk = mappedPsl->blockCount-1;

/* set start/end of sequences */
mappedPsl->qStart = mappedPsl->qStarts[0];
mappedPsl->qEnd = mappedPsl->qStarts[lastBlk] + mappedPsl->blockSizes[lastBlk];
if (pslQStrand(mappedPsl) == '-')
    reverseIntRange(&mappedPsl->qStart, &mappedPsl->qEnd, mappedPsl->qSize);

mappedPsl->tStart = mappedPsl->tStarts[0];
mappedPsl->tEnd = mappedPsl->tStarts[lastBlk] + mappedPsl->blockSizes[lastBlk];
if (pslTStrand(mappedPsl) == '-')
    reverseIntRange(&mappedPsl->tStart, &mappedPsl->tEnd, mappedPsl->tSize);
}

static void adjustOrientation(unsigned opts, struct psl *inPsl, char *inPslOrigStrand,
                              struct psl* mappedPsl)
/* adjust strand, possibly reverse complementing, based on
 * pslTransMapKeepTrans option and input psls. */
{
if (!(opts&pslTransMapKeepTrans) || ((inPslOrigStrand[1] == '\0') && (mappedPsl->strand[1] == '\0')))
    {
    /* make untranslated */
    if (pslTStrand(mappedPsl) == '-')
        pslRc(mappedPsl);
    mappedPsl->strand[1] = '\0';  /* implied target strand */
    }
}

static struct block getBeforeBlockMapping(unsigned mqStart, unsigned mqEnd,
                                          struct block* align1Blk)
/* map part of an ungapped psl block that occurs before a mapPsl block */
{
struct block mappedBlk;

/* mRNA start in genomic gap before this block, this will
 * be an inPsl insert */
unsigned size = (align1Blk->tEnd < mqStart)
    ? (align1Blk->tEnd - align1Blk->tStart)
    : (mqStart - align1Blk->tStart);
ZeroVar(&mappedBlk);
mappedBlk.qStart = align1Blk->qStart;
mappedBlk.qEnd = align1Blk->qStart + size;
return mappedBlk;
}

static struct block getOverBlockMapping(unsigned mqStart, unsigned mqEnd,
                                        unsigned mtStart, struct block* align1Blk)
/* map part of an ungapped psl block that overlapps a mapPsl block. */
{
struct block mappedBlk;

/* common sequence start contained in this block, this handles aligned
 * and genomic inserts */
unsigned off = align1Blk->tStart - mqStart;
unsigned size = (align1Blk->tEnd > mqEnd)
    ? (mqEnd - align1Blk->tStart)
    : (align1Blk->tEnd - align1Blk->tStart);
ZeroVar(&mappedBlk);
mappedBlk.qStart = align1Blk->qStart;
mappedBlk.qEnd = align1Blk->qStart + size;
mappedBlk.tStart = mtStart + off;
mappedBlk.tEnd = mtStart + off + size;
return mappedBlk;
}

static struct block getBlockMapping(struct psl* inPsl, struct psl *mapPsl,
                                    int *iMapBlkPtr, struct block* align1Blk)
/* Map part or all of a ungapped psl block to the genome.  This returns the
 * coordinates of the sub-block starting at the beginning of the align1Blk.
 * If this is a gap, either the target or query coords are zero.  This works
 * in genomic strand space.  The search starts at the specified map block,
 * which is updated to prevent rescanning large psls.
 */
{
int iBlk;
struct block mappedBlk;

/* search for block or gap containing start of mrna block */
for (iBlk = *iMapBlkPtr; iBlk < mapPsl->blockCount; iBlk++)
    {
    unsigned mqStart = mapPsl->qStarts[iBlk];
    unsigned mqEnd = mapPsl->qStarts[iBlk]+mapPsl->blockSizes[iBlk];
    if (align1Blk->tStart < mqStart)
        {
        *iMapBlkPtr = iBlk;
        return getBeforeBlockMapping(mqStart, mqEnd, align1Blk);
        }
    if ((align1Blk->tStart >= mqStart) && (align1Blk->tStart < mqEnd))
        {
        *iMapBlkPtr = iBlk;
        return getOverBlockMapping(mqStart, mqEnd, mapPsl->tStarts[iBlk], align1Blk);
        }
    }

/* reached the end of the mRNA->genome alignment, finish off the 
 * rest of the the protein as an insert */
ZeroVar(&mappedBlk);
mappedBlk.qStart = align1Blk->qStart;
mappedBlk.qEnd = align1Blk->qEnd;
*iMapBlkPtr = iBlk;
return mappedBlk;
}

static boolean mapBlock(struct psl *inPsl, struct psl *mapPsl, int *iMapBlkPtr,
                        struct block *align1Blk, struct psl* mappedPsl,
                        int* mappedPslMax)
/* Add a PSL block from a ungapped portion of an alignment, mapping it to the
 * genome.  If the started of the inPsl block maps to a part of the mapPsl
 * that is aligned, it is added as a PSL block, otherwise the gap is skipped.
 * Block starts are adjusted for next call.  Return FALSE if the end of the
 * alignment is reached. */
{
struct block mappedBlk;
unsigned size;
assert(align1Blk->qStart <= align1Blk->qEnd);
assert(align1Blk->tStart <= align1Blk->tEnd);
assert((align1Blk->qEnd - align1Blk->qStart) == (align1Blk->tEnd - align1Blk->tStart));

if ((align1Blk->qStart >= align1Blk->qEnd) || (align1Blk->tStart >= align1Blk->tEnd))
    return FALSE;  /* end of ungapped block. */

/* find block or gap with start coordinates of mrna */
mappedBlk = getBlockMapping(inPsl, mapPsl, iMapBlkPtr, align1Blk);

if ((mappedBlk.qEnd != 0) && (mappedBlk.tEnd != 0))
    addPslBlock(mappedPsl, &mappedBlk, mappedPslMax);

/* advance past block or gap */
size = (mappedBlk.qEnd != 0)
    ? (mappedBlk.qEnd - mappedBlk.qStart)
    : (mappedBlk.tEnd - mappedBlk.tStart);
align1Blk->qStart += size;
align1Blk->tStart += size;

return TRUE;
}

struct psl* pslTransMap(unsigned opts, struct psl *inPsl, struct psl *mapPsl)
/* map a psl via a mapping psl, a single psl is returned, or NULL if it
 * couldn't be mapped. */
{
int mappedPslMax = 8; /* allocated space in output psl */
int iMapBlk = 0;
char inPslOrigStrand[3];
boolean rcInPsl = (pslTStrand(inPsl) != pslQStrand(mapPsl));
boolean cnv1 = (pslIsProtein(inPsl) && !pslIsProtein(mapPsl));
boolean cnv2 = (pslIsProtein(mapPsl) && !pslIsProtein(inPsl));
int iBlock;
struct psl* mappedPsl;

/* sanity check size, but allow names to vary to allow ids to have
 * unique-ifying suffixes. */
if (inPsl->tSize != mapPsl->qSize)
    errAbort("Error: inPsl %s tSize (%d) != mapPsl %s qSize (%d)",
            inPsl->tName, inPsl->tSize, mapPsl->qName, mapPsl->qSize);

/* convert protein PSLs */
if (cnv1)
    pslProtToNA(inPsl);
if (cnv2)
    pslProtToNA(mapPsl);

/* need to ensure common sequence is in same orientation, save strand for later */
safef(inPslOrigStrand, sizeof(inPslOrigStrand), "%s", inPsl->strand);
if (rcInPsl)
    pslRc(inPsl);

mappedPsl = createMappedPsl(inPsl, mapPsl, mappedPslMax);

/* Fill in ungapped blocks.  */
for (iBlock = 0; iBlock < inPsl->blockCount; iBlock++)
    {
    struct block align1Blk = blockFromPslBlock(inPsl, iBlock);
    while (mapBlock(inPsl, mapPsl, &iMapBlk, &align1Blk, mappedPsl,
                    &mappedPslMax))
        continue;
    }

/* finish up psl, or free if no blocks were added */
assert(mappedPsl->blockCount <= mappedPslMax);
if (mappedPsl->blockCount == 0)
    pslFree(&mappedPsl);  /* nothing made it */
else
    {
    setPslBounds(mappedPsl);
    adjustOrientation(opts, inPsl, inPslOrigStrand, mappedPsl);
    }

/* restore input */
if (rcInPsl)
    {
    pslRc(inPsl);
    strcpy(inPsl->strand, inPslOrigStrand);
    }
if (cnv1)
    pslNAToProt(inPsl);
if (cnv2)
    pslNAToProt(mapPsl);

return mappedPsl;
}

