/* cirTree chromosome id r tree.  Part of a system to index chromosome ranges - things of
 * form chrN:start-end.  Generally you'll be using the crTree module - which
 * makes use of this module and the bPlusTree module - rather than this module directly.
 * This module works with chromosomes mapped to small integers rather than chromosomes
 * as strings, saving space and speeding things up in the process, but requiring the
 * separate bPlusTree to map the names to IDs. 
 *   This module implements a one dimensional R-tree index treating the chromosome ID
 * as the most significant part of a two-part key, and the base position as the least 
 * significant part of the key. */

#include "common.h"
#include "localmem.h"
#include "sig.h"
#include "udc.h"
#include "cirTree.h"

struct rTree
/* Recursive range structure. */
    {
    struct rTree *next;	/* Next on same level. */
    struct rTree *children;	/* Child list. */
    struct rTree *parent;	/* Our parent if any. */
    bits32 startChromIx;	/* Starting chromosome. */
    bits32 startBase;		/* Starting base position. */
    bits32 endChromIx;		/* Ending chromosome. */
    bits32 endBase;		/* Ending base. */
    bits64 startFileOffset;	/* Start offset in file for leaves. */
    bits64 endFileOffset;	/* End file offset for leaves. */
    };

#define fileHeaderSize (48)	/* Size of file header. */
#define indexSlotSize (24)	/* Size of startChrom,startBase,endChrom,endBase,offset */
#define leafSlotSize (32)       /* Size of startChrom,startBase,endChrom,endBase,offset,size */
#define nodeHeaderSize (4)	/* Size of rTree node header. isLeaf,reserved,childCount. */

int indexNodeSize(int blockSize)
/* Return size of an index node. */
{
return nodeHeaderSize + indexSlotSize * blockSize;
}

int leafNodeSize(int blockSize)
/* Return size of a leaf node. */
{
return nodeHeaderSize + leafSlotSize * blockSize;
}


static bits64 rWriteIndexLevel(bits16 blockSize, int childNodeSize,
	struct rTree *tree, int curLevel, int destLevel,
	bits64 offsetOfFirstChild, FILE *f)
/* Recursively write an index level, skipping levels below destLevel,
 * writing out destLevel. */
{
struct rTree *el;
bits64 offset = offsetOfFirstChild;
if (curLevel == destLevel)
    {
    /* We've reached the right level, write out a node header */
    UBYTE reserved = 0;
    UBYTE isLeaf = FALSE;
    bits16 countOne = slCount(tree->children);
    writeOne(f, isLeaf);
    writeOne(f, reserved);
    writeOne(f, countOne);

    /* Write out elements of this node. */
    for (el = tree->children; el != NULL; el = el->next)
	{
	writeOne(f, el->startChromIx);
	writeOne(f, el->startBase);
	writeOne(f, el->endChromIx);
	writeOne(f, el->endBase);
	writeOne(f, offset);
	offset += childNodeSize;
	}

    /* Write out zeroes for empty slots in node. */
    int i;
    for (i=countOne; i<blockSize; ++i)
	repeatCharOut(f, 0, indexSlotSize);
    }
else 
    {
    /* Otherwise recurse on children. */
    for (el = tree->children; el != NULL; el = el->next)
	offset = rWriteIndexLevel(blockSize, childNodeSize, el, curLevel+1, destLevel,
		offset, f);
    }
return offset;
}

static void writeIndexLevel(int blockSize, int childNodeSize, 
	struct rTree *tree, bits64 offsetOfFirstChild, int level, FILE *f)
/* Write out a non-leaf level nodes at given level. */
{
rWriteIndexLevel(blockSize, childNodeSize, tree, 0, level, offsetOfFirstChild, f);
}

static void rWriteLeaves(int itemsPerSlot, int lNodeSize, struct rTree *tree, int curLevel,
	int leafLevel, FILE *f)
