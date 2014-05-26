/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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
#include "udc.h"
#include "bPlusTree.h"

/* This section of code deals with locating a value in a b+ tree. */

struct bptFile *bptFileAttach(char *fileName, struct udcFile *udc)
/* Open up index file on previously open file, with header at current file position. */
{
/* Open file and allocate structure to hold info from header etc. */
struct bptFile *bpt = needMem(sizeof(*bpt));
bpt->fileName = fileName;
bpt->udc = udc;

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = FALSE;
udcMustReadOne(udc, magic);
if (magic != bptSig)
    {
    magic = byteSwap32(magic);
    isSwapped = bpt->isSwapped = TRUE;
    if (magic != bptSig)
       errAbort("%s is not a bpt b-plus tree index file", fileName);
    }

/* Read rest of defined bits of header, byte swapping as needed. */
bpt->blockSize = udcReadBits32(udc, isSwapped);
bpt->keySize = udcReadBits32(udc, isSwapped);
bpt->valSize = udcReadBits32(udc, isSwapped);
bpt->itemCount = udcReadBits64(udc, isSwapped);

/* Skip over reserved bits of header. */
bits32 reserved32;
udcMustReadOne(udc, reserved32);
udcMustReadOne(udc, reserved32);

/* Save position of root block of b+ tree. */
bpt->rootOffset = udcTell(udc);

return bpt;
}

void bptFileDetach(struct bptFile **pBpt)
/* Detach and free up cirTree file opened with cirTreeFileAttach. */
{
freez(pBpt);
}

struct bptFile *bptFileOpen(char *fileName)
/* Open up index file - reading header and verifying things. */
{
return bptFileAttach(cloneString(fileName), udcFileOpen(fileName, udcDefaultDir()));
}

void bptFileClose(struct bptFile **pBpt)
/* Close down and deallocate index file. */
{
struct bptFile *bpt = *pBpt;
if (bpt != NULL)
    {
    udcFileClose(&bpt->udc);
    freeMem(bpt->fileName);
    bptFileDetach(pBpt);
    }
}

static boolean rFind(struct bptFile *bpt, bits64 blockStart, void *key, void *val)
/* Find value corresponding to key.  If found copy value to memory pointed to by val and return 
 * true. Otherwise return false. */
{
/* Seek to start of block. */
udcSeek(bpt->udc, blockStart);

/* Read block header. */
UBYTE isLeaf;
UBYTE reserved;
bits16 i, childCount;
udcMustReadOne(bpt->udc, isLeaf);
udcMustReadOne(bpt->udc, reserved);
boolean isSwapped = bpt->isSwapped;
childCount = udcReadBits16(bpt->udc, isSwapped);

UBYTE keyBuf[bpt->keySize];   /* Place to put a key, buffered on stack. */

if (isLeaf)
    {
    for (i=0; i<childCount; ++i)
        {
	udcMustRead(bpt->udc, keyBuf, bpt->keySize);
	udcMustRead(bpt->udc, val, bpt->valSize);
	if (memcmp(key, keyBuf, bpt->keySize) == 0)
	    return TRUE;
	}
    return FALSE;
    }
else
    {
    /* Read and discard first key. */
    udcMustRead(bpt->udc, keyBuf, bpt->keySize);

    /* Scan info for first file offset. */
    bits64 fileOffset = udcReadBits64(bpt->udc, isSwapped);

    /* Loop through remainder. */
    for (i=1; i<childCount; ++i)
	{
	udcMustRead(bpt->udc, keyBuf, bpt->keySize);
	if (memcmp(key, keyBuf, bpt->keySize) < 0)
	    break;
	fileOffset = udcReadBits64(bpt->udc, isSwapped);
	}
    return rFind(bpt, fileOffset, key, val);
    }
}

