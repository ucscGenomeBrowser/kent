/* Split MAF file based on positions in a bed file */

#include "common.h"
#include "portable.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "maf.h"
#include "bed.h"

static char const rcsid[] = "$Id: mafBedSubset.c,v 1.1 2008/07/08 22:08:35 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafBedSubset - Extract MAFS in a genomic region\n"
  "usage:\n"
  "    mafBedSubset regions.bed out.maf|outDir in.maf(s)\n"
  "options:\n"
  "    -outDir - output separate files named by bed name field to outDir\n"
  "    -ignoreDups - ignore duplicated bed regions in bed file\n"
  "warnings:\n"
  "    This program requires that none of the beds overlap and that\n"
  "    the reference sequence in the maf is in chrom order with no overlaps\n"
  );
}

static struct optionSpec options[] = {
    {"outDir", OPTION_BOOLEAN},
    {"ignoreDups", OPTION_BOOLEAN},
    {NULL, 0},
};

/* a structure to keep the lists of bed's for each chromosome */
struct bedHead
{
    struct bed *head;
};

boolean outDir = FALSE;
char *dir = NULL;
char *scoring = NULL;
boolean ignoreDups = FALSE;

void checkBeds(struct bed *bed)
{
char *chrom = NULL;
struct bed *prev = NULL;

for(; bed; prev = bed, bed = bed->next)
    {
    if ((chrom == NULL) || !sameString(chrom, bed->chrom))
	chrom = bed->chrom;
    else
	{
	if ((bed->chromStart == prev->chromStart) && (bed->chromEnd == prev->chromEnd))
	    {
	    if (ignoreDups)
		{
		prev->next = bed->next;
		bed = prev;
		}
	    else
		errAbort("dup'ed bed starting at %d end at %d on chrom %s with name %s",
		    bed->chromStart, bed->chromEnd, bed->chrom, bed->name);
	    }
	else if (bed->chromStart < prev->chromEnd)
	    {
	    errAbort("bed starting at %d overlaps previous end at %d on chrom %s",
	    	bed->chromStart, prev->chromEnd, chrom);
	    }
	}
    }
}

struct hash *loadRegions(char *file)
/* load regions into a hash of lists by chrom */
{
struct bed *bed = NULL,  *nextBed = NULL;
struct hash *regionHash = newHash(6);
struct bed *regions;
char *chrom = NULL;
struct bedHead *bHead = NULL;

regions = bedLoadNAll(file, outDir ? 4 : 3);
/* order by chrom, start */
slSort(&regions, bedCmp);
verbose(2, "found %d regions\n", slCount(regions));

/* make sure the bed regions are in the right order */
//slSort(regions, bedCmp);
checkBeds(regions);

for (bed = regions; bed != NULL; bed = nextBed)
    {
    verbose(3, "region %s:%d-%d\n", bed->chrom, bed->chromStart+1, bed->chromEnd);
    nextBed = bed->next;
    if ((chrom == NULL) || (!sameString(chrom, bed->chrom)))
	{
	if (bHead)
	    slReverse(&bHead->head);

	if ((bHead = hashFindVal(regionHash, bed->chrom)) != NULL)
	    errAbort("bed file not sorted correctly");
	
	AllocVar(bHead);
	verbose(3, "adding %s to hash\n", bed->chrom);
	hashAdd(regionHash, bed->chrom, bHead);
	chrom = bed->chrom;
        }

    slAddHead(&bHead->head, bed);
    }

if (bHead)
    slReverse(&bHead->head);

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
if (f && outDir)
    {
    mafWriteEnd(f);
    carefulClose(&f);
    }
}

void extractMafs(char *file, FILE *f, struct hash *regionHash)
/* extract MAFs in a file from regions specified in hash */
{
char *chrom = NULL, *thisChrom;
struct bedHead *bHead;
struct bed *bed = NULL;
struct mafFile *mf = mafOpen(file);
struct mafAli *maf = NULL;
char path[256];
char *outName = NULL;

verbose(1, "extracting from %s\n", file);
for(maf = mafNext(mf); maf; mafAliFree(&maf), maf = mafNext(mf) )
    {
    struct mafAli *submaf;
    struct mafComp *mc = maf->components;

    thisChrom = chromFromSrc(mc->src);
    if ((chrom == NULL) || (!sameString(chrom, thisChrom)))
	{
	bed = NULL;
	if ((bHead = hashFindVal(regionHash, thisChrom)) != NULL)
	    bed = bHead->head;
	chrom = thisChrom;
	}

    for (; (bed != NULL) && (bed->chromEnd <= mc->start);  bed = bed->next)
	;

    for(; (bed != NULL) && (bed->chromStart < mc->start + mc->size);  bed = bed->next)
	{
	submaf = mafSubsetE(maf, mc->src, bed->chromStart, bed->chromEnd, TRUE);

	if (outDir && ((outName == NULL) ||  !sameString(bed->name, outName)))
	    {
            endOutFile(f);
	    safef(path, sizeof (path), "%s/%s.maf", dir, bed->name);
	    outName = bed->name;
	    f = startOutFile(path);
	    }

        mafWrite(f, submaf);
	mafAliFree(&submaf);

	if (bed->chromEnd > mc->start + mc->size)
	    break;
	}
    }

mafFileFree(&mf);
}

void mafBedSubset(char *regionFile, char *out, int mafCount, char *mafFiles[])
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
ignoreDups = optionExists("ignoreDups");
if (argc < 4)
    usage();
mafBedSubset(argv[1], argv[2], argc-3, &argv[3]);
return 0;
}
