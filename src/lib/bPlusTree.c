/* bptFile - B+ Trees.  These are a method of indexing data similar to binary trees, but 
 * with many children rather than just two at each node. They work well when stored on disk,
 * since typically only two or three disk accesses are needed to locate any particular
 * piece of data.  This implementation is *just* for disk based storage.  For memory
 * use the rbTree instead. Currently the implementation is just useful for data warehouse
 * type applications.  That is it implements a function to create a b+ tree from bulk data
 * (bptFileCreate) and a function to lookup a value given a key (bptFileFind) but not functions
 * to add or delete individual items.
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

#include "common.h"
#include "sig.h"
#include "bPlusTree.h"

/* This section of code deals with locating a value in a b+ tree. */

struct bptFile *bptFileOpen(char *fileName)
/* Open up index file - reading header and verifying things. */
{
/* Open file and allocate structure to hold info from header etc. */
struct bptFile *bpt = needMem(sizeof(*bpt));
bpt->fileName = cloneString(fileName);
FILE *f = mustOpen(fileName, "rb");
bpt->f = f;

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = FALSE;
mustReadOne(f, magic);
if (magic != bptSig)
    {
    magic = byteSwap32(magic);
    isSwapped = bpt->isSwapped = TRUE;
    if (magic != bptSig)
       errAbort("%s is not a bpt b-plus tree index file", fileName);
    }

/* Read rest of defined bits of header, byte swapping as needed. */
bpt->blockSize = readBits32(f, isSwapped);
bpt->keySize = readBits32(f, isSwapped);
bpt->valSize = readBits32(f, isSwapped);
bpt->itemCount = readBits64(f, isSwapped);

/* Skip over reserved bits of header. */
bits32 reserved32;
mustReadOne(f, reserved32);
mustReadOne(f, reserved32);

/* Save position of root block of b+ tree. */
bpt->rootOffset = ftell(f);

return bpt;
}

void bptFileClose(struct bptFile **pBpt)
/* Close down and deallocate index file. */
{
struct bptFile *bpt = *pBpt;
if (bpt != NULL)
    {
    carefulClose(&bpt->f);
    freeMem(bpt->fileName);
    freez(&bpt);
    }
}

static boolean rFind(struct bptFile *bpt, bits64 blockStart, void *key, void *val)
/* Find value corresponding to key.  If found copy value to memory pointed to by val and return 
 * true. Otherwise return false. */
{
/* Seek to start of block. */
fseek(bpt->f, blockStart, SEEK_SET);

/* Read block header. */
UBYTE isLeaf;
UBYTE reserved;
bits16 i, childCount;
mustReadOne(bpt->f, isLeaf);
mustReadOne(bpt->f, reserved);
boolean isSwapped = bpt->isSwapped;
childCount = readBits16(bpt->f, isSwapped);

UBYTE keyBuf[bpt->keySize];   /* Place to put a key, buffered on stack. */

if (isLeaf)
    {
    for (i=0; i<childCount; ++i)
        {
	mustRead(bpt->f, keyBuf, bpt->keySize);
	mustRead(bpt->f, val, bpt->valSize);
	if (memcmp(key, keyBuf, bpt->keySize) == 0)
	    return TRUE;
	}
    return FALSE;
    }
else
    {
    /* Read and discard first key. */
    mustRead(bpt->f, keyBuf, bpt->keySize);

    /* Scan info for first file offset. */
    bits64 fileOffset = readBits64(bpt->f, isSwapped);

    /* Loop through remainder. */
    for (i=1; i<childCount; ++i)
	{
	mustRead(bpt->f, keyBuf, bpt->keySize);
	if (memcmp(key, keyBuf, bpt->keySize) < 0)
	    break;
	fileOffset = readBits64(bpt->f, isSwapped);
	}
    return rFind(bpt, fileOffset, key, val);
    }
}

boolean bptFileFind(struct bptFile *bpt, void *key, int keySize, void *val, int valSize)
/* Find value associated with key.  Return TRUE if it's found. 
*  Parameters:
*     bpt - file handle returned by bptFileOpen
*     key - pointer to key string, which needs to be bpt->keySize long
*     val - pointer to where to put retrieved value
*/
{
/* Check key size vs. file key size, and act appropriately.  If need be copy key to a local
 * buffer and zero-extend it. */
if (keySize > bpt->keySize)
    return FALSE;
char keyBuf[keySize];
if (keySize != bpt->keySize)
    {
    memcpy(keyBuf, key, keySize);
    memset(keyBuf+keySize, 0, bpt->keySize - keySize);
    key = keyBuf;
    }

/* Make sure the valSize matches what's in file. */
if (valSize != bpt->valSize)
    errAbort("Value size mismatch between bptFileFind (valSize=%d) and %s (valSize=%d)",
    	valSize, bpt->fileName, bpt->valSize);

/* Call recursive finder. */
return rFind(bpt, bpt->rootOffset, key, val);
}


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

static int bptCountLevels(int maxBlockSize, int itemCount)
/* Count up number of levels needed in tree of given maximum block size. */
{
int levels = 1;
while (itemCount > maxBlockSize)
    {
    itemCount = (itemCount + maxBlockSize - 1)  / maxBlockSize;
    levels += 1;
    }
return levels;
}


static bits32 writeIndexLevel(bits16 blockSize, 
	void *itemArray, int itemSize, int itemCount, 
	bits32 indexOffset, int level, 
	void (*fetchKey)(const void *va, char *keyBuf), bits32 keySize, bits32 valSize,
	FILE *f)
/* Write out a non-leaf level. */
{
char *items = itemArray;

/* Calculate number of nodes to write at this level. */
int slotSizePer = xToY(blockSize, level);   // Number of items per slot in node
int nodeSizePer = slotSizePer * blockSize;  // Number of items per node
int nodeCount = (itemCount + nodeSizePer - 1)/nodeSizePer;	

/* Calculate sizes and offsets. */
int bytesInIndexBlock = (bptBlockHeaderSize + blockSize * (keySize+sizeof(bits64)));
int bytesInLeafBlock = (bptBlockHeaderSize + blockSize * (keySize+valSize));
bits64 bytesInNextLevelBlock = (level == 1 ? bytesInLeafBlock : bytesInIndexBlock);
bits64 levelSize = nodeCount * bytesInIndexBlock;
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
	nextChild += bytesInNextLevelBlock;
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

static void writeLeafLevel(bits16 blockSize, void *itemArray, int itemSize, int itemCount, 
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


void bptFileCreate(void *itemArray, int itemSize, bits64 itemCount, bits32 blockSize,
	void (*fetchKey)(const void *va, char *keyBuf), bits32 keySize,
	void* (*fetchVal)(const void *va), bits32 valSize, char *fileName)
/* Create a b+ tree index from a sorted array. */
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
    	i, fetchKey, keySize, valSize, f);
    indexOffset = ftell(f);
    if (endLevelOffset != indexOffset)
        internalErr();
    }

/* Write leaf nodes */
writeLeafLevel(blockSize, itemArray, itemSize, itemCount, 
	fetchKey, keySize, fetchVal, valSize, f);
}