static void rFindMulti(struct bptFile *bpt, bits64 blockStart, void *key, struct slRef **pList)
/* Find values corresponding to key and add them to pList.  You'll need to 
 * Do a slRefFreeListAndVals() on the list when done. */
{
/* Seek to start of block. */
udcSeek(bpt->udc, blockStart);

/* Read block header. */
UBYTE isLeaf;
UBYTE reserved;
bits16 i, childCount;
udcMustReadOne(bpt->udc, isLeaf);
udcMustReadOne(bpt->udc, reserved);
boolean isSwapped = bpt->isSwapped;
childCount = udcReadBits16(bpt->udc, isSwapped);

int keySize = bpt->keySize;
UBYTE keyBuf[keySize];   /* Place to put a key, buffered on stack. */
UBYTE valBuf[bpt->valSize];   /* Place to put a value, buffered on stack. */

if (isLeaf)
    {
    for (i=0; i<childCount; ++i)
        {
	udcMustRead(bpt->udc, keyBuf, keySize);
	udcMustRead(bpt->udc, valBuf, bpt->valSize);
	if (memcmp(key, keyBuf, keySize) == 0)
	    {
	    void *val = cloneMem(valBuf, bpt->valSize);
	    refAdd(pList, val);
	    }
	}
    }
else
    {
    /* Read first key and first file offset. */
    udcMustRead(bpt->udc, keyBuf, keySize);
    bits64 lastFileOffset = udcReadBits64(bpt->udc, isSwapped);
    bits64 fileOffset = lastFileOffset;
    int lastCmp = memcmp(key, keyBuf, keySize);

    /* Loop through remainder. */
    for (i=1; i<childCount; ++i)
	{
	udcMustRead(bpt->udc, keyBuf, keySize);
	fileOffset = udcReadBits64(bpt->udc, isSwapped);
	int cmp = memcmp(key, keyBuf, keySize);
	if (lastCmp >= 0 && cmp <= 0)
	    {
	    bits64 curPos = udcTell(bpt->udc);
	    rFindMulti(bpt, lastFileOffset, key, pList);
	    udcSeek(bpt->udc, curPos);
	    }
	if (cmp < 0)
	    return;
	lastCmp = cmp;
	lastFileOffset = fileOffset;
	}
    /* If made it all the way to end, do last one too. */
    rFindMulti(bpt, fileOffset, key, pList);
    }
}


static void rTraverse(struct bptFile *bpt, bits64 blockStart, void *context, 
    void (*callback)(void *context, void *key, int keySize, void *val, int valSize) )
/* Recursively go across tree, calling callback at leaves. */
{
/* Seek to start of block. */
udcSeek(bpt->udc, blockStart);

/* Read block header. */
UBYTE isLeaf;
UBYTE reserved;
bits16 i, childCount;
udcMustReadOne(bpt->udc, isLeaf);
udcMustReadOne(bpt->udc, reserved);
boolean isSwapped = bpt->isSwapped;
childCount = udcReadBits16(bpt->udc, isSwapped);

char keyBuf[bpt->keySize], valBuf[bpt->valSize];
if (isLeaf)
    {
    for (i=0; i<childCount; ++i)
        {
	udcMustRead(bpt->udc, keyBuf, bpt->keySize);
	udcMustRead(bpt->udc, valBuf, bpt->valSize);
	callback(context, keyBuf, bpt->keySize, valBuf, bpt->valSize);
	}
    }
else
    {
    bits64 fileOffsets[childCount];
    /* Loop through to get file offsets of children. */
    for (i=0; i<childCount; ++i)
	{
	udcMustRead(bpt->udc, keyBuf, bpt->keySize);
	fileOffsets[i] = udcReadBits64(bpt->udc, isSwapped);
	}
    /* Loop through recursing on child offsets. */
    for (i=0; i<childCount; ++i)
	rTraverse(bpt, fileOffsets[i], context, callback);
    }
}

