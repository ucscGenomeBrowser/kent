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
  "alignments where the source gene is an genomic annotations in genePred\n"
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
  "\n");
}

/*
 * Notes:
 *   - if pslMap mergers blocks and the frame is inconsistent with the source of the merged blocks, the genePred
 *     block will not be split to store frameshifts. 
 */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

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
    if (((gp->exonEnds[iExon] - gp->exonStarts[iExon])
         != (gp0->exonEnds[iExon] - gp0->exonStarts[iExon]))
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
                                        char* qName)
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
return gp;
}

struct range
/* A range for function returns */
{
    int start;
    int end;
};

struct srcQueryExon
/* source query exon ranges, cds, and frame. */
{
    int qStart;    // computed query range of exon
    int qEnd;
    int qCdsStart; // query range of CDS in exon
    int qCdsEnd;
    boolean cdsBegin;  // is the the start/end of full cds?
    boolean cdsEnd;
    int frame;
};

static int srcQueryExonMake(struct genePred* srcGp,
                            int qStart, int iExon,
                            struct srcQueryExon* srcQueryExon)
/* update CDS and frame info for an exon and return next qStart */
{
int qEnd = qStart + (srcGp->exonEnds[iExon] - srcGp->exonStarts[iExon]);
srcQueryExon->qStart = qStart;
srcQueryExon->qEnd = qEnd;
srcQueryExon->frame = srcGp->exonFrames[iExon];

// find what part of exon is in CDS
int tCdsStart = max(srcGp->exonStarts[iExon], srcGp->cdsStart);
int tCdsEnd = min(srcGp->exonEnds[iExon], srcGp->cdsEnd);

if (tCdsStart >= tCdsEnd)
    {
    // no CDS in exon
    srcQueryExon->qCdsStart = srcQueryExon->qCdsEnd = 0;
    srcQueryExon->cdsBegin = srcQueryExon->cdsEnd = FALSE;
    }
else
    {
    // part or all of exon is CDS
    srcQueryExon->qCdsStart = qStart + (tCdsStart - srcGp->exonStarts[iExon]);
    srcQueryExon->qCdsEnd =  qStart + (tCdsEnd - srcGp->exonStarts[iExon]);
    // is full cds being/end in this exon?
    srcQueryExon->cdsBegin = (srcGp->exonStarts[iExon] <= srcGp->cdsStart)
        && (srcGp->cdsStart < srcGp->exonEnds[iExon]);
    srcQueryExon->cdsEnd = (srcGp->exonStarts[iExon] < srcGp->cdsEnd)
        && (srcGp->cdsEnd <= srcGp->exonEnds[iExon]);
    }
return qEnd;
}

static void srcQueryExonBuild(struct genePred* srcGp,
                              struct srcQueryExon* srcQueryExons)
/* create a list of query positions to frames.  Array should hold all
 * be the length of the src */
{
int qStart = 0;
int iExon;
for (iExon = 0; iExon < srcGp->exonCount; iExon++)
    qStart = srcQueryExonMake(srcGp, qStart, iExon, &(srcQueryExons[iExon]));
}

static struct srcQueryExon* srcQueryExonFind(struct genePred* srcGp,
                                             struct srcQueryExon* srcQueryExons,
                                             struct psl *mappedPsl, int iBlock)
/* find srcQueryExon object containing the mapped block  */
{
int qStart = pslQStart(mappedPsl, iBlock);
int iExon;
for (iExon = 0; iExon < srcGp->exonCount; iExon++)
    {
    if ((srcQueryExons[iExon].qStart <= qStart) && (qStart < srcQueryExons[iExon].qEnd))
        {
        return srcQueryExons + iExon;
        }
    }
errAbort("could't find find mapped query block start %d in sourceGenePred %s", qStart, srcGp->name);
return NULL;
}

static struct range srcQueryExonMappedQueryCds(struct srcQueryExon* srcQueryExon,
                                               struct psl *mappedPsl, int iBlock)
/* find intersection of CDS in mapped block with the query, start >= end if no intersection */
{
struct range mappedQCds;
mappedQCds.start = max(srcQueryExon->qCdsStart, pslQStart(mappedPsl, iBlock));
mappedQCds.end = min(srcQueryExon->qCdsEnd, pslQEnd(mappedPsl, iBlock));
return mappedQCds;
}

static void convertPslBlockCdsStart(struct psl *mappedPsl, int iBlock, struct genePred* srcGp,
                                    struct srcQueryExon* srcQueryExon, struct range mappedQCds,
                                    struct genePred *mappedGp)
/* set CDS start info on first CDS block */
{
mappedGp->cdsStart = pslBlockQueryToTarget(mappedPsl, iBlock, mappedQCds.start);
// complete if query cds start is in this first block and src is complete
boolean isComplete = (srcQueryExon->qCdsStart == mappedQCds.start) && srcQueryExon->cdsBegin;
mappedGp->cdsStartStat = (isComplete && (srcGp->cdsStartStat == cdsComplete)) ? cdsComplete : cdsIncomplete;
}
    
