
/* Routines for reading bedTabix formatted files. bedTabix files are bed files indexed by tabix.  Indexing
 * works like vcfTabix does.
 *
 * This file is copyright 2016 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */
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
