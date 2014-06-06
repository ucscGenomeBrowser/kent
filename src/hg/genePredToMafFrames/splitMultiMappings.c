/* splitMultiMappings - split genes that mapped to multiple locations or have rearranged
 * exons into separate gene objects. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "splitMultiMappings.h"
#include "orgGenes.h"

/* maximum size allowed for an intron, large ones are assumed to be multiple
 * alignments and split */
#define MAX_INTRON_SIZE 2000000

static struct cdsExon *getGeneRestExon(struct cdsExon *exon,
                                       struct gene **geneRestRet)
/* get the corresponding exon in the geneRest; cloning gene if
 * it hasn't already been done. */
{
if (*geneRestRet == NULL)
    *geneRestRet = geneClone(exon->gene);
return geneGetExon(*geneRestRet, exon->exonNum);
}

static void moveExonFrames(struct exonFrames *prevEf, struct exonFrames *ef,
                           struct gene **geneRestRet)
/* move an exonFrames object from one gene to the rest gene */
{
struct cdsExon *exon = ef->exon;
struct cdsExon *exonRest = getGeneRestExon(exon, geneRestRet);
assert((prevEf == NULL) || (prevEf->next == ef));

/* unlink from gene */
if (prevEf == NULL)
    exon->frames = ef->next;
else
    prevEf->next = ef->next;
ef->next = NULL;
ef->exon->gene->numExonFrames--;

/* link to geneRest */
ef->exon = exonRest;
exonRest->gene->numExonFrames++;
slAddHead(&exonRest->frames, ef);
}

static void splitExonChromStrand(struct cdsExon *exon, struct exonFrames *ef0,
                                 struct gene **geneRestRet)
/* split exons with different chromosome and strand mappings than ef0 */
{
struct exonFrames *ef = exon->frames, *prevEf = NULL;
while (ef != NULL)
    {
    if ((ef->mf.strand[0] != ef0->mf.strand[0]) || !sameString(ef->mf.chrom, ef0->mf.chrom))
        {
        moveExonFrames(prevEf, ef, geneRestRet);
        ef = (prevEf == NULL) ? exon->frames : prevEf->next;
        }
    else
        {
        prevEf = ef;
        ef = ef->next;
        }
    }
}

static void splitChromStrand(struct gene *gene, struct gene **geneRestRet)
/* split genes with different chromsome and strand mappings  */
{
struct exonFrames *ef0 = geneFirstExonFrames(gene);
struct cdsExon *exon;
for (exon = gene->exons; exon != NULL; exon = exon->next)
    splitExonChromStrand(exon, ef0, geneRestRet);
}

static boolean exonFramesAfter(struct exonFrames *ef1, struct exonFrames *ef2)
/* return true if ef2 is after ef1 in target space in the direction of
 * transcription */
{
if (ef1->mf.strand[0] == '+')
    return ef2->mf.chromStart >= ef1->mf.chromEnd;
else
    return ef2->mf.chromEnd <= ef1->mf.chromStart;
}

static int exonFramesGapLen(struct exonFrames *ef1, struct exonFrames *ef2)
/* return length of gap between ef1 and ef2 in target space in the direction
 * of transcription */
{
if (ef1->mf.strand[0] == '+')
    return ef1->mf.chromEnd - ef2->mf.chromStart;
else
    return ef1->mf.chromStart - ef2->mf.chromEnd;
}

static boolean isNextConflicting(struct exonFrames *ef, struct exonFrames *nextEf)
/* determine if nextEf conflicts with preceeding exonFrame ef */
{
if (nextEf == NULL)
    return FALSE; /* no next */
if (nextEf->cdsStart < ef->cdsEnd)
    return TRUE;  /* overlap in gene space */
if (!exonFramesAfter(ef, nextEf))
    return TRUE; /* out of order or overlap in target space */
if (exonFramesGapLen(ef, nextEf) > MAX_INTRON_SIZE)
    return TRUE; /* absurdly large exon */
return FALSE;
}

static struct exonFrames *splitConflicting(struct exonFrames *ef, struct gene **geneRestRet)
/* split out conflicting exonFrames following ef, return next non-conflicting
 * exonFrames */
{
struct exonFrames *nextEf;
for (nextEf = exonFramesNext(ef); isNextConflicting(ef, nextEf); nextEf = exonFramesNext(ef))
    {
    /* pass null for prevEf if we switched to a new exon */
    moveExonFrames(((ef->next == NULL) ? NULL : ef), nextEf, geneRestRet);
    }
return nextEf;
}

static void splitMultAlign(struct gene *gene, struct gene **geneRestRet)
/* Split based on portions of a gene being aligned multiple times.  Opposite
 * strand alignments must have been processed first. */
{
struct exonFrames *ef;

/* algorithm requires sorting by offset in gene */
geneSortFramesOffTarget(gene);

/* Check each exonFrame to see if the next one overlaps in gene, goes
 * backwards in target space, or results in an absurdly large exon.  If so,
 * find the best continuation of this frame and move all intervening ones to
 * the new gene */
for (ef = geneFirstExonFrames(gene); ef != NULL;)
    {
    struct exonFrames *nextEf = exonFramesNext(ef);
    if (isNextConflicting(ef, nextEf))
        ef = splitConflicting(ef, geneRestRet);
    else
        ef = nextEf;
    }
}

static void splitGeneMultiMappings(struct gene *gene)
/* check if a gene is mapped to multiple locations or exons have been
 * reordered, and if so, make current gene consistent, and move the remaining
 * exonFrames to a new gene object, to be made consistent in the next pass */
{
struct gene *geneRest = NULL;

splitChromStrand(gene, &geneRest);
splitMultAlign(gene, &geneRest);
}

void splitMultiMappings(struct orgGenes *genes)
/* check if genes are mapped to multiple locations, and if so, split them
 * into two or more genes */
{
struct gene *gene, *doneGenes = NULL;

/* a gene is removed from the list and processed, if this results in a
 * inconsistent exonFrames being added to a new gene, it will be at the head
 * of the list list and processed next time through the loop*/
while ((gene = slPopHead(&genes->genes)) != NULL)
    {
    if (gene->numExonFrames > 0)
        splitGeneMultiMappings(gene);
    slAddHead(&doneGenes, gene);
    }
genes->genes = doneGenes;
}

