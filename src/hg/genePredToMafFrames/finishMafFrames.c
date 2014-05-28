/* finishMafFrames - link mafFrames objects to deal with spliced codons */

/* Copyright (C) 2008 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "finishMafFrames.h"
#include "frameIncr.h"
#include "orgGenes.h"
#include "chromBins.h"
#include "binRange.h"
#include "localmem.h"

static boolean isSplitCodon(struct exonFrames *ef0, struct exonFrames *ef1) 
/* Determine if the last codon of ef0 is split across maf blocks and is
 * continued in ef1. If codon is not split, or only partially aligned, false
 * is returned.  ef0 preceeds ef1 in the direction of transcription */
{
int tLen0 = ef0->mf.chromEnd - ef0->mf.chromStart;
if (ef1->cdsStart != ef0->cdsEnd)
    return FALSE; /* deletion in gene between two records */
if (frameIncr(ef0->mf.frame, tLen0) == 0)
    return FALSE; /* codon not split */
if (ef1->mf.frame != frameIncr(ef0->mf.frame, tLen0))
    return FALSE;  /* frame not contiguous */
return TRUE;
}

static void outOfOrderLink(struct exonFrames *ef)
/* generate error about linking exons that are out of transcription
 * order.  This indicates a failure to separate multiply aligned genes */
{
errAbort("gene %s mapped to %s (%s) has frames linking out of transcription order",
         ef->exon->gene->name, ef->mf.chrom, ef->mf.strand);
}

static void linkFrames(struct exonFrames *prevEf, struct exonFrames *ef)
/* link the previous exonFrames object to this object.  assumes prevEf
 * is not NULL and called in transcription order */
{
if (ef->mf.strand[0] == '+')
    {
    prevEf->mf.nextFramePos = ef->mf.chromStart;
    ef->mf.prevFramePos = prevEf->mf.chromEnd-1;
    if ((prevEf->mf.nextFramePos < prevEf->mf.chromEnd)
        || (ef->mf.prevFramePos > ef->mf.chromStart))
        outOfOrderLink(ef);
    }
else
    {
    prevEf->mf.nextFramePos = ef->mf.chromEnd-1;
    ef->mf.prevFramePos = prevEf->mf.chromStart;
    if ((prevEf->mf.nextFramePos > prevEf->mf.chromStart)
        || (ef->mf.prevFramePos < ef->mf.chromEnd))
        outOfOrderLink(ef);
    }
}

static boolean isAdjacentFrames(struct exonFrames *ef0, struct exonFrames *ef1) 
/* check if exonFrames objects are adjacent and should be joined. ef0 preceeds
 * ef1 in the direction of transcription.  Must be i */
{
if (ef0->mf.strand[0] == '+')
    return (ef1->mf.chromStart == ef0->mf.chromEnd)
        && (ef1->mf.frame == frameIncr(ef0->mf.frame, (ef0->mf.chromEnd-ef0->mf.chromStart)));
else
    return (ef1->mf.chromEnd == ef0->mf.chromStart)
        && (ef1->mf.frame == frameIncr(ef0->mf.frame, (ef0->mf.chromEnd-ef0->mf.chromStart)));
}

static void joinFrames(struct exonFrames *ef0, struct exonFrames *ef1) 
/* join if exonFrames objects that are adjacent, keeping ef0 */
{
if (ef0->mf.strand[0] == '+')
    {
    ef0->srcEnd = ef1->srcEnd;
    ef0->mf.chromEnd = ef1->mf.chromEnd;
    }
else
    {
    ef0->srcStart = ef1->srcStart;
    ef0->mf.chromStart = ef1->mf.chromStart;
    }
ef0->cdsEnd = ef1->cdsEnd;
ef0->mf.nextFramePos = ef1->mf.nextFramePos;

assert(ef0->srcStart < ef0->srcEnd);
assert(ef0->cdsStart < ef0->cdsEnd);
assert(ef0->mf.chromStart < ef0->mf.chromEnd);
}

static void joinExonFrames(struct cdsExon *exon) 
/* join adjacent exonFrames objects for an exon, removing ones that were
 * split due to maf block splits.  */
{
/* rebuild frames list, seed with first */
struct exonFrames *frames = slPopHead(&exon->frames);
struct exonFrames *prevEf = frames, *ef;

while ((ef = slPopHead(&exon->frames)) != NULL)
    {
    if (isAdjacentFrames(prevEf, ef))
        joinFrames(prevEf, ef);
    else
        {
        slAddHead(&frames, ef);
        prevEf = ef;
        }
    }
slReverse(&frames);
exon->frames = frames;
}

 static struct exonFrames *linkExonFrames(struct exonFrames *prevEf, struct cdsExon *exon)
/* link split frames for an exon, if needed.  Assumes this is called in
 * transcription order, so preceeding exon frames for this gene have been
 * linked. Return new prevEf. */
{
struct exonFrames *ef;
       
for (ef = exon->frames; ef != NULL; ef = ef->next)
    {
    if ((prevEf != NULL) && isSplitCodon(prevEf, ef))
        linkFrames(prevEf, ef);
    prevEf = ef;
    }
return prevEf;
}

static void setExonBounds(struct cdsExon *exon)
/* set isExonStart/isExonEnd flags on frames associated with an exon */
{
if (exon->frames != NULL)
    {
    exon->frames->mf.isExonStart = TRUE;
    struct exonFrames *ef = slLastEl(exon->frames);
    ef->mf.isExonEnd = TRUE;
    }
}

static void finishGene(struct gene *gene)
/* finish mafFrames for one gene. */
{
struct cdsExon *exon;
struct exonFrames *prevEf = NULL;
geneSortFramesTargetOff(gene);

for (exon = gene->exons; exon != NULL; exon = exon->next)
    joinExonFrames(exon);
for (exon = gene->exons; exon != NULL; exon = exon->next)
    {
    prevEf = linkExonFrames(prevEf, exon);
    setExonBounds(exon);
    }
}

void finishMafFrames(struct orgGenes *genes)
/* Finish mafFrames build, linking mafFrames prev/next field for each gene */
{
struct gene *gene;
for (gene = genes->genes; gene != NULL; gene = gene->next)
    finishGene(gene);
}
