/* hapRegions - mapping between sequences containing haplotype regions and
 * reference chromosomes */
#ifndef HAPREGIONS_H
#define HAPREGIONS_H

struct cDnaQuery;

struct hapRegions *hapRegionsNew(char *hapPslFile);
/* construct a new hapRegions object from PSL alignments of the haplotype
 * pseudo-chromosomes to the haplotype regions of the reference chromsomes. */

void hapRegionsFree(struct hapRegions **hrPtr);
/* free a hapRegions object */

boolean hapRegionsIsHapChrom(struct hapRegions *hr, char *chrom);
/* determine if a chromosome is a haplotype pseudo-chromosome */

boolean hapRegionsInHapRegion(struct hapRegions *hr, char *chrom, int start, int end);
/* determine if chrom range is in a haplotype region of a reference chromosome */

void hapRegionsLink(struct hapRegions *hr, struct cDnaQuery *cdna,
                    FILE *hapRefMappedFh, FILE *hapRefCDnaFh);
/* Link a haplotype chrom alignments to reference chrom alignments if
 * they can be mapped. */

#endif
