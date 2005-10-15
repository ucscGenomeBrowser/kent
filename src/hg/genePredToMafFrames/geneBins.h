/* geneBins - objects used to hold gene related data */
#ifndef GENEBINS
#define GENEBINS
#include "mafFrames.h"
struct geneBins;
struct mafComp;
struct mafFrames;

struct geneBins
/* binRange table of genes, by chromosome */
{
    struct chromBins *bins;  /* map of chrom and ranges to cds exons */
    struct lm *memPool;      /* memory for exons allocated from this pool */
    char *curChrom;          /* cache of current string for allocating pool memory */
    char *curGene;
};

struct exonFrames
/* object the hold frame information for part of an exon.  A new record
 * is created if there is any discontinuity in the alignment */
{
    struct exonFrames *next;
    int queryStart;           /* range in query coordinates */
    int queryEnd;
    struct mafFrames mf;      /* MAF frames object being created, this is in
                               * the target coordinates */
};

struct cdsExon
/* one CDS exon */
{
    struct cdsExon* prev; /* links to previous and next exons in gene */
    struct cdsExon* next;
    char *gene;           /* gene name */
    char *chrom;          /* chromosome range */
    int chromStart;
    int chromEnd;
    char strand;
    char frame;           /* frame number */
    int  iExon;           /* exon index in genePred */
    struct exonFrames *frames;  /* frames associated with the exon */
    struct lm *memPool;  /* shared memory pool for exons */
};

struct geneBins *geneBinsNew(char *genePredFile);
/* construct a new geneBins object from the specified file */

void geneBinsFree(struct geneBins **genesPtr);
/* free geneBins object */

struct binElement *geneBinsFind(struct geneBins *genes, struct mafComp *comp);
/* Return list of references to exons overlapping the specified component,
 * sorted into the assending order of the component. slFreeList returned list. */

struct binElement *geneBinsChromExons(struct geneBins *genes, char *chrom);
/* Return list of references to all exons on a chromosome, sorted in
 * assending target order. slFreeList returned list. */

#endif
