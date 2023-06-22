/* pslTransMap - transitive mapping of an alignment another sequence, via a
 * common alignment */

/* Copyright (C) 2009 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
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

static char *pslTypeDesc[] =
/* description of pslType */
{
    "unspecified",      // pslTypeUnspecified
    "protein-protein",  // pslTypeProtProt
    "protein-NA",       // pslTypeProtNa
    "NA-NA"             // pslTypeNaNa
};

static boolean pslTypeQueryIsProtein(enum pslType pslType)
/* is the pslType indicate the query is a protein? */
{
return (pslType == pslTypeProtProt) || (pslType == pslTypeProtNa);
}

static boolean pslTypeQueryIsNa(enum pslType pslType)
/* is the pslType indicate the query is a NA? */
{
return !pslTypeQueryIsProtein(pslType);
}

static boolean pslTypeTargetIsProtein(enum pslType pslType)
/* is the pslType indicate the target is a protein? */
{
return (pslType == pslTypeProtProt);
}

static boolean pslTypeTargetIsNa(enum pslType pslType)
/* is the pslType indicate the target is a NA? */
{
return !pslTypeTargetIsProtein(pslType);
}

struct block
/* Coordinates of a block, which might be aligned or a gap on one side */
{
    int qStart;          /* Query start position. */
    int qEnd;            /* Query end position. */
    int tStart;          /* Target start position. */
    int tEnd;            /* Target end position. */
};

static struct block ZERO_BLOCK = {0, 0, 0, 0};

static void assertBlockAligned(struct block *blk)
/* Make sure both sides are same size and not negative length.  */
{
assert(blk->qStart <= blk->qEnd);
assert(blk->tStart <= blk->tEnd);
assert((blk->qEnd - blk->qStart) == (blk->tEnd - blk->tStart));
}

static int blockIsAligned(struct block *blk)
/* check that both the query and target side have alignment */
{
return (blk->qEnd != 0) && (blk->tEnd != 0); // can start at zero
}

static int blockLength(struct block *blk)
/* get length of an aligned block  */
{
assertBlockAligned(blk);
return (blk->qEnd - blk->qStart);
}

static void setPslBoundsCounts(struct psl* psl)
/* set sequences bounds and counts from blocks on a PSL */
{
int lastBlk = psl->blockCount-1;

/* set start/end of sequences */
psl->qStart = psl->qStarts[0];
psl->qEnd = psl->qStarts[lastBlk] + psl->blockSizes[lastBlk];
if (pslQStrand(psl) == '-')
    reverseIntRange(&psl->qStart, &psl->qEnd, psl->qSize);

psl->tStart = psl->tStarts[0];
psl->tEnd = psl->tStarts[lastBlk] + psl->blockSizes[lastBlk];
if (pslTStrand(psl) == '-')
    reverseIntRange(&psl->tStart, &psl->tEnd, psl->tSize);

for (int iBlk = 0; iBlk < psl->blockCount; iBlk++)
    psl->match += psl->blockSizes[iBlk];
pslComputeInsertCounts(psl);
}

static void pslProtToNAConvert(struct psl *psl)
/* convert a protein/NA or protein/protein alignment to a NA/NA alignment */
{
boolean isProtNa = pslIsProtein(psl);
int iBlk;

psl->qStart *= 3;
psl->qEnd *= 3;
psl->qSize *= 3;
if (!isProtNa)
    psl->tSize *= 3;
for (iBlk = 0; iBlk < psl->blockCount; iBlk++)
    {
    psl->blockSizes[iBlk] *= 3;
    psl->qStarts[iBlk] *= 3;
    if (!isProtNa)
        psl->tStarts[iBlk] *= 3;
    }
setPslBoundsCounts(psl);
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
strand[2] = '\0';

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
assertBlockAligned(blk);

assert((blk->qEnd - blk->qStart) == (blk->tEnd - blk->tStart));
if (newIBlk >= *pslMax)
    pslGrow(psl, pslMax);
psl->qStarts[newIBlk] = blk->qStart;
psl->tStarts[newIBlk] = blk->tStart;
psl->blockSizes[newIBlk] = blk->qEnd - blk->qStart;
psl->blockCount++;
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
/* mRNA start in genomic gap before this block, this will
 * be an inPsl insert */
unsigned size = (align1Blk->tEnd < mqStart)
    ? (align1Blk->tEnd - align1Blk->tStart)
    : (mqStart - align1Blk->tStart);
struct block mappedBlk = ZERO_BLOCK;
mappedBlk.qStart = align1Blk->qStart;
mappedBlk.qEnd = align1Blk->qStart + size;
return mappedBlk;
}

