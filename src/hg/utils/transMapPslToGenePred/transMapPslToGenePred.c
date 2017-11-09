/* transMapPslToGenePred - convert PSL alignments of mRNAs to gene annotation.
 */

/* Copyright (C) 2016 The Regents of the University of California
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "options.h"
#include "genePred.h"
#include "genePredReader.h"
#include "psl.h"
#include "hash.h"
#include "dnautil.h"

/*
 * Notes:
 *  - Must dealt with block structure that is different than the source genePred.  TransMap
 *    may split or merge blocks.   This builds a genePred with the number of blocks generated
 *    by intersecting the two block structures and then packs the genePred at the end.
 *  - Strand may have changed, so this must be handled with all query coordinates.
 */



void usage()
/* Explain usage and exit. */
{
errAbort(
  "transMapPslToGenePred - convert PSL alignments of mRNAs to gene annotations.\n"
  "\n"
  "usage:\n"
  "   mrnaToGene [options] sourceGenePred mappedPsl mappedGenePred\n"
  "\n"

  "Convert PSL alignments from transmap to genePred.  It specifically handles\n"
  "alignments where the source genes are genomic annotations in genePred\n"
  "format, that are converted to PSL for mapping and using this program to\n"
  "create a new genePred.\n"
  "\n"
  "This is an alternative to mrnaToGene which determines CDS and frame from\n"
  "the original annotation, which may have been imported from GFF/GTF.  This\n"
  "was created because the genbankCds structure use by mrnaToGene doesn't\n"
  "handle partial start/stop codon or programmed frame shifts.  This requires\n"
  "handling the list of CDS regions and the /codon_start attribute,  At some\n"
  "point, this program may be extended to do handle genbank alignments correctly.\n"
  "\n"
  "Options:\n"
  "  -nonCodingGapFillMax=0 - fill gaps in non-coding regions up to this many bases\n"
  "   in length.\n"
  "  -codingGapFillMax=0 - fill gaps in coding regions up to this many bases\n"
  "   in length.  Only coding gaps that are a multiple of three will be fill,\n"
  "   with the max rounded down.\n"
  "  -noBlockMerge - don't do any block merging of genePred, even of adjacent blocks.\n"
  "   This is mainly for debugging.\n"
  "\n");
}

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"nonCodingGapFillMax", OPTION_INT},
    {"codingGapFillMax", OPTION_INT},
    {"noBlockMerge", OPTION_BOOLEAN},
    {NULL, 0}
};
static int nonCodingGapFillMax = 0;
static int codingGapFillMax = 0;
static boolean noBlockMerge = FALSE;

static void swapBoolean(boolean *a, boolean *b)
/* swap two booleans */
{
boolean hold = *a;
*a = *b;
*b = hold;
}

static int frameIncr(int frame, int amt)
/* increment an interger frame by positive or negative amount. Frame of -1
 * already returns -1. */
{
if (frame < 0)
    return frame;  // no frame not changed
else if (amt >= 0)
    return (frame + amt) % 3;
else
    {
    int amt3 = (-amt) % 3;
    return (frame - (amt - amt3)) % 3;
    }
}

static int genePredExonSize(struct genePred* gp, int iExon)
/* calculate size of an exon in a genePred */
{
return gp->exonEnds[iExon] - gp->exonStarts[iExon];
}

static int genePredSize(struct genePred* gp)
/* calculate size of an all of the exons in a genePred */
{
int iExon, size = 0;
for (iExon = 0; iExon < gp->exonCount; iExon++)
    size += genePredExonSize(gp, iExon);
return size;
}

static int pslBlockQueryToTarget(struct psl *psl, int iBlock, int pos)
/* project a position from query to target.  Position maybe a start
 * or end */
{
assert((pslQStart(psl, iBlock) <= pos) && (pos <= pslQEnd(psl, iBlock)));
return pslTStart(psl, iBlock) + pos - pslQStart(psl, iBlock);
}

static boolean srcGenePredCheckSameStruct(struct genePred *gp,
                                          struct genePred *gp0)