/* Write out leaf-level nodes. */
{
if (curLevel == leafLevel)
    {
    /* We've reached the right level, write out a node header. */
    UBYTE reserved = 0;
    UBYTE isLeaf = TRUE;
    bits16 countOne = slCount(tree->children);
    writeOne(f, isLeaf);
    writeOne(f, reserved);
    writeOne(f, countOne);

    /* Write out elements of this node. */
    struct rTree *el;
    for (el = tree->children; el != NULL; el = el->next)
	{
	writeOne(f, el->startChromIx);
	writeOne(f, el->startBase);
	writeOne(f, el->endChromIx);
	writeOne(f, el->endBase);
	writeOne(f, el->startFileOffset);
	bits64 size = el->endFileOffset - el->startFileOffset;
	writeOne(f, size);
	}

    /* Write out zeroes for empty slots in node. */
    int i;
    for (i=countOne; i<itemsPerSlot; ++i)
	repeatCharOut(f, 0, indexSlotSize);
    }
else
    {
    /* Otherwise recurse on children. */
    struct rTree *el;
    for (el = tree->children; el != NULL; el = el->next)
	rWriteLeaves(itemsPerSlot, lNodeSize, el, curLevel+1, leafLevel, f);
    }
}

static void writeLeaves(int itemsPerSlot, int lNodeSize, struct rTree *tree, int leafLevel, FILE *f)
/* Write out leaf-level nodes. */
{
rWriteLeaves(itemsPerSlot, lNodeSize, tree, 0, leafLevel, f);
}

void calcLevelSizes(struct rTree *tree, int *levelSizes, int level, int maxLevel)
/* Recursively count sizes of levels and add to appropriate slots of levelSizes */
{
struct rTree *el;
for (el = tree; el != NULL; el = el->next)
    {
    levelSizes[level] += 1;
    if (level < maxLevel)
        calcLevelSizes(el->children, levelSizes, level+1, maxLevel);
    }
}

static struct rTree *rTreeFromChromRangeArray( struct lm *lm, int blockSize, int itemsPerSlot,
	void *itemArray, int itemSize, bits64 itemCount,  void *context,
	struct cirTreeRange (*fetchKey)(const void *va, void *context),
	bits64 (*fetchOffset)(const void *va, void *context), bits64 endFileOffset,
	int *retLevelCount)
{
char *items = itemArray;
struct rTree *el, *list=NULL, *tree = NULL;

/* Make first level above leaf. */
bits64 i;
bits64 nextOffset = (*fetchOffset)(items, context);
for (i=0; i<itemCount; i += itemsPerSlot)
    {
    /* Figure out if we are on final iteration through loop, and the
     * count of items in this iteration. */
    boolean finalIteration = FALSE;
    int oneSize = itemCount-i;
    if (oneSize > itemsPerSlot)
        oneSize = itemsPerSlot;
    else
        finalIteration = TRUE;

    /* Allocate element and put on list. */
    lmAllocVar(lm, el);
    slAddHead(&list, el);

    /* Fill out most of element from first item in element. */
    char *startItem = items + itemSize * i;
    struct cirTreeRange key = (*fetchKey)(startItem, context);
    el->startChromIx = el->endChromIx = key.chromIx;
    el->startBase = key.start;
    el->endBase = key.end;
    el->startFileOffset = nextOffset;

    /* Figure out end of element from offset of next element (or file size
     * for final element.) */
    if (finalIteration)
	nextOffset = endFileOffset;
    else
        {
	char *endItem = startItem + itemSize*oneSize;
        nextOffset = (*fetchOffset)(endItem, context);
	}
    el->endFileOffset = nextOffset;

    /* Expand area spanned to include all items in block. */
    int j;
    for (j=1; j<oneSize; ++j)
        {
	void *item = items + itemSize*(i+j);
	key = (*fetchKey)(item, context);
	if (key.chromIx < el->startChromIx)
	    {
	    el->startChromIx = key.chromIx;
	    el->startBase = key.start;
	    }
	else if (key.chromIx == el->startChromIx)
	    {
	    if (key.start < el->startBase)
	        el->startBase = key.start;
	    }
	if (key.chromIx > el->endChromIx)
	    {
	    el->endChromIx = key.chromIx;
	    el->endBase = key.end;
	    }
	else if (key.chromIx == el->endChromIx)
	    {
	    if (key.end > el->endBase)
	        el->endBase = key.end;
	    }
	}
    }
slReverse(&list);
verbose(2, "Made %d primary index nodes out of %llu items\n", slCount(list), itemCount);

/* Now iterate through making more and more condensed versions until have just one. */
int levelCount = 1;
tree = list;
while (tree->next != NULL || levelCount < 2)
    {
    list = NULL;
    int slotsUsed = blockSize;
    struct rTree *parent = NULL, *next;
    for (el = tree; el != NULL; el = next)
        {
	next = el->next;
	if (slotsUsed >= blockSize)
	    {
	    slotsUsed = 1;
	    lmAllocVar(lm, parent);
	    parent = lmCloneMem(lm, el, sizeof(*el));
	    parent->children = el;
	    el->parent = parent;
	    el->next = NULL;
	    slAddHead(&list, parent);
	    }
	else
	    {
	    ++slotsUsed;
	    slAddHead(&parent->children, el);
	    el->parent = parent;
	    if (el->startChromIx < parent->startChromIx)
		{
	        parent->startChromIx = el->startChromIx;
		parent->startBase = el->startBase;
		}
	    else if (el->startChromIx == parent->startChromIx)
	        {
		if (el->startBase < parent->startBase)
		    parent->startBase = el->startBase;
		}
	    if (el->endChromIx > parent->endChromIx)
		{
	        parent->endChromIx = el->endChromIx;
		parent->endBase = el->endBase;
		}
	    else if (el->endChromIx == parent->endChromIx)
	        {
		if (el->endBase > parent->endBase)
		    parent->endBase = el->endBase;
		}
	    }
	}

    slReverse(&list);
    for (el = list; el != NULL; el = el->next)
        slReverse(&el->children);
    tree = list;
    levelCount += 1;
    }
*retLevelCount = levelCount;
return tree;
}