static struct block getOverBlockMapping(unsigned mqStart, unsigned mqEnd,
                                        unsigned mtStart, struct block* align1Blk)
/* map part of an ungapped psl block that overlapps a mapPsl block. */
{
/* common sequence start contained in this block, this handles aligned
 * and genomic inserts */
unsigned off = align1Blk->tStart - mqStart;
unsigned size = (align1Blk->tEnd > mqEnd)
    ? (mqEnd - align1Blk->tStart)
    : (align1Blk->tEnd - align1Blk->tStart);
struct block mappedBlk = ZERO_BLOCK;
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

/* search for block or gap containing start of mrna block */
int iBlk = *iMapBlkPtr;
for (; iBlk < mapPsl->blockCount; iBlk++)
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
struct block mappedBlk = ZERO_BLOCK;
mappedBlk.qStart = align1Blk->qStart;
mappedBlk.qEnd = align1Blk->qEnd;
*iMapBlkPtr = iBlk;
return mappedBlk;
}

static void trimOverlapping(struct psl *mappedPsl, struct block *mappedBlk)
/* if this block overlaps the previous block, trim accordingly.  Overlaps
 * can be created with protein to DNA PSLs */
{
assertBlockAligned(mappedBlk);
assert(mappedPsl->blockCount > 0);

// use int so we can go negative
int prevQEnd = pslQEnd(mappedPsl, mappedPsl->blockCount - 1);
int prevTEnd = pslTEnd(mappedPsl, mappedPsl->blockCount - 1);

// trim, possibly setting to zero-length
int overAmt = max((prevQEnd - (int)mappedBlk->qStart), (prevTEnd - (int)mappedBlk->tStart));
if (overAmt < 0)
    overAmt = 0;
else if (overAmt > blockLength(mappedBlk))
    overAmt = blockLength(mappedBlk);

mappedBlk->qStart += overAmt;
mappedBlk->tStart += overAmt;
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
assert(align1Blk->qStart <= align1Blk->qEnd);
assert(align1Blk->tStart <= align1Blk->tEnd);
assert((align1Blk->qEnd - align1Blk->qStart) == (align1Blk->tEnd - align1Blk->tStart));

if ((align1Blk->qStart >= align1Blk->qEnd) || (align1Blk->tStart >= align1Blk->tEnd))
    return FALSE;  /* end of ungapped block. */

/* find block or gap with start coordinates of mrna */
struct block mappedBlk = getBlockMapping(inPsl, mapPsl, iMapBlkPtr, align1Blk);

/* Need to compute how much of input was consumed before trimming overlap */
unsigned consumed = (mappedBlk.qEnd != 0)
    ? (mappedBlk.qEnd - mappedBlk.qStart)
    : (mappedBlk.tEnd - mappedBlk.tStart);

/* remove overlap, which can happen with protein -> NA alignments */
if (blockIsAligned(&mappedBlk) && (mappedPsl->blockCount > 0))
    trimOverlapping(mappedPsl, &mappedBlk);

if (blockIsAligned(&mappedBlk))
    addPslBlock(mappedPsl, &mappedBlk, mappedPslMax);

/* advance past block or gap */
align1Blk->qStart += consumed;
align1Blk->tStart += consumed;
return TRUE;
}

struct psl* doMapping(struct psl *inPsl, struct psl *mapPsl)
/* do the mapping after adjustments made to input */
{
int mappedPslMax = 8; /* allocated space in output psl */
int iMapBlk = 0;
struct psl* mappedPsl = createMappedPsl(inPsl, mapPsl, mappedPslMax);

/* Fill in ungapped blocks.  */
for (int iBlock = 0; iBlock < inPsl->blockCount; iBlock++)
    {
    struct block align1Blk = blockFromPslBlock(inPsl, iBlock);
    while (mapBlock(inPsl, mapPsl, &iMapBlk, &align1Blk, mappedPsl,
                    &mappedPslMax))
        continue;
    }
assert(mappedPsl->blockCount <= mappedPslMax);

return mappedPsl;
}

