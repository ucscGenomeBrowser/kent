/*
 * Used to sort refAlign objects by chromosome.
 */
#ifndef HG_REF_ALIGN_CHROM_LIST_H
#define HG_REF_ALIGN_CHROM_LIST_H

/* per-chromosome list of refAlign objects */
struct refAlignChrom {
    struct refAlignChrom* next;
    struct slName* slName;   /* contains the chromosome name memory */
    char* chrom;
    struct refAlign* refAlignList;
};

struct refAlignChrom* refAlignChromListBuild(struct refAlign* refAlignList);
/* split refAlign objects by chromosome, ownership of objects is passed
 * to this list. */

void refAlignChromListFree(struct refAlignChrom* chromList);
/* free a refAlignChrom, frees the refAlign objects  */

#endif
