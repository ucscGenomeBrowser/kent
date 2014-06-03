/* Split MAF file based on positions in a bed file */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "portable.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "maf.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafsInRegion - Extract MAFS in a genomic region\n"
  "usage:\n"
  "    mafsInRegion regions.bed out.maf|outDir in.maf(s)\n"
  "options:\n"
  "    -outDir - output separate files named by bed name field to outDir\n"
  "    -keepInitialGaps - keep alignment columns at the beginning and of a block that are gapped in all species\n"
  );
}

static struct optionSpec options[] = {
    {"outDir", OPTION_BOOLEAN},
    {"keepInitialGaps", OPTION_BOOLEAN},
    {NULL, 0},
};

boolean outDir = FALSE;
boolean keepInitialGaps = FALSE;
char *dir = NULL;
char *scoring = NULL;

struct hash *loadRegions(char *file)
/* load regions into a hash of lists by chrom */
{
struct bed *bed = NULL, *bedList = NULL, *nextBed = NULL, *temp = NULL;
struct hash *regionHash = newHash(6);
struct bed *regions;

regions = bedLoadNAll(file, outDir ? 4 : 3);
/* order by chrom, start */
slSort(&regions, bedCmp);
verbose(2, "found %d regions\n", slCount(regions));
bedList = regions;
for (bed = regions; bed != NULL; bed = nextBed)
    {
    verbose(3, "region %s:%d-%d\n", bed->chrom, bed->chromStart+1, bed->chromEnd);
    nextBed = bed->next;
    if ((bed->next == NULL) || (differentString(bed->chrom,bed->next->chrom)))
	{
	temp = bed->next;
	bed->next = NULL;
	hashAdd(regionHash, bed->chrom, bedList);
	verbose(2, "just added %d regions on %s\n", slCount(bedList), bed->chrom);
	bedList = temp;
	}
    }
return regionHash;
}

char *chromFromSrc(char *src)
/* get chrom name from <db>.<chrom> */
{
char *p;
if ((p = strchr(src, '.')) == NULL)
    errAbort("Can't find chrom in MAF component src: %s\n", src);
return ++p;
}

FILE *startOutFile(char *outFile)
{
static FILE *f = NULL;

f = mustOpen(outFile, "w");
verbose(3, "creating %s\n", outFile);
mafWriteStart(f, scoring);
return f;
}

void endOutFile(FILE *f)
{
mafWriteEnd(f);
carefulClose(&f);
}

void extractMafs(char *file, FILE *f, struct hash *regionHash)
/* extract MAFs in a file from regions specified in hash */
{
char *chrom = NULL;
struct bed *bed = NULL;
struct mafFile *mf = mafOpen(file);
struct mafAli *maf = NULL;
struct mafComp *mc;
char path[256];

verbose(1, "extracting from %s\n", file);
maf = mafNext(mf);
while (maf)
    {
    mc = maf->components;
    if (!chrom || differentString(chrom, chromFromSrc(mc->src)))
        chrom = cloneString(chromFromSrc(mc->src));         /* new chrom */
    bed = (struct bed *)hashFindVal(regionHash, chrom);
    if (!bed)
        {
        /* no regions on this chrom -- skip to next chrom */
        do
            mafAliFree(&maf);
        while (((maf = mafNext(mf)) != NULL) && sameString(chromFromSrc(maf->components->src), chrom));
        continue;  // start over with this maf
        }
    verbose(2, "region: %s:%d-%d\n", 
            bed->chrom, bed->chromStart+1, bed->chromEnd);
    if (outDir)
        {
        if (f)
            endOutFile(f);
        safef(path, sizeof (path), "%s/%s.maf", dir, bed->name);
        f = startOutFile(path);
        }

    /* skip mafs before region, stopping if chrom changes */
    while (maf && (mc = maf->components) && sameString(chrom, chromFromSrc(mc->src)) &&
        (mc->start + mc->size) <= bed->chromStart)
        {
        mafAliFree(&maf);
        maf = mafNext(mf);
        }

    /* extract all mafs and pieces of mafs in region */
    while (maf && (mc = maf->components) && sameString(chrom, chromFromSrc(mc->src)) &&
        (bed->chromStart < mc->start + mc->size && bed->chromEnd > mc->start))
        {
        int mafStart = mc->start;
        int mafEnd = mc->start + mc->size;
        struct mafAli *full = maf;
        if (mafStart < bed->chromStart || mafEnd > bed->chromEnd)
            {
            full = maf;
            maf = mafSubsetE(full, mc->src, bed->chromStart, bed->chromEnd, keepInitialGaps);
            mc = maf->components;
            }
        verbose(2, "   %s:%d-%d\n", chrom, mc->start+1, mc->start + mc->size);
        mafWrite(f, maf);
        struct mafAli *nextMaf = (mafEnd > bed->chromEnd+1)
            ? mafSubset(full, mc->src, bed->chromEnd+1, mafEnd) : mafNext(mf);
        if (maf != full)
            mafAliFree(&maf);
        mafAliFree(&full);
        maf = nextMaf;
        }
    /* get next region */
    hashRemove(regionHash, bed->chrom);
    if (bed->next)
        hashAdd(regionHash, bed->chrom, bed->next);
    }
mafFileFree(&mf);
}

void mafsInRegion(char *regionFile, char *out, int mafCount, char *mafFiles[])
/* Extract MAFs in regions listed in regin file */
{
int i = 0;
struct hash *bedHash = NULL;
FILE *f = NULL;
struct mafFile *mf = NULL;

verbose(1, "Extracting from %d files to %s\n", mafCount, out);
bedHash = loadRegions(regionFile);

/* get scoring scheme */
mf = mafOpen(mafFiles[0]);
if (!mf)
    errAbort("can't open MAF file: %s\n", mafFiles[0]);
scoring = cloneString(mf->scoring);
mafFileFree(&mf);

/* set up output dir */
if (outDir)
    {
    dir = out;
    makeDir(dir);
    }
else
    f = startOutFile(out);
for (i = 0; i < mafCount; i++)
    extractMafs(mafFiles[i], f, bedHash);
if (!outDir)
    endOutFile(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
outDir = optionExists("outDir");
keepInitialGaps = optionExists("keepInitialGaps");
if (argc < 4)
    usage();
mafsInRegion(argv[1], argv[2], argc-3, &argv[3]);
return 0;
}
