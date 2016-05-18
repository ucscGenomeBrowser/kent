
#ifndef BEDTABIX_H
#define BEDTABIX_H

#include "linefile.h"

struct bedTabixFile
{
struct lineFile *lf;
};

struct bedTabixFile *bedTabixFileMayOpen(char *fileOrUrl, char *chrom, int start, int end);
/* Open a bed file that has been compressed and indexed by tabix */

struct bed *bedTabixReadBeds(struct bedTabixFile *btf, char *chromName, int winStart, int winEnd, struct bed * (*loadBed)(), int minScore);

void bedTabixFileClose(struct bedTabixFile *btf);
#endif //BEDTABIX_H
