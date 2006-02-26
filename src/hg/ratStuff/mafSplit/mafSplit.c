/* Split MAF file based on positions in a bed file */

#include "common.h"
#include "portable.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "maf.h"
#include "bed.h"

static char const rcsid[] = "$Id: mafSplit.c,v 1.1 2006/02/26 16:29:03 kate Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafSplit - Split multiple alignment files\n"
  "usage:\n"
  "   mafSplit splits.bed outRoot file(s).maf\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct hash *loadSplits(char *file)
/* load splits into a hash of lists by chrom */
{
struct bed *bed = NULL, *bedList = NULL, *nextBed = NULL;
struct hash *splitHash = newHash(6);
struct bed *splits = bedLoadNAll(file, 3);

verbose(2, "found %d splits\n", slCount(splits));
for (bed = splits; bed != NULL; bed = nextBed)
    {
    verbose(3, "split %s:%d\n", bed->chrom, bed->chromStart);
    nextBed = bed->next;
    if ((bedList = hashFindVal(splitHash, bed->chrom)) == NULL)
        {
        verbose(3, "adding %s to hash\n", bed->chrom);
        hashAdd(splitHash, bed->chrom, cloneBed(bed));
        }
    else
        slAddTail(&bedList, cloneBed(bed));
    freeMem(bed);
    }
return splitHash;
}

char *chromFromSrc(char *src)
/* get chrom name from <db>.<chrom> */
{
char *p;
if ((p = strchr(src, '.')) == NULL)
    errAbort("Can't find chrom in MAF component src: %s\n", src);
return ++p;
}

void splitMafFile(char *file, char *outDir, char *outPrefix, 
                        struct hash *splitHash)
/* split file based on positions in hash */
{
char *chrom = NULL;
char outFile[PATH_LEN];
int ix = 0;
FILE *f;
boolean nextFile = TRUE;
struct bed *bed, *bedList = NULL;
int splitPos = 0;
struct mafFile *mf = mafOpen(file);
struct mafAli *maf = NULL;
struct mafComp *mc;

verbose(1, "splitting %s\n", file);
maf = mafNext(mf);
while (1)
    {
    if (!maf)
        {
        maf = mafNext(mf);
        if (!maf)
            break;
        }
    mc = maf->components;
    if (!chrom || differentString(chrom, chromFromSrc(mc->src)))
        /* new chrom */
        {
        char *p;
        chrom = cloneString(chromFromSrc(mc->src));
        bedList = (struct bed *)hashFindVal(splitHash, chrom);
        if (bedList)
            bed = (struct bed *)slPopHead(&bedList);
        else
            /* no entry for this chrom -- use the whole thing */
            bed = NULL;
        if (bed)
            splitPos = bed->chromStart;
        else
            /* last piece of this chrom */
            splitPos = mc->srcSize;
        ix = 0;
        nextFile = TRUE;
        }
    if (nextFile)
        {
        verbose(2, "file %d, pos: %d\n", ix, splitPos);
        safef(outFile, sizeof(outFile), "%s/%s%s.%d.maf", 
                    outDir, outPrefix, chrom, ix);
        f = mustOpen(outFile, "w");
        mafWriteStart(f, mf->scoring);
        nextFile = FALSE;
        }
    if (mc->start + mc->size < splitPos)
        {
        mafWrite(f, maf);
        mafAliFree(&maf);
        continue;
        }
    if (mc->start < splitPos)
        {
        struct mafAli *full = maf;
        struct mafAli *subset = mafSubset(full, mc->src, mc->start, splitPos);
        mafWrite(f, subset);
        mafAliFree(&subset);
        maf = mafSubset(full, mc->src, splitPos + 1, mc->start + mc->size);
        mafAliFree(&full);
        }
    mafWriteEnd(f);
    carefulClose(&f);
    ix++;
    nextFile = TRUE;
    bed = (struct bed *)slPopHead(&bedList);
    if (bed)
        splitPos = bed->chromStart;
    else
        splitPos = mc->srcSize;
    }
mafFileFree(&mf);
}

void mafSplit(char *splitFile, char *outRoot, int mafCount, char *mafFiles[])
/* Split MAF files at breaks in split file */
{
struct hashEl *hel;
char dir[256], file[128];
int i = 0;
struct hash *bedHash = NULL;

verbose(1, "Splitting %d files from %s to %s\n", mafCount, splitFile, outRoot);
splitPath(outRoot, dir, file, NULL);
if (dir[0])
    makeDir(dir);
bedHash = loadSplits(splitFile);
for (i = 0; i < mafCount; i++)
    splitMafFile(mafFiles[i], dir, file, bedHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 4)
    usage();
mafSplit(argv[1], argv[2], argc-3, &argv[3]);
return 0;
}
