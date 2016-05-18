#include "bedTabix.h"

struct bedTabixFile *bedTabixFileMayOpen(char *fileOrUrl, char *chrom, int start, int end)
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

struct bed *bedTabixReadBeds(struct bedTabixFile *btf, char *chrom, int start, int end, struct bed * (*loadBed)(void *tg), int minScore)
{
struct bed *bedList = NULL;

int wordCount;
char *words[100];

lineFileSetTabixRegion(btf->lf, chrom, start, end);
while ((wordCount = lineFileChopTab(btf->lf, words)) > 0)
    {
    struct bed *bed = loadBed(words);
    slAddHead(&bedList, bed);
    }
return bedList;
}

void bedTabixFileClose(struct bedTabixFile *btf)
{
lineFileClose(&btf->lf);
}
