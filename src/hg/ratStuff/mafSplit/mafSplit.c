/* Split MAF file based on positions in a bed file */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "portable.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "sqlNum.h"
#include "maf.h"
#include "bed.h"


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
  "   -useFullSequenceName  For use only with -byTarget.\n"
  "                     Instead of auto-incrementing an integer to determine\n"
  "                     output filename, use the target sequence name\n"
  "                     to tack onto outRoot.\n"
  "   -useHashedName=N  For use only with -byTarget.\n"
  "                     Instead of auto-incrementing an integer or requiring\n"
  "                     a unique number in the sequence name, use a hash\n"
  "                     function on the sequence name to compute an N-bit\n"
  "                     number.  This limits the max #filenames to 2^N and\n"
  "                     ensures that even if different subsets of sequences\n"
  "                     appear in different pairwise mafs, the split file\n"
  "                     names will be consistent (due to hash function).\n"
  "                     This option is useful when a \"scaffold-based\"\n"
  "                     assembly has more than one sequence name pattern,\n"
  "                     e.g. both chroms and scaffolds.\n"
  "\n"
  );
}

static struct optionSpec options[] = {
   {"byTarget", OPTION_BOOLEAN},
   {"outDirDepth", OPTION_INT},
   {"useSequenceName", OPTION_BOOLEAN},
   {"useFullSequenceName", OPTION_BOOLEAN},
   {"useHashedName", OPTION_INT},
   {NULL, 0},
};

/* Option variables */
static boolean byTarget = FALSE;
static int outDirDepth = 0;
static boolean useSequenceName = FALSE;
static boolean useFullSequenceName = FALSE;
static int hashedNameBits = 0;


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

static int numberFromHashedName(char *name, int hashedNameBits)
/* Run name through a hash function and mask to hashedNameBits. */
{
int hashedName = hashCrc(name);
unsigned mask = (1 << hashedNameBits) - 1;
return hashedName & mask;
}

static char *mkOutPath(char *outRootDir, char *outRootFile, int seqNum, char *target)
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
    if (target)
	dyStringPrintf(dy, "%s%s.maf", outRootFile, target);
    else
	dyStringPrintf(dy, "%s%05d.maf", outRootFile, seqNum);
    }
else
    {
    if (target)
	dyStringPrintf(dy, "%s/%s%s.maf", outRootDir, outRootFile, target);
    else
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

prevTarget[0] = '\0';
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
	char *path;
	if (hashedNameBits > 0)
	    {
	    /* Hash paths by themselves instead of by target, because 
	     * we may end up reusing a path for several targets. */
	    int tHashed = numberFromHashedName(targetName, hashedNameBits);
	    path = mkOutPath(outRootDir, outRootFile, tHashed, NULL);
	    path = (char *)hashFindVal(pathHash, path);
	    }
	else
	    path = (char *)hashFindVal(pathHash, targetName);
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
	    else if (hashedNameBits > 0)
		seqNum = numberFromHashedName(targetName, hashedNameBits);
	    if (useFullSequenceName)
		{
		/* skip over db. prefix if any */
		char *target = strchr(targetName,'.');
		if (target)
		    ++target;
		else
		    target = targetName;
		path = mkOutPath(outRootDir, outRootFile, seqNum, target);
		}
	    else
		path = mkOutPath(outRootDir, outRootFile, seqNum, NULL);
	    verbose(3, "Opening path %s for writing and adding it to hash "
		    "for %s\n", path, targetName);
	    f = mustOpen(path, "w");
	    fprintf(f, "##maf version=1 scoring=blastz\n");
	    if (hashedNameBits > 0)
		hashAdd(pathHash, path, path);
	    else
		hashAdd(pathHash, targetName, path);
	    }
	prevFile = f;
	}
    mafWrite(f, maf);
    safef(prevTarget, sizeof(prevTarget), "%s", targetName);
    mafAliFree(&maf);
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
        safef(outFile, sizeof(outFile), "%s/%s%s.%02d.maf", 
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
        maf = mafSubset(full, mc->src, splitPos , mc->start + mc->size);
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
useFullSequenceName = optionExists("useFullSequenceName");
hashedNameBits = optionInt("useHashedName", hashedNameBits);
if (outDirDepth > 0 && !byTarget)
    errAbort("-outDirDepth=N can be specified only when -byTarget is "
	     "specified.");
if (useSequenceName && !byTarget)
    errAbort("-useSequenceName can be specified only when -byTarget is "
	     "specified.");
if (useFullSequenceName && !byTarget)
    errAbort("-useFullSequenceName can be specified only when -byTarget is "
	     "specified.");
if (hashedNameBits > 0 && !byTarget)
    errAbort("-useHashedName can be specified only when -byTarget is "
	     "specified.");
if (hashedNameBits > 0 && useSequenceName)
    errAbort("-useHashedName and -useSequenceName are mutually exclusive.  "
	     "Please pick one.");
if (hashedNameBits > 0 &&
    (hashedNameBits < 2 || hashedNameBits > 17))
    errAbort("-useHashedName=N: N should be between 2 and 17.  If you "
	     "strongly disagree, modify mafSplit.c.  Watch fileserver if "
	     "exceeding.");
if (argc < 4)
    usage();
mafSplit(argv[1], argv[2], argc-3, &argv[3]);
return 0;
}
