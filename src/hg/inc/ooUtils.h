/* ooUtils - utilities to help manage building o+o directories and
 * the like. */
#ifndef OOUTILS_H
#define OOUTILS_H

void ooToAllContigs(char *ooDir, void (*doContig)(char *dir, char *chrom, char *contig));
/* Apply "doContig" to all contig subdirectories of ooDir. */

#endif /* OOUTILS_H */