static void writeTreeToOpenFile(struct rTree *tree, int blockSize, int levelCount, FILE *f)
/* Write out tree to a file that is open already - writing out index nodes from 
 * highest to lowest level, and then leaf nodes. */
{
/* Calculate sizes of each level. */
int i;
int levelSizes[levelCount];
for (i=0; i<levelCount; ++i)
    levelSizes[i] = 0;
calcLevelSizes(tree, levelSizes, 0, levelCount-1);

/* Calc offsets of each level. */
bits64 levelOffsets[levelCount];
bits64 offset = ftell(f);
bits64 iNodeSize = indexNodeSize(blockSize);
bits64 lNodeSize = leafNodeSize(blockSize);
for (i=0; i<levelCount; ++i)
    {
    levelOffsets[i] = offset;
    offset += levelSizes[i] * iNodeSize;
    verbose(2, "level %d: size %d, offset %llu\n", i, levelSizes[i], levelOffsets[i]);
    }

verbose(2, "%d levels.  Level sizes are", levelCount);
for (i=0; i<levelCount; ++i) verbose(2, " %d", levelSizes[i]);
verbose(2, "\n");

/* Write out index levels. */
int finalLevel = levelCount-3;
for (i=0; i<=finalLevel; ++i)
    {
    bits64 childNodeSize = (i==finalLevel ? lNodeSize : iNodeSize);
    writeIndexLevel(blockSize, childNodeSize, tree,
    	levelOffsets[i+1], i, f);
    if (ftell(f) != levelOffsets[i+1])
        errAbort("Internal error: offset mismatch (%llu vs %llu) line %d of %s\n", (bits64)ftell(f), levelOffsets[i+1], __LINE__, __FILE__);
    }

/* Write out leaf level. */
int leafLevel = levelCount - 2;
writeLeaves(blockSize, leafNodeSize(blockSize), tree, leafLevel, f);
}

void cirTreeFileBulkIndexToOpenFile(
	void *itemArray, int itemSize, bits64 itemCount, 
	bits32 blockSize, bits32 itemsPerSlot,
	void *context,
	struct cirTreeRange (*fetchKey)(const void *va, void *context),
	bits64 (*fetchOffset)(const void *va, void *context), 
	bits64 endFileOffset, FILE *f)