/* check that duplicate genePred frames and exon sizes are the same (PAR
 * case). */
{
int iExon;
if (gp->exonCount != gp0->exonCount)
    return FALSE;
if (genePredCdsSize(gp)!= genePredCdsSize(gp0))
    return FALSE;
for (iExon = 0; iExon < gp->exonCount; iExon++)
    {
    if ((genePredExonSize(gp, iExon) != genePredExonSize(gp0, iExon))
        || (gp->exonFrames[iExon] != gp0->exonFrames[iExon]))
        return FALSE;
    }
return TRUE;
}

static void srcGenePredAdd(struct hash* srcGenePredMap,
                           struct genePred *gp)
/* add source, if duplicate, validate that structure is the same if duplicate,
 * which handles PAR. */
{
if (gp->exonFrames == NULL)
    errAbort("genePred does not have exonFrames: %s", gp->name);
struct hashEl *hel = hashStore(srcGenePredMap, gp->name);
if (hel->val == NULL)
    hel->val = gp;
else
    {
    if (!srcGenePredCheckSameStruct(gp, (struct genePred *)hel->val))
        errAbort("genePred structure or frames don't match for multiple occurrences of %s", gp->name);
    genePredFree(&gp);
    }
}

static struct hash* srcGenePredLoad(char* srcGenePredFile)
/* load genepreds into a hash by name */
{
struct hash* srcGenePredMap = hashNew(0);
struct genePred *gp, *gps = genePredReaderLoadFile(srcGenePredFile, NULL);
while ((gp = slPopHead(&gps)) != NULL)
    srcGenePredAdd(srcGenePredMap, gp);
return srcGenePredMap;
}

static struct genePred* srcGenePredFind(struct hash* srcGenePredMap,
                                        char* qName, int qSize)
{
// ignore unique suffix everything after the last
char qNameBase[512];
safecpy(qNameBase, sizeof(qNameBase), qName);
char *dash = strrchr(qNameBase, '-');
if (dash != NULL)
    *dash = '\0';
struct genePred* gp = hashFindVal(srcGenePredMap, qNameBase);
if (gp == NULL)
    errAbort("can't find source genePred for %s with name %s", qName, qNameBase);
if (genePredSize(gp) != qSize)
    errAbort("size of query computed from genePred %s (%d), doesn't match expected %s (%d)",
             qNameBase, genePredSize(gp), qName, qSize);
return gp;
}

struct range
/* A range for function returns */
{
    int start;
    int end;
};

struct srcQueryExon
/* source query exon ranges, cds, and frame.  These are all in to positive
 * strand coordinates. */
{
    int qStart;         // computed query range of exon
    int qEnd;
    int qSize;
    int qCdsStart;      // query range of CDS in exon
    int qCdsEnd;
    int frame;
    boolean hasCdsStart;   // is this the start/end of full cds?
    boolean hasCdsEnd;
};

static void srcQueryExonMakeCdsPos(struct genePred* srcGp,
                                   int qStart, int iExon, int qSize,
                                   struct range tCds, struct srcQueryExon* srcQueryExon)
/* fill in srcQueryExon CDS information for the positive strand */
{
// map target CDS range in exon to query
srcQueryExon->qCdsStart = qStart + (tCds.start - srcGp->exonStarts[iExon]);
srcQueryExon->qCdsEnd = qStart + (tCds.end - srcGp->exonStarts[iExon]);

// is full cds being/end in this exon?
srcQueryExon->hasCdsStart = (srcGp->exonStarts[iExon] <= srcGp->cdsStart)
    && (srcGp->cdsStart < srcGp->exonEnds[iExon]);
srcQueryExon->hasCdsEnd = (srcGp->exonStarts[iExon] < srcGp->cdsEnd)
    && (srcGp->cdsEnd <= srcGp->exonEnds[iExon]);
}

static void srcQueryExonMakeCdsNeg(struct genePred* srcGp,
                                   int qEnd, int iExon, int qSize,
                                   struct range tCds, struct srcQueryExon* srcQueryExon)
