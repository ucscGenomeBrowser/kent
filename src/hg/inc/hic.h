/* hic.h contains a few helpful wrapper functions for managing Hi-C data. */

#ifndef HIC_H
#define HIC_H

#include "interact.h"

/* Metadata associated with a Hi-C track */
struct hicMeta
{
char *filename;
char *fileAssembly;
char **chromNames;
int nChroms;
char **resolutions;
int nRes;
char *ucscAssembly;
struct hash *ucscToAlias; // Takes UCSC chrom names to names the underlying file recognizes
};

char *hicLoadHeader(char *filename, struct hicMeta **header, char *ucscAssembly);
/* Create a hicMeta structure for the supplied Hi-C file.  If
 * the return value is non-NULL, it points to a string containing
 * an error message that explains why the retrieval failed. */

char *hicLoadData(struct hicMeta *fileInfo, int resolution, char *normalization, char *chrom1,
    int start1, int end1, char *chrom2, int start2, int end2, struct interact **resultPtr);
/* Fetch heatmap data from a hic file.  The hic file info must be provided in fileInfo, which should be
 * populated by hicLoadHeader.  The result is a linked list of interact structures in *resultPtr,
 * and the return value (if non-NULL) is the text of any error message encountered by the underlying
 * Straw library. */

#endif /* HIC_H */