static bits64 bptDataStart(struct bptFile *bpt)
/* Return offset of first bit of data (as opposed to index) in file.  In hind sight I wish
 * this were stored in the header, but fortunately it's not that hard to compute. */
{
bits64 offset = bpt->rootOffset;
for (;;)
    {
    /* Seek to block start */
    udcSeek(bpt->udc, offset);

    /* Read block header,  break if we are leaf. */
    UBYTE isLeaf;
    UBYTE reserved;
    bits16 childCount;
    udcMustReadOne(bpt->udc, isLeaf);
    if (isLeaf)
         break;
    udcMustReadOne(bpt->udc, reserved);
    boolean isSwapped = bpt->isSwapped;
    childCount = udcReadBits16(bpt->udc, isSwapped);

    /* Read and discard first key. */
    char keyBuf[bpt->keySize];
    udcMustRead(bpt->udc, keyBuf, bpt->keySize);

    /* Get file offset of sub-block. */
    offset = udcReadBits64(bpt->udc, isSwapped);
    }
return offset;
}

static bits64 bptDataOffset(struct bptFile *bpt, bits64 itemPos)
/* Return position of file of data corresponding to given itemPos.  For first piece of
 * data pass in 0. */
{
if (itemPos >= bpt->itemCount)
    errAbort("Item index %lld greater than item count %lld in %s", 
	itemPos, bpt->itemCount, bpt->fileName);
bits64 blockPos = itemPos/bpt->blockSize;
bits32 insidePos = itemPos - blockPos*bpt->blockSize;
int blockHeaderSize = 4;
bits64 itemByteSize = bpt->valSize + bpt->keySize;
bits64 blockByteSize = bpt->blockSize * itemByteSize + blockHeaderSize;
bits64 blockOffset = blockByteSize*blockPos + bptDataStart(bpt);
bits64 itemOffset = blockOffset + blockHeaderSize + itemByteSize * insidePos;
return itemOffset;
}

void bptKeyAtPos(struct bptFile *bpt, bits64 itemPos, void *result)
/* Fill in result with the key at given itemPos.  For first piece of data itemPos is 0 
 * Result must be at least bpt->keySize.  If result is a string it won't be zero terminated
 * by this routine.  Use bptStringKeyAtPos instead. */
{
bits64 offset = bptDataOffset(bpt, itemPos);
udcSeek(bpt->udc, offset);
udcMustRead(bpt->udc, result, bpt->keySize);
}

void bptStringKeyAtPos(struct bptFile *bpt, bits64 itemPos, char *result, int maxResultSize)
/* Fill in result with the key at given itemPos.  The maxResultSize should be 1+bpt->keySize
 * to accommodate zero termination of string. */
{
assert(maxResultSize > bpt->keySize);
bptKeyAtPos(bpt, itemPos, result);
result[bpt->keySize] = 0;
}

static boolean bptFileFindMaybeMulti(struct bptFile *bpt, void *key, int keySize, int valSize,
    boolean multi, void *singleVal, struct slRef **multiVal)
/* Do either a single or multiple find depending in multi parameter.  Only one of singleVal
 * or multiVal should be non-NULL, depending on the same parameter. */
{
/* Check key size vs. file key size, and act appropriately.  If need be copy key to a local
 * buffer and zero-extend it. */
if (keySize > bpt->keySize)
    return FALSE;
char keyBuf[bpt->keySize];
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

if (multi)
    {
    rFindMulti(bpt, bpt->rootOffset, key, multiVal);
    return *multiVal != NULL;
    }
else
    return rFind(bpt, bpt->rootOffset, key, singleVal);
}

boolean bptFileFind(struct bptFile *bpt, void *key, int keySize, void *val, int valSize)
/* Find value associated with key.  Return TRUE if it's found. 
*  Parameters:
*     bpt - file handle returned by bptFileOpen
*     key - pointer to key string, which needs to be bpt->keySize long
*     val - pointer to where to put retrieved value
*/
{
return bptFileFindMaybeMulti(bpt, key, keySize, valSize, FALSE, val, NULL);
}

