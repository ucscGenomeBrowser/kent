
#ifndef BEDTABIX_H
#define BEDTABIX_H

#include "linefile.h"

struct bedTabixFile
{
struct lineFile *lf;
};

struct asObject *longTabixAsObj();

struct bedTabixFile *bedTabixFileMayOpen(char *fileOrUrl, char *chrom, int start, int end);
/* Open a bed file that has been compressed and indexed by tabix */

struct bedTabixFile *bedTabixFileOpen(char *fileOrUrl, char *chrom, int start, int end);
/* Attempt to open bedTabix file. errAbort on failure. */

struct bed *bedTabixReadFirstBed(struct bedTabixFile *btf, char *chrom, int start, int end, struct bed * (*loadBed)());
/* Read in first bed in range (for next item).*/

struct bed *bedTabixReadBeds(struct bedTabixFile *btf, char *chromName, int winStart, int winEnd, struct bed * (*loadBed)());
/* Read in all beds in range.*/

void bedTabixFileClose(struct bedTabixFile **btf);
#endif //BEDTABIX_H
