/* chromToPos - Given list of chromosomes and sizes, create files that index them by position 
 * in genome via a b+ tree. 
 *    This module is as much about experimenting with b+ tree implementation as anything, 
 * though once that is stable, it'll probably be refactored into a b+ tree library module
 * with this being just a client. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sig.h"
#include "net.h"


int blockSize = 100;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chromToPos - Given list of chromosomes and sizes, create files that index them by position\n"
  "via a b+ tree.\n"
  "usage:\n"
  "   chromToPos chromSizes.txt chromPos.bin chromPos.ix\n"
  "options:\n"
  "   -blockSize=N (default %d) Size of block for index purposes\n"
  , blockSize);
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {NULL, 0},
};

struct chromInfo
/* A chromosome name and a chromosome size. */
    {
    struct chromInfo *next;	/* Next in list. */
    char *name;			/* Chromosome name. */
    bits32 size;		/* Size of chromosome. */
    bits32 genomeOffset;	/* Offset of chromosome in genome coordinates. */
    bits32 binFileOffset;	/* Offset of record in binary file. */
    };

struct chromInfo *readChromSizes(char *fileName)
/* Create list of chromInfos based on a two column file <chrom><size> */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[2];
struct chromInfo *list = NULL, *el;
bits64 maxTotal = (1LL << 32) - 1;
bits64 total = 0;
int chromCount = 0;
struct hash *uniqHash = hashNew(16);

while (lineFileChop(lf, row))
    {
    char *name = row[0];
    if (hashLookup(uniqHash, name))
       errAbort("Duplicate chromosome or contig name %s line %d of %s",
       	name, lf->lineIx, lf->fileName);
    hashAdd(uniqHash, name, NULL);
    AllocVar(el);
    el->name = cloneString(name);
    el->size = lineFileNeedNum(lf, row, 1);
    el->genomeOffset = total;
    total += el->size;
    if (total > maxTotal)
        errAbort("Too many bases line %d of %s.  Max is %lld,  total so far is %lld",
		lf->lineIx, lf->fileName, maxTotal, total);
    slAddHead(&list, el);
    ++chromCount;
    }
hashFree(&uniqHash);
lineFileClose(&lf);
slReverse(&list);
verbose(1, "Read %d chroms totalling %lld bases in %s\n", chromCount, total, fileName);
return list;
}

int countLevels(int maxBlockSize, int itemCount)
/* Count up number of levels needed in tree of given maximum block size. */
{
int levels = 1;
while (itemCount > maxBlockSize)
    {
    itemCount = (itemCount + maxBlockSize - 1)  / blockSize;
    levels += 1;
    }
return levels;
}

void writeBinSaveOffsets(struct chromInfo *chromList, int chromCount, char *fileName)
/* Save chromosome info as a binary file, and save offsets of each chromosome within file. */
{
bits32 magic = chromSizeBinSig;
bits32 count = chromCount;
bits32 reserved = 0;
FILE *f = mustOpen(fileName, "wb");

writeOne(f, magic);
writeOne(f, chromCount);
writeOne(f, reserved);
writeOne(f, reserved);

struct chromInfo *chrom;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    chrom->binFileOffset = ftell(f);
    writeOne(f, chrom->genomeOffset);
    mustWrite(f, chrom->name, strlen(chrom->name)+1);
    }

carefulClose(&f);
}

int xToY(int x, unsigned y)
/* Return x to the Y power, with y usually small. */
{
int i, val = 1;
for (i=0; i<y; ++i)
    val *= x;
return val;
}

/* Say have a block size of 4 and 27 items.  You'd put them in the tree as so:
 *  01 17
 *  01 05 09 13   17 21 25
 *  01 02 03 04   05 06 07 08  09 10 11 12   13 14 15 16   17 18 19 20   21 22 23 24  25 26 27
 */


bits32 writeIndexLevel(struct chromInfo **chromArray, int chromCount, 
	bits32 indexOffset, int level, FILE *f)
