/* geneBins - objects used to hold gene related data */
#ifndef GENEBINS
#define GENEBINS
struct geneBins;
struct mafComp;

struct cdsExon
/* one CDS exon */
{
    char *gene;          /* gene name */
    char *chrom;         /* chromosome range */
    int chromStart;
    int chromEnd;
    char strand;
    char frame;          /* frame number */
    int  iExon;          /* exon index in genePred */
};

struct geneBins *geneBinsNew(char *genePredFile);
/* construct a new geneBins object from the specified file */

void geneBinsFree(struct geneBins **genesPtr);
/* free geneBins object */

struct binElement *geneBinsFind(struct geneBins *genes, struct mafComp *comp);
/* Return list of references to exons overlapping the specified component,
 * sorted into the assending order of the component. slFree returned list. */

#endif
