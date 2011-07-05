/* hapRegions - mapping between sequences containing haplotype regions and
 * reference chromosomes */
#ifndef HAPREGIONS_H
#define HAPREGIONS_H

struct cDnaQuery;

struct hapRegions *hapRegionsNew(char *hapPslFile, FILE *hapRefMappedFh, FILE *hapRefCDnaFh);
/* construct a new hapRegions object from PSL alignments of the haplotype
 * pseudo-chromosomes to the haplotype regions of the reference chromsomes. */

void hapRegionsFree(struct hapRegions **hrPtr);
/* free a hapRegions object */

boolean hapRegionsIsHapChrom(struct hapRegions *hr, char *chrom);
/* determine if a chromosome is a haplotype pseudo-chromosome */

void hapRegionsLinkHaps(struct hapRegions *hr, struct cDnaQuery *cdna);
/* Link a haplotype chrom alignments to reference chrom alignments if
 * they can be mapped. */

void hapRegionsBuildHapSets(struct cDnaQuery *cdna);
/* build links to all alignments that are not haplotypes linked to reference
 * forming haplotype sets.  These includes unlinked haplotypes.
 * this should also be called when there are no hapRegions alignments to build
 * default hapSets.
 */

#endif