/* fill in srcQueryExon CDS information for the negative strand */
{
// map target CDS range in exon to query
srcQueryExon->qCdsStart = qEnd - (tCds.end - srcGp->exonStarts[iExon]);
srcQueryExon->qCdsEnd = qEnd - (tCds.start - srcGp->exonStarts[iExon]);

// is full cds being/end in this exon?
srcQueryExon->hasCdsStart = (srcGp->exonStarts[iExon] < srcGp->cdsEnd)
    && (srcGp->cdsEnd <= srcGp->exonEnds[iExon]);
srcQueryExon->hasCdsEnd = (srcGp->exonStarts[iExon] <= srcGp->cdsStart)
    && (srcGp->cdsStart < srcGp->exonEnds[iExon]);
}

static int srcQueryExonMake(struct genePred* srcGp,
                            int qStart, int iExon, int qSize,
                            struct srcQueryExon* srcQueryExon)
/* update CDS and frame info for an exon and return next qStart */
{
int qEnd = qStart + genePredExonSize(srcGp, iExon);
srcQueryExon->qStart = qStart;
srcQueryExon->qEnd = qEnd;
srcQueryExon->qSize = qSize;
srcQueryExon->frame = srcGp->exonFrames[iExon];

// find what part of exon is in CDS
struct range tCds = {max(srcGp->exonStarts[iExon], srcGp->cdsStart),
                     min(srcGp->exonEnds[iExon], srcGp->cdsEnd)};
if (tCds.start >= tCds.end)
    {
    // no CDS in exon
    srcQueryExon->qCdsStart = srcQueryExon->qCdsEnd = 0;
    srcQueryExon->hasCdsStart = srcQueryExon->hasCdsEnd = FALSE;
    }
else
    {
    // part or all of exon is CDS
    if (srcGp->strand[0] == '+')
        srcQueryExonMakeCdsPos(srcGp, qStart, iExon, qSize, tCds, srcQueryExon);
    else
        srcQueryExonMakeCdsNeg(srcGp, qEnd, iExon, qSize, tCds, srcQueryExon);
    assert(srcQueryExon->qCdsStart < srcQueryExon->qCdsEnd);
    assert(srcQueryExon->qStart <= srcQueryExon->qCdsStart);
    assert(srcQueryExon->qCdsEnd <= srcQueryExon->qEnd);
    assert(srcQueryExon->frame >= 0);
    }
return qEnd;
}

static void srcQueryExonBuild(struct genePred* srcGp,
                              struct srcQueryExon* srcQueryExons)
/* create a list of query positions to frames.  Array should hold all
 * be the length of the src */
{
// building in strand-order generates correct coordinate
int qStart = 0;
int qSize = genePredSize(srcGp);
int iExon;
if (srcGp->strand[0] == '+')
    {
    for (iExon = 0; iExon < srcGp->exonCount; iExon++)
        qStart = srcQueryExonMake(srcGp, qStart, iExon, qSize, &(srcQueryExons[iExon]));
    }
else
    {
    for (iExon = srcGp->exonCount-1; iExon >= 0; iExon--)
        qStart = srcQueryExonMake(srcGp, qStart, iExon, qSize, &(srcQueryExons[iExon]));
    }
}

struct srcQueryExon srcQueryExonReverse(struct srcQueryExon* srcQueryExon)
/* reverse a srcQueryExon structure */
{
struct srcQueryExon rev = *srcQueryExon;
reverseIntRange(&rev.qStart, &rev.qEnd, rev.qSize);
if (rev.qCdsStart < rev.qCdsEnd)
    reverseIntRange(&rev.qCdsStart, &rev.qCdsEnd, rev.qSize);
// note: frame is always in direction of transcription
rev.frame = srcQueryExon->frame;
swapBoolean(&rev.hasCdsStart, &rev.hasCdsEnd);
return rev;
}

static struct srcQueryExon* srcQueryExonFind(struct genePred* srcGp,
                                             struct srcQueryExon* srcQueryExons,
                                             int qStart)
/* find srcQueryExon object containing the mapped block. qStart is positive strand */
{
int iExon;
for (iExon = 0; iExon < srcGp->exonCount; iExon++)
    {
    if ((srcQueryExons[iExon].qStart <= qStart) && (qStart < srcQueryExons[iExon].qEnd))
        return srcQueryExons + iExon;
    }
errAbort("could't find find mapped query block start %d in sourceGenePred %s", qStart, srcGp->name);
return NULL;
}

