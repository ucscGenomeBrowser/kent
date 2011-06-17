/* bedWeedOverlapping - Filter out beds that overlap a 'weed.bed' file.. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "rangeTree.h"

/* Variables set from command line. */
double maxOverlap = 0.0;
boolean invert = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedWeedOverlapping - Filter out beds that overlap a 'weed.bed' file.\n"
  "usage:\n"
  "   bedWeedOverlapping weeds.bed input.bed output.bed\n"
  "options:\n"
  "   -maxOverlap=0.N - maximum overlapping ratio, default 0 (any overlap)\n"
  "   -invert - keep the overlapping and get rid of everything else\n"
  );
}

static struct optionSpec options[] = {
   {"maxOverlap", OPTION_DOUBLE},
   {"invert", OPTION_BOOLEAN},
   {NULL, 0},
};

void addBedToRangeTree(struct rbTree *rangeTree, struct bed *bed)
/* Add bed to range tree, and exon at a time if it has blocks. */
{
int blockCount = bed->blockCount;
int totalSize = 0;
if (blockCount == 0)
    rangeTreeAdd(rangeTree, bed->chromStart, bed->chromEnd);
else
    {
    int i;
    for (i=0; i<blockCount; ++i)
        {
	int start = bed->chromStart + bed->chromStarts[i];
	int size = bed->blockSizes[i];
	int end = start + size;
	totalSize += size;
	rangeTreeAdd(rangeTree, start, end);
	}
    }
verbose(2, "%s size %d\n", bed->name, totalSize);
}

boolean bedOverlapsRangeTree(struct rbTree *rangeTree, struct bed *bed, double maxOverlapRatio)
/* Return TRUE if bed overlaps with rangeTree, by at least maxOverlapRatio of it's bases. */
{
int totalBases = 0, overlappingBases = 0;
if (bed->blockCount == 0)
    {
    totalBases = bed->chromEnd - bed->chromStart;
    overlappingBases = rangeTreeOverlapSize(rangeTree, bed->chromStart, bed->chromEnd);
    }
else
    {
    int i;
    for (i=0; i<bed->blockCount; ++i)
        {
	int start = bed->chromStart + bed->chromStarts[i];
	int end = start + bed->blockSizes[i];
	totalBases += end - start;
	overlappingBases += rangeTreeOverlapSize(rangeTree, start, end);
	}
    }
if (overlappingBases == 0)
    return FALSE;
else
    return overlappingBases > maxOverlapRatio * totalBases;
}

struct hash *makeWeedChromHash(char *weedFile)
/* Read in beds from weed file.  Create a hash full of range
 * trees that contain all the weed exons. */
{
struct bed *bed, *bedList = bedLoadAll(weedFile);
struct hash *hash = newHash(16); 
struct lm *lm = hash->lm;
struct rbTreeNode **stack;
lmAllocArray(lm, stack, 128);

for (bed = bedList; bed != NULL; bed = bed->next)
    {
    struct rbTree *rangeTree = hashFindVal(hash, bed->chrom);
    if (rangeTree == NULL)
	{
        rangeTree = rangeTreeNewDetailed(lm, stack);
	hashAdd(hash, bed->chrom, rangeTree);
	}
    addBedToRangeTree(rangeTree, bed);
    }
bedFreeList(&bedList);
return hash;
}


void bedWeedOverlapping(char *weedFile, char *inFile, char *outFile)
/* bedWeedOverlapping - Filter out beds that overlap a 'weed.bed' file.. */
{
struct hash *chromHash = makeWeedChromHash(weedFile);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
char *row[256];
int fieldCount;
while ((fieldCount = lineFileChop(lf, row)) != 0)
    {
    if (fieldCount >= ArraySize(row))
        errAbort("Too many words line %d of %s\n", lf->lineIx, lf->fileName);
    struct bed *bed = bedLoadN(row, fieldCount);
    struct rbTree *rangeTree = hashFindVal(chromHash, bed->chrom);
    boolean doOutput = (rangeTree == NULL || !bedOverlapsRangeTree(rangeTree, bed, maxOverlap));
    if (invert)
        doOutput = !doOutput;
    if (doOutput)
	bedTabOutN(bed, fieldCount, f);
    bedFree(&bed);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
maxOverlap = optionDouble("maxOverlap", maxOverlap);
invert = optionExists("invert");
bedWeedOverlapping(argv[1], argv[2], argv[3]);
return 0;
}