/* Write out a non-leaf level. */
{
/* Calculate number of nodes to write at this level. */
int slotSizePer = xToY(blockSize, level);   // Number of chroms per slot in node
int nodeSizePer = slotSizePer * blockSize;  // Number of chroms per node
int nodeCount = (chromCount + nodeSizePer - 1)/nodeSizePer;	

/* Calculate sizes and offsets. */
int bytesInBlock = (2*sizeof(UBYTE) + sizeof(bits16) + blockSize * (2*sizeof(bits32)));
bits32 levelSize = nodeCount * bytesInBlock;
bits32 endLevel = indexOffset + levelSize;
bits32 nextChild = endLevel;

UBYTE isLeaf = FALSE;
UBYTE reserved = 0;

int i,j;
for (i=0; i<chromCount; i += nodeSizePer)
    {
    /* Calculate size of this block */
    bits16 countOne = (chromCount - i + slotSizePer - 1)/slotSizePer;
    if (countOne > blockSize)
        countOne = blockSize;

    /* Write block header. */
    writeOne(f, isLeaf);
    writeOne(f, reserved);
    writeOne(f, countOne);

    int slotsUsed = 0;
    int endIx = i + nodeSizePer;
    if (endIx > chromCount)
        endIx = chromCount;
    for (j=i; j<endIx; j += slotSizePer)
        {
        struct chromInfo *chrom = chromArray[j];
	writeOne(f, chrom->genomeOffset);
	writeOne(f, nextChild);
	nextChild += bytesInBlock;
	++slotsUsed;
	}
    assert(slotsUsed == countOne);
    for (j=countOne; j<blockSize; ++j)
        {
	bits32 genomeOffsetPad=0;
	bits32 binFileOffsetPad=0;
	writeOne(f, genomeOffsetPad);
	writeOne(f, binFileOffsetPad);
	}
    }
return endLevel;
}

void writeLeafLevel(struct chromInfo **chromArray, int chromCount, FILE *f)
/* Write out leaf level blocks. */
{
int i,j;
UBYTE isLeaf = TRUE;
UBYTE reserved = 0;
bits16 countOne;
bits32 genomeOffsetPad=0;
bits32 binFileOffsetPad=0;
int countLeft = chromCount;
for (i=0; i<chromCount; i += countOne)
    {
    /* Write block header */
    if (countLeft > blockSize)
        countOne = blockSize;
    else
        countOne = countLeft;
    writeOne(f, isLeaf);
    writeOne(f, reserved);
    writeOne(f, countOne);

    /* Write out position in genome and in file for each chrom. */
    for (j=0; j<countOne; ++j)
        {
	assert(i+j < chromCount);
        struct chromInfo *chrom = chromArray[i+j];
	writeOne(f, chrom->genomeOffset);
	writeOne(f, chrom->binFileOffset);
	}
    
    /* Pad out any unused bits of last block with zeroes. */
    for (j=countOne; j<blockSize; ++j)
        {
	writeOne(f, genomeOffsetPad);
	writeOne(f, binFileOffsetPad);
	}

    countLeft -= countOne;
    }
}

void writeIndex(struct chromInfo *chromList, int chromCount, char *fileName)
/* Write index file - a b+ tree. */
{
/* Open file and write out header. */
FILE *f = mustOpen(fileName, "w");
bits32 magic = chromSizeIndexSig;
bits32 count = chromCount;
bits16 bSize = blockSize;
bits16 reserved16 = 0;
bits32 reserved32 = 0;
writeOne(f, magic);
writeOne(f, chromCount);
writeOne(f, bSize);
writeOne(f, reserved16);
writeOne(f, reserved32);
bits32 indexOffset = ftell(f);

/* Make array for all chromosomes. */
struct chromInfo *chrom, **chromArray;
AllocArray(chromArray, chromCount);
int i;
for (i=0, chrom=chromList; i<chromCount; ++i, chrom=chrom->next)
    chromArray[i] = chrom;

/* Figure out how many levels in B tree, and number of chroms between items at highest level. */
int levels = countLevels(blockSize, chromCount);
verbose(1, "%d levels with blockSize %d covers %d items\n", levels, blockSize, chromCount);

/* Write non-leaf nodes. */
for (i=levels-1; i > 0; --i)
    {
    bits32 endLevelOffset = writeIndexLevel(chromArray, chromCount, indexOffset, i, f);
    indexOffset = ftell(f);
    if (endLevelOffset != indexOffset)
        errAbort("internal err: mismatch endLevelOffset=%u vs indexOffset=%u", endLevelOffset, indexOffset);
    }

/* Write leaf nodes */
writeLeafLevel(chromArray, chromCount, f);

/* Clean up and go home. */
freez(&chromArray);
carefulClose(&f);
}

void chromToPos(char *sizesText, char *posBin, char *posIndex)
/* chromToPos - Given list of chromosomes and sizes, create files that index them by position via 
 * a b+ tree. */
{
struct chromInfo *chrom, *chromList = readChromSizes(sizesText);
int chromCount = slCount(chromList);
writeBinSaveOffsets(chromList, chromCount, posBin);
writeIndex(chromList, chromCount, posIndex);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
blockSize = optionInt("blockSize", blockSize);
int minBlockSize = 2, maxBlockSize = (1L << 16) - 1;
if (blockSize < minBlockSize || blockSize > maxBlockSize)
    errAbort("Block size (%d) not in range, must be between %d and %d",
    	blockSize, minBlockSize, maxBlockSize);
chromToPos(argv[1], argv[2], argv[3]);
return 0;
}