static struct srcQueryExon srcQueryExonFindStrand(struct genePred* srcGp,
                                                  struct srcQueryExon* srcQueryExons,
                                                  int qStart, char qStrand)
/* find srcQueryExon object containing the mapped block, mapping to the desired strand.
 *  qStart is on the specified strand. */
{
int qStartPos = (qStrand == '+') ? qStart : (srcQueryExons[0].qSize - qStart)-1;
struct srcQueryExon srcQueryExon = *srcQueryExonFind(srcGp, srcQueryExons, qStartPos);
if (qStrand == '-')
    srcQueryExon = srcQueryExonReverse(&srcQueryExon);
return srcQueryExon;
}

static struct range srcQueryExonMappedQueryCds(struct srcQueryExon* srcQueryExon,
                                               struct range mappedQRange)
/* find intersection of CDS in mapped block with the query, start >= end if no intersection. */
{
struct range mappedQCds;
mappedQCds.start = max(srcQueryExon->qCdsStart, mappedQRange.start);
mappedQCds.end = min(srcQueryExon->qCdsEnd, mappedQRange.end);
return mappedQCds;
}

struct mappedCdsBounds
/* used to record either the mapping of the start or end of the CDS so bounds and cdsStatus can
 * be recorded once the whole gene is mapped. */
{
    int cdsStart;               // start of CDS in mapped
    boolean cdsStartIsMapped;   // was the source CDS start mapped?
    int cdsEnd;                 // end of CDS in mapped
    boolean cdsEndIsMapped;     // was the source cds end mapped?
};
static struct mappedCdsBounds mappedCdsBoundsInit = {-1, FALSE, -1, FALSE};

static void convertPslBlockCdsStart(struct psl *mappedPsl, int iBlock, struct genePred* srcGp,
                                    struct srcQueryExon* srcQueryExon, struct range mappedQCds,
                                    struct genePred *mappedGp, struct mappedCdsBounds* mappedCdsBounds)
/* set CDS start info on first CDS block that was mapped */
{
mappedCdsBounds->cdsStart = pslBlockQueryToTarget(mappedPsl, iBlock, mappedQCds.start);

// Is the query CDS start is in this first block and the src CDS contained
// in this mapped block?
if (srcQueryExon->hasCdsStart && (mappedQCds.start == srcQueryExon->qCdsStart))
    mappedCdsBounds->cdsStartIsMapped = TRUE;
}

static void convertPslBlockCdsEnd(struct psl *mappedPsl, int iBlock, struct genePred* srcGp,
                                  struct srcQueryExon* srcQueryExon, struct range mappedQCds,
                                  struct genePred *mappedGp, struct mappedCdsBounds* mappedCdsBounds)
/* update CDS end info, called in order, on all blocks with CDS */
{
mappedCdsBounds->cdsEnd = pslBlockQueryToTarget(mappedPsl, iBlock, mappedQCds.end);

// Is the query CDS end is in this block and the src CDS contained
// in this mapped block?
if (srcQueryExon->hasCdsEnd && (mappedQCds.end == srcQueryExon->qCdsEnd))
    mappedCdsBounds->cdsEndIsMapped = TRUE;
}

static void convertPslBlockCds(struct psl *mappedPsl, int iBlock, struct genePred* srcGp,
                               struct srcQueryExon* srcQueryExon, struct range mappedQCds,
                               struct genePred *mappedGp, int iExon, struct mappedCdsBounds* mappedCdsBounds)
/* Update CDS bounds and frame for a block with CDS */
{
// on the positive strand, frame is adjusted from the start, on the negative strand, from the end.
int cdsOff = (pslQStrand(mappedPsl) == '+')
    ? (mappedQCds.start - srcQueryExon->qCdsStart)
    : (srcQueryExon->qCdsEnd - mappedQCds.end);
mappedGp->exonFrames[iExon] = frameIncr(srcQueryExon->frame, cdsOff);

if (mappedCdsBounds->cdsStart < 0)
    convertPslBlockCdsStart(mappedPsl, iBlock, srcGp, srcQueryExon, mappedQCds, mappedGp, mappedCdsBounds);
convertPslBlockCdsEnd(mappedPsl, iBlock, srcGp, srcQueryExon, mappedQCds, mappedGp, mappedCdsBounds);
}