/* Create a r tree index from a sorted array, writing output starting at current position
 * of an already open file.  See rTreeFileCreate for explanation of parameters. */
{
int levelCount;
struct lm *lm = lmInit(0);
struct rTree *tree = rTreeFromChromRangeArray(lm, blockSize, itemsPerSlot,
	itemArray, itemSize, itemCount, context, fetchKey, fetchOffset, endFileOffset,
	&levelCount);
bits32 magic = cirTreeSig;
bits32 reserved = 0;
writeOne(f, magic);
writeOne(f, blockSize);
writeOne(f, itemCount);
writeOne(f, tree->startChromIx);
writeOne(f, tree->startBase);
writeOne(f, tree->endChromIx);
writeOne(f, tree->endBase);
writeOne(f, endFileOffset);
writeOne(f, itemsPerSlot);
writeOne(f, reserved);
writeTreeToOpenFile(tree, blockSize, levelCount, f);
lmCleanup(&lm);
}

void cirTreeFileCreate(
	void *itemArray, 	/* Sorted array of things to index. */
	int itemSize, 		/* Size of each element in array. */
	bits64 itemCount, 	/* Number of elements in array. */
	bits32 blockSize,	/* R tree block size - # of children for each node. */
	bits32 itemsPerSlot,	/* Number of items to put in each index slot at lowest level. */
	void *context,		/* Context pointer for use by fetch call-back functions. */
	struct cirTreeRange (*fetchKey)(const void *va, void *context),/* Given item, return key. */
	bits64 (*fetchOffset)(const void *va, void *context), /* Given item, return file offset */
	bits64 endFileOffset,				 /* Last position in file we index. */
	char *fileName)                                  /* Name of output file. */
/* Create a r tree index file from a sorted array. */
{
FILE *f = mustOpen(fileName, "wb");
cirTreeFileBulkIndexToOpenFile(itemArray, itemSize, itemCount, blockSize, itemsPerSlot,
	context, fetchKey, fetchOffset, endFileOffset, f);
carefulClose(&f);
}

struct cirTreeFile *cirTreeFileAttach(char *fileName, struct udcFile *udc)
/* Open up r-tree index file on previously open file, with cirTree
 * header at current file position. */
{
/* Open file and allocate structure to hold info from header etc. */
struct cirTreeFile *crt = needMem(sizeof(*crt));
crt->fileName = fileName;
crt->udc = udc;

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = FALSE;
udcMustReadOne(udc, magic);
if (magic != cirTreeSig)
    {
    magic = byteSwap32(magic);
    isSwapped = crt->isSwapped = TRUE;
    if (magic != cirTreeSig)
       errAbort("%s is not a chromosome id r-tree index file", fileName);
    }

/* Read rest of defined bits of header, byte swapping as needed. */
crt->blockSize = udcReadBits32(udc, isSwapped);
crt->itemCount = udcReadBits64(udc, isSwapped);
crt->startChromIx = udcReadBits32(udc, isSwapped);
crt->startBase = udcReadBits32(udc, isSwapped);
crt->endChromIx = udcReadBits32(udc, isSwapped);
crt->endBase = udcReadBits32(udc, isSwapped);
crt->fileSize = udcReadBits64(udc, isSwapped);
crt->itemsPerSlot = udcReadBits32(udc, isSwapped);

/* Skip over reserved bits of header. */
bits32 reserved32;
udcMustReadOne(udc, reserved32);

/* Save position of root block of r tree. */
crt->rootOffset = udcTell(udc);

return crt;
}

struct cirTreeFile *cirTreeFileOpen(char *fileName)
/* Open up r-tree index file - reading header and verifying things. */
{
return cirTreeFileAttach(cloneString(fileName), udcFileOpen(fileName, udcDefaultDir()));
}

void cirTreeFileDetach(struct cirTreeFile **pCrt)
/* Detatch and free up cirTree file opened with cirTreeFileAttach. */
{
freez(pCrt);
}

void cirTreeFileClose(struct cirTreeFile **pCrt)
/* Close and free up cirTree file opened with cirTreeFileAttach. */
{
struct cirTreeFile *crt = *pCrt;
if (crt != NULL)
    {
    freez(&crt->fileName);
    udcFileClose(&crt->udc);
    cirTreeFileDetach(pCrt);
    }
}

