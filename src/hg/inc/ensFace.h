/* ensFace - stuff to manage interface to Ensembl web site. */

#ifndef ENSFACE_H
#define ENSFACE_H

char *ensOrgName(char *ucscOrgName);
/* Convert from ucsc to Ensembl organism name */

struct dyString *ensContigViewUrl(char *ensOrg, char *chrom, int chromSize,
	int winStart, int winEnd, char *archive);
/* Return a URL that will take you to ensembl's contig view on a chrom. */

char *ensOrgNameFromScientificName(char *scientificName);
/* Convert from ucsc to Ensembl organism name. */

#endif /* ENSFACE_H */
