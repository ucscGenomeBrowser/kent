/* Split MAF file based on positions in a bed file */

#include "common.h"
#include "portable.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "sqlNum.h"
#include "maf.h"
#include "bed.h"

static char const rcsid[] = "$Id: mafSplit.c,v 1.3 2006/04/28 19:20:30 angie Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafSplit - Split multiple alignment files\n"
  "usage:\n"
  "   mafSplit splits.bed outRoot file(s).maf\n"
  "options:\n"
  "   -byTarget       Make one file per target sequence.  (splits.bed input\n"
  "                   is ignored).\n"
  "   -outDirDepth=N  For use only with -byTarget.\n"
  "                   Create N levels of output directory under current dir.\n"
  "                   This helps prevent NFS problems with a large number of\n"
  "                   file in a directory.  Using -outDirDepth=3 would\n"
  "                   produce ./1/2/3/outRoot123.maf.\n"
  "   -useSequenceName  For use only with -byTarget.\n"
  "                     Instead of auto-incrementing an integer to determine\n"
  "                     output filename, expect each target sequence name to\n"
  "                     end with a unique number and use that number as the\n"
  "                     integer to tack onto outRoot.\n"
  );
}

static struct optionSpec options[] = {
   {"byTarget", OPTION_BOOLEAN},
   {"outDirDepth", OPTION_INT},
   {"useSequenceName", OPTION_BOOLEAN},
   {NULL, 0},
};

/* Option variables */
static boolean byTarget = FALSE;
static int outDirDepth = 0;
static boolean useSequenceName = FALSE;


static int numberFromName(char *name)
/* Given a name that ends in a number, pick off the number. */
{
char *s = name + strlen(name) - 1;
if (!isdigit(*s))
    errAbort("numberFromName: name \"%s\" does not end in a number.", name);
while (s > name && isdigit(*(s-1)))
    s--;
return atoi(s);
}

static char *mkOutPath(char *outRootDir, char *outRootFile, int seqNum)
/* formulate pathname, using seqNum if outDirDepth > 0 */
{
struct dyString *dy = dyStringNew(1024);
if (outDirDepth > 0)
    {
    int i = 0, pwrOfTen = 1;
    dyStringPrintf(dy, "%s/", outRootDir);
    for (i = 0;  i < outDirDepth-1;  i++)
	pwrOfTen *= 10;
    for (i = 0;  i < outDirDepth;  i++, pwrOfTen /= 10)
	{
	int seqNumDownshifted = seqNum / pwrOfTen;
	int seqNumMasked = seqNumDownshifted % 10;
	if (seqNumMasked < 0)
	    errAbort("mkOutPath got negative number: %d --> %d --> %d\n",
		     seqNum, seqNumDownshifted, seqNumMasked);
	dyStringPrintf(dy, "%d/", seqNumMasked);
	makeDir(dy->string);
	}
    dyStringPrintf(dy, "%s%05d.maf", outRootFile, seqNum);
    }
else
    {
    dyStringPrintf(dy, "%s/%s%03d.maf", outRootDir, outRootFile, seqNum);
    }
return dyStringCannibalize(&dy);
}

void splitMafByTarget(char *mafInName, char *outRootDir, char *outRootFile,
		      struct hash *pathHash, int *pSeqNum)
/* Split maf into one file per target sequence. */
{
static FILE *prevFile = NULL;
static char prevTarget[512];
struct mafFile *mf = mafOpen(mafInName);
struct mafAli *maf = NULL;

prevTarget[0] = 0;
while ((maf = mafNext(mf)) != NULL)
    {
    struct mafComp *mc = maf->components;
    char *targetName = mc->src;
    FILE *f = NULL;

    if (isNotEmpty(prevTarget) && sameString(targetName, prevTarget))
	{
	verbose(3, "Reusing open filehandle for target %s\n", prevTarget);
	f = prevFile;
	}
    else 
	{
	char *path = (char *)hashFindVal(pathHash, targetName);
	carefulClose(&prevFile);
	if (path != NULL)
	    {
	    verbose(3, "Appending path from hash lookup on %s: %s\n",
		    targetName, path);
	    f = mustOpen(path, "a");
	    }
	else
	    {
	    int seqNum = (*pSeqNum)++;
	    if (useSequenceName)
		seqNum = numberFromName(targetName);
	    path = mkOutPath(outRootDir, outRootFile, seqNum);
	    verbose(3, "Opening path %s for writing and adding it to hash "
		    "for %s\n", path, targetName);
	    f = mustOpen(path, "w");
	    fprintf(f, "##maf version=1 scoring=blastz\n");
	    hashAdd(pathHash, targetName, path);
	    }
	prevFile = f;
	}
    mafWrite(f, maf);
    mafAliFree(&maf);
    safef(prevTarget, sizeof(prevTarget), "%s", targetName);
    }
mafFileFree(&mf);
}

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
char dir[256], file[128];
int i = 0;

if (byTarget)
    verbose(1, "Splitting %d files by target sequence -- ignoring first "
	    "argument %s\n",
	    mafCount, splitFile);
else
    verbose(1, "Splitting %d files from %s to %s\n",
	    mafCount, splitFile, outRoot);
splitPath(outRoot, dir, file, NULL);
if (isNotEmpty(dir))
    makeDir(dir);
else
    safef(dir, sizeof(dir), "%s", ".");
if (byTarget)
    {
    struct hash *pathHash = hashNew(17);
    int seqNum = 0;
    for (i = 0; i < mafCount; i++)
	splitMafByTarget(mafFiles[i], dir, file, pathHash, &seqNum);
    }
else
    {
    struct hash *bedHash = loadSplits(splitFile);
    for (i = 0; i < mafCount; i++)
	splitMafFile(mafFiles[i], dir, file, bedHash);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
byTarget = optionExists("byTarget");
outDirDepth = optionInt("outDirDepth", outDirDepth);
useSequenceName = optionExists("useSequenceName");
if (outDirDepth > 0 && !byTarget)
    errAbort("-outDirDepth=N can be specified only when -byTarget is "
	     "specified.");
if (useSequenceName && !byTarget)
    errAbort("-useSequenceName can be specified only when -byTarget is "
	     "specified.");
if (argc < 4)
    usage();
mafSplit(argv[1], argv[2], argc-3, &argv[3]);
return 0;
}