inline int cmpTwoBits32(bits32 aHi, bits32 aLo, bits32 bHi, bits32 bLo)
/* Return - if b is less than a , 0 if equal, else +*/
{
if (aHi < bHi)
    return 1;
else if (aHi > bHi)
    return -1;
else
    {
    if (aLo < bLo)
       return 1;
    else if (aLo > bLo)
       return -1;
    else
       return 0;
    }
}

static inline boolean cirTreeOverlaps(int qChrom, int qStart, int qEnd, 
	int rStartChrom, int rStartBase, int rEndChrom, int rEndBase)
{
return cmpTwoBits32(qChrom, qStart, rEndChrom, rEndBase) > 0 &&
       cmpTwoBits32(qChrom, qEnd, rStartChrom, rStartBase) < 0;
}

static void rFindOverlappingBlocks(struct cirTreeFile *crt, int level, bits64 indexFileOffset,
	bits32 chromIx, bits32 start, bits32 end, struct fileOffsetSize **retList)
/* Recursively find blocks with data. */
{
struct udcFile *udc = crt->udc;

/* Seek to start of block. */
udcSeek(udc, indexFileOffset);

/* Read block header. */
UBYTE isLeaf;
UBYTE reserved;
bits16 i, childCount;
udcMustReadOne(udc, isLeaf);
udcMustReadOne(udc, reserved);
boolean isSwapped = crt->isSwapped;
childCount = udcReadBits16(udc, isSwapped);

verbose(3, "rFindOverlappingBlocks %llu %u:%u-%u.  childCount %d. isLeaf %d\n", indexFileOffset, chromIx, start, end, (int)childCount, (int)isLeaf);

if (isLeaf)
    {
    /* Loop through node adding overlapping leaves to block list. */
    for (i=0; i<childCount; ++i)
        {
	bits32 startChromIx = udcReadBits32(udc, isSwapped);
	bits32 startBase = udcReadBits32(udc, isSwapped);
	bits32 endChromIx = udcReadBits32(udc, isSwapped);
	bits32 endBase = udcReadBits32(udc, isSwapped);
	bits64 offset = udcReadBits64(udc, isSwapped);
	bits64 size = udcReadBits64(udc, isSwapped);
	if (cirTreeOverlaps(chromIx, start, end, startChromIx, startBase, endChromIx, endBase))
	    {
	    struct fileOffsetSize *block;
	    AllocVar(block);
	    block->offset = offset;
	    block->size = size;
	    slAddHead(retList, block);
	    }
	}
    }
else
    {
    /* Read node into arrays. */
    bits32 startChromIx[childCount], startBase[childCount];
    bits32 endChromIx[childCount], endBase[childCount];
    bits64 offset[childCount];
    for (i=0; i<childCount; ++i)
        {
	startChromIx[i] = udcReadBits32(udc, isSwapped);
	startBase[i] = udcReadBits32(udc, isSwapped);
	endChromIx[i] = udcReadBits32(udc, isSwapped);
	endBase[i] = udcReadBits32(udc, isSwapped);
	offset[i] = udcReadBits64(udc, isSwapped);
	}

    /* Recurse into child nodes that we overlap. */
    for (i=0; i<childCount; ++i)
	{
	if (cirTreeOverlaps(chromIx, start, end, startChromIx[i], startBase[i], 
		endChromIx[i], endBase[i]))
	    {
	    rFindOverlappingBlocks(crt, level+1, offset[i], chromIx, start, end, retList);
	    }
	}
    }
}

struct fileOffsetSize *cirTreeFindOverlappingBlocks(struct cirTreeFile *crt, 
	bits32 chromIx, bits32 start, bits32 end)
/* Return list of file blocks that between them contain all items that overlap
 * start/end on chromIx.  Also there will be likely some non-overlapping items
 * in these blocks too. When done, use slListFree to dispose of the result. */
{
struct fileOffsetSize *blockList = NULL;
rFindOverlappingBlocks(crt, 0, crt->rootOffset, chromIx, start, end, &blockList);
slReverse(&blockList);
return blockList;
}

