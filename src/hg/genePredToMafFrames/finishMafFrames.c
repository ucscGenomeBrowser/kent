/* finishMafFrames - link mafFrames objects to deal with spliced codons */
#include "common.h"
#include "finishMafFrames.h"
#include "frameIncr.h"
#include "geneBins.h"
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
if (isSplitCodon(prevEf, ef))
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
}

static void linkExonFrames(struct cdsExon *prevExon, struct cdsExon *exon)
/* link exonFrames for an exon.  Assumes this is called in transcription
 * order, so preceeding exon frames for this gene have been linked. */
{
struct exonFrames *prevEf = (prevExon == NULL) ? NULL
    : slLastEl(prevExon->frames);
struct exonFrames *ef;
for (ef = exon->frames; ef != NULL; ef = ef->next)
    {
    if (prevEf != NULL)
        linkFrames(prevEf, ef);
    prevEf = ef;
    }
}

static void finishGene(struct gene *gene)
/* finish mafFrames for one gene. */
{
struct cdsExon *prevExon = NULL, *exon;
geneSortFramesTargetOff(gene);

for (exon = gene->exons; exon != NULL; exon = exon->next)
    {
    linkExonFrames(prevExon, exon);
    prevExon = exon;
    }
}

void finishMafFrames(struct geneBins *genes)
/* Finish mafFrames build, linking mafFrames prev/next field for each gene */
{
struct gene *gene;
for (gene = genes->genes; gene != NULL; gene = gene->next)
    finishGene(gene);
}