static enum pslType determinePslType(struct psl *psl, enum pslType pslType, char *errDesc)
/* check the psl type if specified, set if unspecified */
{
boolean isProtNa = pslIsProtein(psl);
if (pslType == pslTypeUnspecified)
    return  isProtNa ? pslTypeProtNa : pslTypeNaNa;  // default

// check specified type
if (isProtNa &&  (pslType != pslTypeProtNa))
    errAbort("%s PSL %s to %s is a protein to NA alignment, however %s specified",
             errDesc, psl->qName, psl->tName, pslTypeDesc[pslType]);
    
if ((!isProtNa) &&  (pslType == pslTypeProtNa))
    errAbort("%s PSL %s to %s is not a protein to NA alignment, however %s was specified",
             errDesc, psl->qName, psl->tName, pslTypeDesc[pslType]);
return pslType;
}

static void checkInMapCompat(struct psl *inPsl, enum pslType inPslType,
                             struct psl *mapPsl, enum pslType mapPslType)
/* validate input and mapping alignments are compatible types */
{
/* allowed combinations:
 *     inPslType   mapPslType  outPslType
 *     na_na       na_na       na_na
 *     prot_prot   prot_prot   prot_prot
 *     prot_na     na_na       cds_na
 *     prot_prot   na_na       cds_na
 *     prot_prot   prot_na     cds_na
 */

if (!((pslTypeTargetIsNa(inPslType) && pslTypeQueryIsNa(mapPslType)) ||
      pslTypeTargetIsProtein(inPslType)))
    errAbort("input PSL %s to %s type %s is not compatible with mapping PSL %s to %s type %s ",
             inPsl->qName, inPsl->tName, pslTypeDesc[inPslType],
             mapPsl->qName, mapPsl->tName, pslTypeDesc[mapPslType]);
}
    
struct psl* pslTransMap(unsigned opts, struct psl *inPsl, enum pslType inPslType,
                        struct psl *mapPsl, enum pslType mapPslType)
/* map a psl via a mapping psl, a single psl is returned, or NULL if it
 * couldn't be mapped. psl types are normally set as pslTypeUnspecified,
 * and assumed to be NA->NA. */
{
inPslType = determinePslType(inPsl, inPslType, "input PSL");
mapPslType = determinePslType(mapPsl, mapPslType, "mapping PSL");
checkInMapCompat(inPsl, inPslType, mapPsl, mapPslType);

// check if need to swap strands
boolean rcInPsl = (pslTStrand(inPsl) != pslQStrand(mapPsl));

// need to convert all sides of both alignments to a common coordinate space
// all protein or all NA require no conversions.
boolean cnvIn = (inPslType == pslTypeProtNa) ||
    ((inPslType == pslTypeProtProt) && (mapPslType != pslTypeProtProt));
boolean cnvMap = (mapPslType == pslTypeProtNa) || (cnvIn && (mapPslType == pslTypeProtProt));

/* ensure common sequence is in same orientation and convert protein PSLs */
char inPslOrigStrand[3];
safef(inPslOrigStrand, sizeof(inPslOrigStrand), "%s", inPsl->strand);
if (cnvIn || rcInPsl)
    inPsl = pslClone(inPsl);
if (cnvIn)
    pslProtToNAConvert(inPsl);
if (rcInPsl)
    pslRc(inPsl);
if (cnvMap)
    {
    mapPsl = pslClone(mapPsl);
    pslProtToNAConvert(mapPsl);
    }

/* sanity check sizes after conversion Don't check name, so names to vary to
 * allow ids to have unique-ifying suffixes. */
if (inPsl->tSize != mapPsl->qSize)
    errAbort("Error: inPsl %s tSize (%d) != mapPsl %s qSize (%d)",
            inPsl->tName, inPsl->tSize, mapPsl->qName, mapPsl->qSize);


struct psl* mappedPsl = doMapping(inPsl, mapPsl);

/* finish up psl, or free if no blocks were added */
if (mappedPsl->blockCount == 0)
    pslFree(&mappedPsl);  /* nothing made it */
else
    {
    setPslBoundsCounts(mappedPsl);
    adjustOrientation(opts, inPsl, inPslOrigStrand, mappedPsl);
    }

if (cnvIn || rcInPsl)
    pslFree(&inPsl);
if (cnvMap)
    pslFree(&mapPsl);

return mappedPsl;
}