static int convertPslBlockRegion(struct psl *mappedPsl, int iBlock, int qNext, struct genePred* srcGp,
                                 struct srcQueryExon* srcQueryExons, struct genePred *mappedGp,
                                 unsigned *currentExonSpace, struct mappedCdsBounds* mappedCdsBounds)
/* Convert a region of one block to an genePred exon, return the offset of the
 * next part of the block to convert.  Regions must be converted because
 * blocks can be merged by transmap and not have a one-to-one mapping to the
 * source genePred regions.
 */
{
int iExon = mappedGp->exonCount;
if (iExon >= *currentExonSpace)
    genePredGrow(mappedGp, currentExonSpace);

// NOTE: all query coordinates are converted to mappedPsl query strand

// determine intersection range of mapped psl block regions and source genePred
struct srcQueryExon srcQueryExon = srcQueryExonFindStrand(srcGp, srcQueryExons, qNext, pslQStrand(mappedPsl));
struct range mappedQRange = {qNext, min(pslQEnd(mappedPsl, iBlock), srcQueryExon.qEnd)};
assert(mappedQRange.start < mappedQRange.end);

struct range mappedQCds = srcQueryExonMappedQueryCds(&srcQueryExon, mappedQRange);
if (mappedQCds.start < mappedQCds.end)
    convertPslBlockCds(mappedPsl, iBlock, srcGp, &srcQueryExon, mappedQCds, mappedGp, iExon, mappedCdsBounds);
else
    mappedGp->exonFrames[iExon] = -1; // no CDS in block
mappedGp->exonStarts[iExon] = pslBlockQueryToTarget(mappedPsl, iBlock, mappedQRange.start);
mappedGp->exonEnds[iExon] = pslBlockQueryToTarget(mappedPsl, iBlock, mappedQRange.end);
mappedGp->exonCount++;
return mappedQRange.end;
}

static void convertPslBlock(struct psl *mappedPsl, int iBlock, struct genePred* srcGp,
                            struct srcQueryExon* srcQueryExons, struct genePred *mappedGp,
                            unsigned *currentExonSpace, struct mappedCdsBounds* mappedCdsBounds)
/* convert one block to an genePred exon, including setting frame and CDS bounds */
{
int qNext = pslQStart(mappedPsl, iBlock);
while (qNext < pslQEnd(mappedPsl, iBlock))
    qNext = convertPslBlockRegion(mappedPsl, iBlock, qNext, srcGp, srcQueryExons, mappedGp, currentExonSpace, mappedCdsBounds);
}

static void finishWithCds(struct genePred* srcGp, struct genePred *mappedGp, struct mappedCdsBounds* mappedCdsBounds)
/* finish CDS annotation when we have a CDS mapped */
{
mappedGp->cdsStart = mappedCdsBounds->cdsStart;
mappedGp->cdsEnd = mappedCdsBounds->cdsEnd;

// copy initial status from source, noting that start/end is in chromosome coordinates
if (sameString(srcGp->strand, mappedGp->strand))
    {
    mappedGp->cdsStartStat = srcGp->cdsStartStat;
    mappedGp->cdsEndStat = srcGp->cdsEndStat;
    }
else
    {
    mappedGp->cdsStartStat = srcGp->cdsEndStat;
    mappedGp->cdsEndStat = srcGp->cdsStartStat;
    }

// If start/end didn't get mapped, force to incomplete
if (!mappedCdsBounds->cdsStartIsMapped)
    mappedGp->cdsStartStat = cdsIncomplete;
if (!mappedCdsBounds->cdsEndIsMapped)
    mappedGp->cdsEndStat = cdsIncomplete;
}

static void finishWithOutCds(struct genePred *mappedGp)
/* finish CDS annotation when we have don't have a CDS mapped (non-coding or CDS regions didn't map */
{
// use common representation of no CDS.
mappedGp->cdsStart = mappedGp->cdsEnd = mappedGp->txEnd;
mappedGp->cdsStartStat = mappedGp->cdsEndStat = cdsNone;
}

