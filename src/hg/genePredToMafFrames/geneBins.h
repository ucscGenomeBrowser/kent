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
    struct gene *genes;      /* linked list of all genes */
};

struct gene
/* A gene, one per source gene, however it maybe mapped to multiple
 * target locations*/
{
    struct gene *next;
    char *name;              /* gene name */
    struct cdsExon *exons;   /* list of CDS exons, in transcription order */
    int numExonFrames;       /* count of associated exonFrames objects */
    struct geneBins *genes;  /* link back to geneBins object */
};

struct cdsExon
/* one CDS exon */
{
    struct cdsExon* next; /* link for gene's exons */
    struct gene *gene;    /* gene object */
    char *chrom;          /* source chromosome range */
    int chromStart;
    int chromEnd;
    char strand;
    char frame;                 /* frame number */
    int exonNum;               /* exon number, in transcription order */
    int cdsOff;                 /* location within CDS (after splicing) */
    struct exonFrames *frames;  /* frames associated with the exon */
};

struct exonFrames
/* object the hold frame information for part of an exon.  A new record
 * is created if there is any discontinuity in the alignment */
{
    struct exonFrames *next;
    struct cdsExon *exon;     /* associated exon */
    int srcStart;             /* range in src organism MAF coordinates */
    int srcEnd;
    int cdsStart;             /* location within CDS (after splicing)
                               * in direction of transcription */
    int cdsEnd;
    struct mafFrames mf;      /* MAF frames object being created, this is in
                               * the target coordinates */
};

struct geneBins *geneBinsNew(char *genePredFile);
/* construct a new geneBins object from the specified file */

void geneBinsFree(struct geneBins **genesPtr);
/* free geneBins object */

struct binElement *geneBinsFind(struct geneBins *genes, struct mafComp *comp);
/* Return list of references to exons overlapping the specified component,
 * sorted into the assending order of the component. slFreeList returned list. */

struct gene* geneClone(struct gene *gene);
/* clone a gene and it's exons.  Does not clone exonFrames.  Used when
 * spliting a gene mapped to two locations */

struct cdsExon *geneGetExon(struct gene *gene, int exonNum);
/* get an exon from a gene by it's exon number, or an error */

void geneSortFramesTargetOff(struct gene *gene);
/* sort the exonFrames in each exon into target transcription order then gene
 * offset. */

void geneSortFramesOffTarget(struct gene *gene);
/* sort the exonFrames in each exon into gene offset then target transcription
 * order */

struct exonFrames *cdsExonAddFrames(struct cdsExon *exon,
                                    char *src, int qStart, int qEnd,
                                    char *tName, int tStart, int tEnd,
                                    char frame, char strand, int cdsOff);
/* allocate a new mafFrames object and link it exon */

struct exonFrames *geneFirstExonFrames(struct gene *gene);
/* find the first exons frames object, or error if not found */

struct exonFrames *exonFramesNext(struct exonFrames *ef);
/* get the next exonFrames object, moving on to the next exon with exonFrames
 * after the current one */

void exonFramesDump(FILE *fh, struct exonFrames *ef);
/* dump contents of an exonFrame for debugging purposes */

void cdsExonDump(FILE *fh, struct cdsExon *exon);
/* dump contents of a cdsExon for debugging purposes */

void geneDump(FILE *fh, struct gene *gene);
/* dump contents of a gene for debugging purposes */

#endif
