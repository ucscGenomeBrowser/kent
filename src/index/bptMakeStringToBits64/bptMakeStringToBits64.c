/* bptMakeStringToBits64 - Create a B+ tree index with string keys and 64-bit-integer values. 
 * In practice the 64-bit values are often offsets in a file.. */
#include "common.h"
#include "localmem.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "sig.h"

static char const rcsid[] = "$Id: bptMakeStringToBits64.c,v 1.1 2009/01/09 02:59:29 kent Exp $";

int blockSize = 1000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bptMakeStringToBits64 - Create a B+ tree index with string keys and unsigned 64-bit-integer \n"
  "values. In practice the 64-bit values are often offsets in a file.\n"
  "usage:\n"
  "   bptMakeStringToBits64 input.tab output.bpt\n"
  "where input.tab is two columns - name/value."
  "options:\n"
  "   -blockSize=N (default %d) Size of block for index purposes\n"
  , blockSize
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {NULL, 0},
};

/* bpt - B+ Trees.  These are a method of indexing data similar to binary trees, but with
 * many children rather than just two at each node. They work well when stored on disk,
 * since typically only two or three disk accesses are needed to locate any particular
 * piece of data.  This implementation is *just* for disk based storage.  For memory
 * use the rbTree instead.
 *
 * The layout of the file on disk is:
 *    header
 *    root node
 *    (other nodes)
 * In general when the tree is first built the higher level nodes are stored before the
 * lower level nodes.  It is possible if a b+ tree is dynamically updated for this to
 * no longer be strictly true, but actually currently the b+ tree code here doesn't implement
 * dynamic updates - it just creates a b+ tree from a sorted list.
 *
 * Each node can be one of two types - index or leaf.  The index nodes contain pointers
 * to child nodes.  The leaf nodes contain the actual data. 
 *
 * The layout of the file header is:
 *       <magic number>  4 bytes - The value bptSig (0x78CA8C91)
 *       <block size>    4 bytes - Number of children per block (not byte size of block)
 *       <key size>      4 bytes - Number of significant bytes in key
 *       <val size>      4 bytes - Number of bytes in value
 *       <item count>    8 bytes - Number of items in index
 *       <reserved2>     4 bytes - Always 0 for now
 *       <reserved3>     4 bytes - Always 0 for now
 * The magic number may be byte-swapped, in which case all numbers in the file
 * need to be byte-swapped. 
 *
 * The nodes start with a header:
 *       <is leaf>       1 byte  - 1 for leaf nodes, 0 for index nodes.
 *       <reserved>      1 byte  - Always 0 for now.
 *       <count>         2 bytes - The number of children/items in node
 * This is followed by count items.  For the index nodes the items are
 *       <key>           key size bytes - always written most significant byte first
 *       <offset>        8 bytes - Offset of child node in index file.
 * For leaf nodes the items are
 *       <key>           key size bytes - always written most significant byte first
 *       <val>           val sized bytes - the value associated with the key.
 * Note in general the leaf nodes may not be the same size as the index nodes, though in
 * the important case where the values are file offsets they will be.
 */

#define bptFileHeaderSize 32
#define bptBlockHeaderSize 4

/* This section of code deals with making balanced b+ trees given a sorted array as input.
 * The difficult part is mostly just calculating the offsets of various things.  As an example
 * if you had the sorted array:
 *   01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27
 * and wanted to create a tree of block size 4, the resulting tree would have three levels
 * as so:
 *  01 17
 *  01 05 09 13   17 21 25
 *  01 02 03 04   05 06 07 08  09 10 11 12   13 14 15 16   17 18 19 20   21 22 23 24  25 26 27
 */

static int xToY(int x, unsigned y)
/* Return x to the Y power, with y usually small. */
{
int i, val = 1;
for (i=0; i<y; ++i)
    val *= x;
return val;
}

int bptCountLevels(int maxBlockSize, int itemCount)
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


static bits32 writeIndexLevel(bits16 blockSize, 
	void *itemArray, int itemSize, int itemCount, 
	bits32 indexOffset, int level, 
	void (*fetchKey)(const void *va, char *keyBuf), bits32 keySize,
	FILE *f)
/* Write out a non-leaf level. */
{
char *items = itemArray;

/* Calculate number of nodes to write at this level. */
int slotSizePer = xToY(blockSize, level);   // Number of items per slot in node
int nodeSizePer = slotSizePer * blockSize;  // Number of items per node
int nodeCount = (itemCount + nodeSizePer - 1)/nodeSizePer;	

/* Calculate sizes and offsets. */
int bytesInBlock = (bptBlockHeaderSize + blockSize * (keySize+sizeof(bits64)));
bits64 levelSize = nodeCount * bytesInBlock;
bits64 endLevel = indexOffset + levelSize;
bits64 nextChild = endLevel;

UBYTE isLeaf = FALSE;
UBYTE reserved = 0;

int i,j;
char keyBuf[keySize+1];
keyBuf[keySize] = 0;
for (i=0; i<itemCount; i += nodeSizePer)
    {
    /* Calculate size of this block */
    bits16 countOne = (itemCount - i + slotSizePer - 1)/slotSizePer;
    if (countOne > blockSize)
        countOne = blockSize;

    /* Write block header. */
    writeOne(f, isLeaf);
    writeOne(f, reserved);
    writeOne(f, countOne);

    /* Write out the slots that are used one by one, and do sanity check. */
    int slotsUsed = 0;
    int endIx = i + nodeSizePer;
    if (endIx > itemCount)
        endIx = itemCount;
    for (j=i; j<endIx; j += slotSizePer)
        {
	void *item = items + j*itemSize;
	memset(keyBuf, 0, keySize);
	(*fetchKey)(item, keyBuf);
	mustWrite(f, keyBuf, keySize);
	writeOne(f, nextChild);
	nextChild += bytesInBlock;
	++slotsUsed;
	}
    assert(slotsUsed == countOne);

    /* Write out empty slots as all zero. */
    int slotSize = keySize + sizeof(bits64);
    for (j=countOne; j<blockSize; ++j)
	repeatCharOut(f, 0, slotSize);
    }
return endLevel;
}

