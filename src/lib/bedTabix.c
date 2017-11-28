/* bedTabix.c: Routines for reading bedTabix formatted files. bedTabix files are bed files indexed by tabix.  Indexing
 * works like vcfTabix does.
 *
 * This file is copyright 2016 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "bedTabix.h"

struct bedTabixFile *bedTabixFileMayOpen(char *fileOrUrl, char *chrom, int start, int end)
/* Open a bed file that has been compressed and indexed by tabix */
{
struct lineFile *lf = lineFileTabixMayOpen(fileOrUrl, TRUE);
if (lf == NULL)
    return NULL;

struct bedTabixFile *btf;
AllocVar(btf);
btf->lf = lf;
if (isNotEmpty(chrom) && start != end)
    {
    lineFileSetTabixRegion(lf, chrom, start, end);
    }

return btf;
}

struct bedTabixFile *bedTabixFileOpen(char *fileOrUrl, char *chrom, int start, int end)
/* Attempt to open bedTabix file. errAbort on failure. */
{
struct bedTabixFile *btf = bedTabixFileMayOpen(fileOrUrl, chrom, start, end);

if (btf == NULL)
    errAbort("Cannot open bed tabix file %s\n", fileOrUrl);

return btf;
}

struct bed *bedTabixReadFirstBed(struct bedTabixFile *btf, char *chrom, int start, int end, struct bed * (*loadBed)(void *tg))
/* Read in first bed in range (for next item).*/
{
int wordCount;
char *words[100];

if (!lineFileSetTabixRegion(btf->lf, chrom, start, end))
    return NULL;
if ((wordCount = lineFileChopTab(btf->lf, words)) > 0)
    return loadBed(words);
return NULL;
}

struct bed *bedTabixReadBeds(struct bedTabixFile *btf, char *chrom, int start, int end, struct bed * (*loadBed)(void *tg))
/* Read in all beds in range.*/
{
struct bed *bedList = NULL;

int wordCount;
char *words[100];

if (!lineFileSetTabixRegion(btf->lf, chrom, start, end))
    return NULL;
while ((wordCount = lineFileChopTab(btf->lf, words)) > 0)
    {
    struct bed *bed = loadBed(words);
    slAddHead(&bedList, bed);
    }
return bedList;
}

void bedTabixFileClose(struct bedTabixFile **pBtf)
{
lineFileClose(&((*pBtf)->lf));
*pBtf = NULL;
}
