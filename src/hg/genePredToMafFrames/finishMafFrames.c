/* finishMafFrames - link mafFrames objects to deal with spliced codons */
#include "common.h"
#include "finishMafFrames.h"
#include "frameIncr.h"
#include "geneBins.h"
#include "chromBins.h"
#include "binRange.h"
#include "localmem.h"

static int exonFramesCmp(const void *va, const void *vb)
/* compare by target transcription order */
{
struct exonFrames *a = *((struct exonFrames **)va);
struct exonFrames *b = *((struct exonFrames **)vb);
if (a->mf.strand[0] == '+')
    return a->mf.chromStart - b->mf.chromStart;
else
    return b->mf.chromStart - a->mf.chromStart;
}

static void sortExonFrames(struct gene *gene)
/* sort exon frames of a gene into target transcription order  */
{
struct cdsExon *exon;
for (exon = gene->exons; exon != NULL; exon = exon->next)
    slSort(&exon->frames, exonFramesCmp);
}

static struct exonFrames *findFirstExonFrames(struct gene *gene)
/* find the first exons frames object, or NULL if none are found */
{
struct cdsExon *exon;
for (exon = gene->exons; exon != NULL; exon = exon->next)
    {
    if (exon->frames != NULL)
        return exon->frames;
    }
return NULL;
}

static void checkExonMultiMapping(struct cdsExon *exon,
                                  char *chrom, char strand,
                                  boolean *warnedPtr)
/* check an exon for multiple mappings.  Warn on first promblem encountered.
 * Drop those that conflict with first mapping. */
{
struct exonFrames *ef, *keptEf = NULL;
while ((ef = slPopHead(&exon->frames)) != NULL)
    {
    if (!(sameString(ef->mf.chrom, chrom) && (ef->mf.strand[0] == strand)))
        {
        if (!*warnedPtr)
            {
            fprintf(stderr, "Warning: %s maps to multiple target locations\n",
                    exon->gene->name);
            *warnedPtr = TRUE;
            }
        }
    else
        slAddHead(&keptEf, ef);
    }
slReverse(&keptEf);
exon->frames = keptEf;
}

static void checkMultiMapping(struct gene *gene)
/* check for a gene have multiple mappings, issue warning if it does and keep
 * only one of the mappings.  FIXME: this should latter be changed keep only
 * one. FIXME: should also check for overlap. */
{
struct exonFrames *firstEf = findFirstExonFrames(gene);
if (firstEf != NULL)
    {
    struct cdsExon *exon;
    boolean warned = FALSE;
    for (exon = gene->exons; exon != NULL; exon = exon->next)
        checkExonMultiMapping(exon, firstEf->mf.chrom, firstEf->mf.strand[0],
                              &warned);
    }
}

static boolean isSplitCodon(struct exonFrames *ef0, struct exonFrames *ef1) 
/* Determine if the last codon of ef0 is split in the target sequence and is
 * continued in ef1. If codon is not split, or only partially aligned, false
 * is returned.  ef0 preceeds ef1 in the direction of transcription */
{
int tLen0 = ef0->mf.chromEnd - ef0->mf.chromStart;
if (ef1->mf.strand[0] == '+')
    {
    if (ef1->queryStart != ef0->queryEnd)
        return FALSE; /* not contiguous in query */
    if (ef1->mf.chromStart == ef0->mf.chromEnd)
        return FALSE; /* contiguous in target */
    }
else
    {
    if (ef1->queryEnd != ef0->queryStart)
        return FALSE; /* not contiguous in query */
    if (ef1->mf.chromEnd == ef0->mf.chromStart)
        return FALSE; /* contiguous in target */
    }
if (frameIncr(ef0->mf.frame, tLen0) == 0)
    return FALSE; /* codon not split */
if (ef1->mf.frame != frameIncr(ef0->mf.frame, tLen0))
    errAbort("isSplitCodon: frame should be contiguous: gene %s %d exons",
             ef0->exon->gene->name, slCount(ef0->exon->gene->exons));
return TRUE;
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
        }
    else
        {
        prevEf->mf.prevFramePos = ef->mf.chromEnd-1;
        ef->mf.nextFramePos = prevEf->mf.chromStart;
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
checkMultiMapping(gene);
sortExonFrames(gene);

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