void writeLeafLevel(bits16 blockSize, void *itemArray, int itemSize, int itemCount, 
	void (*fetchKey)(const void *va, char *keyBuf), bits32 keySize,
	void* (*fetchVal)(const void *va), bits32 valSize,
	FILE *f)
/* Write out leaf level blocks. */
{
char *items = itemArray;
int i,j;
UBYTE isLeaf = TRUE;
UBYTE reserved = 0;
bits16 countOne;
int countLeft = itemCount;
char keyBuf[keySize+1];
keyBuf[keySize] = 0;
for (i=0; i<itemCount; i += countOne)
    {
    /* Write block header */
    if (countLeft > blockSize)
        countOne = blockSize;
    else
        countOne = countLeft;
    writeOne(f, isLeaf);
    writeOne(f, reserved);
    writeOne(f, countOne);

    /* Write out position in genome and in file for each item. */
    for (j=0; j<countOne; ++j)
        {
	assert(i+j < itemCount);
	void *item = items + (i+j)*itemSize;
	memset(keyBuf, 0, keySize);
	(*fetchKey)(item, keyBuf);
	mustWrite(f, keyBuf, keySize);
	mustWrite(f, (*fetchVal)(item), valSize);
	}
    
    /* Pad out any unused bits of last block with zeroes. */
    int slotSize = keySize + valSize;
    for (j=countOne; j<blockSize; ++j)
	repeatCharOut(f, 0, slotSize);

    countLeft -= countOne;
    }
}


void bptWriteIndex(void *itemArray, int itemSize, bits64 itemCount, bits32 blockSize,
	void (*fetchKey)(const void *va, char *keyBuf), bits32 keySize,
	void* (*fetchVal)(const void *va), bits32 valSize, char *fileName)
/* Create a b+ tree index. */
{
/* Open file and write header. */
FILE *f = mustOpen(fileName, "wb");
bits32 magic = bptSig;
bits32 reserved = 0;
writeOne(f, magic);
writeOne(f, blockSize);
writeOne(f, keySize);
writeOne(f, valSize);
writeOne(f, itemCount);
writeOne(f, reserved);
writeOne(f, reserved);
bits64 indexOffset = ftell(f);

/* Write non-leaf nodes. */
int levels = bptCountLevels(blockSize, itemCount);
int i;
for (i=levels-1; i > 0; --i)
    {
    bits32 endLevelOffset = writeIndexLevel(blockSize, itemArray, itemSize, itemCount, indexOffset, 
    	i, fetchKey, keySize, f);
    indexOffset = ftell(f);
    if (endLevelOffset != indexOffset)
        internalErr();
    }

/* Write leaf nodes */
writeLeafLevel(blockSize, itemArray, itemSize, itemCount, 
	fetchKey, keySize, fetchVal, valSize, f);
}

struct name64
/* Pair of a name and a 64-bit integer. */
    {
    struct name64 *next;
    char *name;
    bits64 val;
    };

int name64Cmp(const void *va, const void *vb)
/* Compare to sort on name. */
{
const struct name64 *a = *((struct name64 **)va);
const struct name64 *b = *((struct name64 **)vb);
return strcmp(a->name, b->name);
}

void name64Key(const void *va, char *keyBuf)
/* Get key field. */
{
const struct name64 *a = *((struct name64 **)va);
strcpy(keyBuf, a->name);
}

void *name64Val(const void *va)
/* Get key field. */
{
const struct name64 *a = *((struct name64 **)va);
return (void*)(&a->val);
}

void bptMakeStringToBits64(char *inFile, char *outIndex)
/* bptMakeStringToBits64 - Create a B+ tree index with string keys and 64-bit-integer values. 
 * In practice the 64-bit values are often offsets in a file.. */
{
/* Read inFile into a list in local memory. */
struct lm *lm = lmInit(0);
struct name64 *el, *list = NULL;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    lmAllocVar(lm, el);
    el->name = lmCloneString(lm, row[0]);
    el->val = sqlLongLong(row[1]);
    slAddHead(&list, el);
    }
lineFileClose(&lf);

int count = slCount(list);
if (count > 0)
    {
    /* Convert list into sorted array */
    struct name64 **itemArray = NULL;
    AllocArray(itemArray, count);
    int i;
    for (i=0, el=list; i<count; i++, el = el->next)
        itemArray[i] = el;
    qsort(itemArray, count, sizeof(itemArray[0]), name64Cmp);

    /* Figure out max size of name field. */
    int maxSize = 0;
    for (i=0; i<count; ++i)
        {
	int size = strlen(itemArray[i]->name);
	if (maxSize < size)
	    maxSize = size;
	}

    bptWriteIndex(itemArray, sizeof(itemArray[0]), count, blockSize,
    	name64Key, maxSize, name64Val, sizeof(bits64), outIndex);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
blockSize = optionInt("blockSize", blockSize);
int minBlockSize = 2, maxBlockSize = (1L << 16) - 1;
if (blockSize < minBlockSize || blockSize > maxBlockSize)
    errAbort("Block size (%d) not in range, must be between %d and %d",
    	blockSize, minBlockSize, maxBlockSize);
bptMakeStringToBits64(argv[1], argv[2]);
return 0;
}
