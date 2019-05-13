/* hic.h contains a few helpful wrapper functions for managing Hi-C data. */

#ifndef HIC_H
#define HIC_H

/* Metadata associated with a Hi-C track */
struct hicMeta
{
char *assembly;
char **chromNames;
int nChroms;
char **resolutions;
int nRes;
struct hash *ucscToAlias; // Takes UCSC chrom names to names the underlying file recognizes
};

char *hicLoadHeader(char *filename, struct hicMeta **header);
/* Create a hicMeta structure for the supplied Hi-C file.  If
 * the return value is non-NULL, it points to a string containing
 * an error message that explains why the retrieval failed. */

#endif /* HIC_H */