struct slRef *bptFileFindMultiple(struct bptFile *bpt, void *key, int keySize, int valSize)
/* Find all values associated with key.  Store this in ->val item of returned list. 
 * Do a slRefFreeListAndVals() on list when done. */
{
struct slRef *list = NULL;
bptFileFindMaybeMulti(bpt, key, keySize, valSize, TRUE, NULL, &list);
slReverse(&list);
return list;
}

void bptFileTraverse(struct bptFile *bpt, void *context,
    void (*callback)(void *context, void *key, int keySize, void *val, int valSize) )
/* Traverse bPlusTree on file, calling supplied callback function at each
 * leaf item. */
{
return rTraverse(bpt, bpt->rootOffset, context, callback);
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

static long xToY(int x, unsigned y)
/* Return x to the Y power, with y usually small. */
{
long i, val = 1;
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
	void *itemArray, int itemSize, long itemCount, 
	bits32 indexOffset, int level, 
	void (*fetchKey)(const void *va, char *keyBuf), bits32 keySize, bits32 valSize,
	FILE *f)
/* Write out a non-leaf level. */
{
char *items = itemArray;

/* Calculate number of nodes to write at this level. */
long slotSizePer = xToY(blockSize, level);   // Number of items per slot in node
long nodeSizePer = slotSizePer * blockSize;  // Number of items per node
long nodeCount = (itemCount + nodeSizePer - 1)/nodeSizePer;	


/* Calculate sizes and offsets. */
long bytesInIndexBlock = (bptBlockHeaderSize + blockSize * (keySize+sizeof(bits64)));
long bytesInLeafBlock = (bptBlockHeaderSize + blockSize * (keySize+valSize));
bits64 bytesInNextLevelBlock = (level == 1 ? bytesInLeafBlock : bytesInIndexBlock);
bits64 levelSize = nodeCount * bytesInIndexBlock;
bits64 endLevel = indexOffset + levelSize;
bits64 nextChild = endLevel;


UBYTE isLeaf = FALSE;
UBYTE reserved = 0;

long i,j;
char keyBuf[keySize+1];
keyBuf[keySize] = 0;
for (i=0; i<itemCount; i += nodeSizePer)
    {
    /* Calculate size of this block */
    long countOne = (itemCount - i + slotSizePer - 1)/slotSizePer;
    if (countOne > blockSize)
        countOne = blockSize;
    bits16 shortCountOne = countOne;

    /* Write block header. */
    writeOne(f, isLeaf);
    writeOne(f, reserved);
    writeOne(f, shortCountOne);

    /* Write out the slots that are used one by one, and do sanity check. */
    int slotsUsed = 0;
    long endIx = i + nodeSizePer;
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
    assert(slotsUsed == shortCountOne);

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

void bptFileBulkIndexToOpenFile(void *itemArray, int itemSize, bits64 itemCount, bits32 blockSize,
	void (*fetchKey)(const void *va, char *keyBuf), bits32 keySize,
	void* (*fetchVal)(const void *va), bits32 valSize, FILE *f)
/* Create a b+ tree index from a sorted array, writing output starting at current position
 * of an already open file.  See bptFileCreate for explanation of parameters. */
{
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

void bptFileCreate(
	void *itemArray, 	/* Sorted array of things to index. */
	int itemSize, 		/* Size of each element in array. */
	bits64 itemCount, 	/* Number of elements in array. */
	bits32 blockSize,	/* B+ tree block size - # of children for each node. */
	void (*fetchKey)(const void *va, char *keyBuf),  /* Given item, copy key to keyBuf */ 
	bits32 keySize,					 /* Size of key */
	void* (*fetchVal)(const void *va), 		 /* Given item, return pointer to value */
	bits32 valSize, 				 /* Size of value */
	char *fileName)                                  /* Name of output file. */
/* Create a b+ tree index file from a sorted array. */

{
/* Open file and write header. */
FILE *f = mustOpen(fileName, "wb");
bptFileBulkIndexToOpenFile(itemArray, itemSize, itemCount, blockSize, fetchKey, keySize, 
	fetchVal, valSize, f);
carefulClose(&f);
}