static struct genePred* createGenePred(struct genePred* srcGp, struct srcQueryExon *srcQueryExons,
                                       struct psl *mappedPsl)
/* create genePred from mapped PSL */
{
struct mappedCdsBounds mappedCdsBounds = mappedCdsBoundsInit;
unsigned currentExonSpace = mappedPsl->blockCount;
// setup cdsStart and cdsEnd as txEnd, 0 to indicate not yet set
struct genePred *mappedGp = genePredNew(mappedPsl->qName, mappedPsl->tName, pslQStrand(mappedPsl),
                                        mappedPsl->tStart, mappedPsl->tEnd,
                                        mappedPsl->tEnd, 0,
                                        genePredAllFlds, currentExonSpace);
mappedGp->score = srcGp->score;
mappedGp->name2 = cloneString(srcGp->name2);

int iBlock;
for (iBlock = 0; iBlock < mappedPsl->blockCount; iBlock++)
    convertPslBlock(mappedPsl, iBlock, srcGp, srcQueryExons, mappedGp, &currentExonSpace,
                    &mappedCdsBounds);
if (mappedCdsBounds.cdsStart < 0)
    finishWithOutCds(mappedGp);
else
    finishWithCds(srcGp, mappedGp, &mappedCdsBounds);
return mappedGp;
}

static boolean haveAdjacentMergedFrames(struct genePred *mappedGp, int iExon)
/* Do two block have adjacent frames and will merging them keep frames contiguous?  */
{
assert(mappedGp->exonFrames[iExon] >= 0);
assert(mappedGp->exonFrames[iExon+1] >= 0);
int gapSize = (mappedGp->exonStarts[iExon+1] - mappedGp->exonEnds[iExon]);
if (mappedGp->strand[0] == '+')
    {
    // how much of CDS is in this exon
    int cdsOff = max(mappedGp->exonStarts[iExon], mappedGp->cdsStart) - mappedGp->exonStarts[iExon];
    int cdsIncr = (genePredExonSize(mappedGp, iExon) - cdsOff) + gapSize;
    return (frameIncr(mappedGp->exonFrames[iExon], cdsIncr) == mappedGp->exonFrames[iExon+1]);
    }
else
    {
    // how much of CDS is in next exon
    int cdsOff = mappedGp->exonEnds[iExon+1] - min(mappedGp->exonEnds[iExon+1], mappedGp->cdsEnd);
    int cdsIncr = (genePredExonSize(mappedGp, iExon+1) - cdsOff)  + gapSize;
    return (frameIncr(mappedGp->exonFrames[iExon+1], cdsIncr) == mappedGp->exonFrames[iExon]);
    }
}

static boolean hasAdjacentNonCoding(struct genePred *mappedGp, int iExon)
/* are one or both adjacent blocks non-coding? */
{
return (mappedGp->exonFrames[iExon] == -1) || (mappedGp->exonFrames[iExon+1] == -1);
}

static boolean canMergeFrames(struct genePred *mappedGp, int iExon)
/* check if frames can be merged, -1 frames merge with with any adjacent
 * frame */
{
return hasAdjacentNonCoding(mappedGp, iExon) || haveAdjacentMergedFrames(mappedGp, iExon);
}

static boolean canMergeBlocks(struct genePred *mappedGp, int iExon)
/* check if blocks are adjacent and can be merged with consistent frame */
{
int maxGapFill = hasAdjacentNonCoding(mappedGp, iExon)
    ? nonCodingGapFillMax : codingGapFillMax;
return ((mappedGp->exonStarts[iExon+1] - mappedGp->exonEnds[iExon]) <= maxGapFill)
        && canMergeFrames(mappedGp, iExon);
}

static void mergeAdjacentFrames(struct genePred *mappedGp, int iExon)
/* merge frames for two adjacent blocks that are being merged */
{
// update frame, handling case were one block has CDS and the other doesn't
if (mappedGp->strand[0] == '+')
    {
    // + strand, keep first frame
    mappedGp->exonFrames[iExon] = (mappedGp->exonFrames[iExon] >= 0)
        ? mappedGp->exonFrames[iExon]
        : mappedGp->exonFrames[iExon+1];
    }
else
    {
    // - strand, keep second frame
    mappedGp->exonFrames[iExon] = (mappedGp->exonFrames[iExon+1] >= 0)
        ? mappedGp->exonFrames[iExon+1]
        : mappedGp->exonFrames[iExon];
    }
}