static void convertPslBlockCdsEnd(struct psl *mappedPsl, int iBlock, struct genePred* srcGp,
                                  struct srcQueryExon* srcQueryExon, struct range mappedQCds,
                                  struct genePred *mappedGp)
/* update CDS end info, called in order, on all blocks with CDS */
{
mappedGp->cdsEnd = pslBlockQueryToTarget(mappedPsl, iBlock, mappedQCds.end);
// complete if query cds is in the block and src is complete, subsequent blocks update this till end reached
boolean isComplete = (srcQueryExon->qCdsEnd == mappedQCds.end) && srcQueryExon->cdsEnd;
mappedGp->cdsEndStat = (isComplete && (srcGp->cdsEndStat == cdsComplete)) ? cdsComplete : cdsIncomplete;
}
    
static void convertPslBlockCds(struct psl *mappedPsl, int iBlock, struct genePred* srcGp,
                               struct srcQueryExon* srcQueryExon, struct range mappedQCds,
                               struct genePred *mappedGp, int iExon)
/* Update CDS bounds and frame for a block with CDS */
{
int cdsOff = (mappedQCds.start - srcQueryExon->qCdsStart);

int frame = frameIncr(srcQueryExon->frame, cdsOff);
mappedGp->exonFrames[iExon] = frame;
if (mappedGp->cdsStart == mappedGp->txEnd)
    convertPslBlockCdsStart(mappedPsl, iBlock, srcGp, srcQueryExon, mappedQCds, mappedGp);
convertPslBlockCdsEnd(mappedPsl, iBlock, srcGp, srcQueryExon, mappedQCds, mappedGp);
}

static void convertPslBlock(struct psl *mappedPsl, int iBlock, struct genePred* srcGp,
                            struct srcQueryExon* srcQueryExons, struct genePred *mappedGp)
/* convert one block to an genePred exon, including setting frame and CDS bounds */
{
int iExon = mappedGp->exonCount;
struct srcQueryExon* srcQueryExon = srcQueryExonFind(srcGp, srcQueryExons, mappedPsl, iBlock);
struct range mappedQCds = srcQueryExonMappedQueryCds(srcQueryExon, mappedPsl, iBlock);
if (mappedQCds.start < mappedQCds.end)
    convertPslBlockCds(mappedPsl, iBlock, srcGp, srcQueryExon, mappedQCds, mappedGp, iExon);
else
    mappedGp->exonFrames[iExon] = -1; // no CDS in block
mappedGp->exonStarts[iExon] = pslTStart(mappedPsl, iBlock);
mappedGp->exonEnds[iExon] = pslTEnd(mappedPsl, iBlock);
mappedGp->exonCount++;
}

static struct genePred* createGenePred(struct genePred* srcGp, struct srcQueryExon *srcQueryExons,
                                       struct psl *mappedPsl)
/* create genePred from mapped PSL */
{
// setup cdsStart and cdsEnd as txEnd, 0 to indicate not yet set
struct genePred *mappedGp = genePredNew(mappedPsl->qName, mappedPsl->tName, pslQStrand(mappedPsl),
                                        mappedPsl->tStart, mappedPsl->tEnd,
                                        mappedPsl->tEnd, 0,
                                        genePredAllFlds, mappedPsl->blockCount);
mappedGp->score = srcGp->score;
mappedGp->name2 = cloneString(srcGp->name2);
int iBlock;
for (iBlock = 0; iBlock < mappedPsl->blockCount; iBlock++)
    convertPslBlock(mappedPsl, iBlock, srcGp, srcQueryExons, mappedGp);
if (mappedGp->cdsStart >= mappedGp->cdsEnd)
    {
    // for to common representation of no CDS.
    mappedGp->cdsStart = mappedGp->cdsEnd = mappedGp->txEnd;
    mappedGp->cdsStartStat = mappedGp->cdsEndStat = cdsNone;
    }
return mappedGp;
}

static void convertPsl(struct hash* srcGenePredMap, struct psl *mappedPsl, FILE* genePredOutFh)
/* convert a single PSL */
{
if (pslTStrand(mappedPsl) == '-')
    pslRc(mappedPsl); // target must be `+'

struct genePred* srcGp = srcGenePredFind(srcGenePredMap, mappedPsl->qName);
if (genePredBases(srcGp) != mappedPsl->qSize)
    errAbort("srcGenePred %s exon size %d does not match mappedPsl %s qSize %d",
             srcGp->name, genePredBases(srcGp), mappedPsl->qName, mappedPsl->qSize);
struct srcQueryExon srcQueryExons[srcGp->exonCount];
srcQueryExonBuild(srcGp, srcQueryExons);

struct genePred *mappedGp = createGenePred(srcGp, srcQueryExons, mappedPsl);
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
char *srcGenePredFile = argv[1];
char *mappedPslFile = argv[2];
char *mappedGenePredFile = argv[3];
transMapPslToGenePred(srcGenePredFile, mappedPslFile, mappedGenePredFile);
return 0;
}