static void shiftBlock(struct genePred *mappedGp, int iExon)
/* shift up block arrays by one, overwriting iExon entry */
{
mappedGp->exonStarts[iExon] = mappedGp->exonStarts[iExon+1];
mappedGp->exonEnds[iExon] = mappedGp->exonEnds[iExon+1];
mappedGp->exonFrames[iExon] = mappedGp->exonFrames[iExon+1];
}

static void mergeAdjacentBlocks(struct genePred *mappedGp, int iExon)
/* merge two blocks that are adjacent */
{
mappedGp->exonEnds[iExon] = mappedGp->exonEnds[iExon+1];
mergeAdjacentFrames(mappedGp, iExon);

// move subsequent blocks
for (iExon++; iExon < mappedGp->exonCount-1; iExon++)
    shiftBlock(mappedGp, iExon);
mappedGp->exonCount--;
}

static void mergeGenePredBlocks(struct genePred *mappedGp)
/* merge adjacent genePred `exons' as long as they have consistent frame */
{
int iExon = 0;
while (iExon < mappedGp->exonCount-1)
    {
    if (canMergeBlocks(mappedGp, iExon))
        mergeAdjacentBlocks(mappedGp, iExon);
    else
        iExon++;
    }
}

static void convertPsl(struct hash* srcGenePredMap, struct psl *mappedPsl, FILE* genePredOutFh)
/* convert a single PSL */
{
if (pslTStrand(mappedPsl) == '-')
    pslRc(mappedPsl); // target must be `+'

struct genePred* srcGp = srcGenePredFind(srcGenePredMap, mappedPsl->qName, mappedPsl->qSize);
if (genePredBases(srcGp) != mappedPsl->qSize)
    errAbort("srcGenePred %s exon size %d does not match mappedPsl %s qSize %d",
             srcGp->name, genePredBases(srcGp), mappedPsl->qName, mappedPsl->qSize);
struct srcQueryExon srcQueryExons[srcGp->exonCount];
srcQueryExonBuild(srcGp, srcQueryExons);

struct genePred *mappedGp = createGenePred(srcGp, srcQueryExons, mappedPsl);
if (!noBlockMerge)
    mergeGenePredBlocks(mappedGp);
if (genePredCheck("mappedGenePred", stderr, -1, mappedGp))
    errAbort("invalid genePred created");

genePredTabOut(mappedGp, genePredOutFh);
genePredFree(&mappedGp);
}

static void transMapPslToGenePred(char* srcGenePredFile, char* mappedPslFile,
                                  char* mappedGenePredFile)
/* convert mapped psl to genePreds */
{
struct hash* srcGenePredMap = srcGenePredLoad(srcGenePredFile);
struct lineFile *pslFh = pslFileOpen(mappedPslFile);
FILE* genePredOutFh = mustOpen(mappedGenePredFile, "w");
struct psl *mappedPsl;
while ((mappedPsl = pslNext(pslFh)) != NULL)
    {
    convertPsl(srcGenePredMap, mappedPsl, genePredOutFh);
    pslFree(&mappedPsl);
    }

carefulClose(&genePredOutFh);
lineFileClose(&pslFh);
hashFreeWithVals(&srcGenePredMap, genePredFree);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 4)
    usage();
nonCodingGapFillMax = optionInt("nonCodingGapFillMax", nonCodingGapFillMax);
codingGapFillMax = optionInt("codingGapFillMax", codingGapFillMax);
codingGapFillMax = (codingGapFillMax/3)*3; // round
noBlockMerge = optionExists("noBlockMerge");

char *srcGenePredFile = argv[1];
char *mappedPslFile = argv[2];
char *mappedGenePredFile = argv[3];
transMapPslToGenePred(srcGenePredFile, mappedPslFile, mappedGenePredFile);
return 0;
}
