/* rt1dTest - Test out some stuff for one dimensional r trees.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "localmem.h"

static char const rcsid[] = "$Id: rt1dTest.c,v 1.3 2009/01/16 22:19:46 kent Exp $";

int blockSize = 64;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rt1dTest - Test out some stuff for one dimensional r trees.\n"
  "usage to create a rtree file:\n"
  "   rt1dTest create in.tab out.rtree\n"
  "to search a file\n"
  "   rt1dTest find in.tab in.rtree chrom start end\n"
  "The in.tab file is tab-separated in the format\n"
  "   <seqId> <start> <end> <name>\n"
  "The index created will associate ranges with positions in the input file\n"
  "options:\n"
  "   -blockSize=N - number of children per node in b+ tree. Default %d\n"
  , blockSize
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {NULL, 0},
};

#define crTreeSig 0x2468ACE0

struct crTreeRange
/* A chromosome id and an interval inside it. */
    {
    bits32 chromIx;	/* Chromosome id. */
    bits32 start;	/* Start position in chromosome. */
    bits32 end;		/* One past last base in interval in chromosome. */
    };


#define CHROMRANGE_NUM_COLS 3

struct chromRange
/* A chromosome and an interval inside it. */
    {
    struct chromRange *next;  /* Next in singly linked list. */
    bits32 chromIx;	/* Index into chromosome table. */
    bits32 start;	/* Start position in chromosome. */
    bits32 end;	/* One past last base in interval in chromosome. */
    bits64 fileOffset;	/* Offset of item in file we are indexing. */
    bits64 fileSize;	/* Size in file. */
    };

struct chromRange *chromRangeLoadAll(char *fileName, bits64 *retFileSize) 
/* Load all chromRange from a whitespace-separated file.
 * Dispose of this with chromRangeFreeList(). */
{
struct chromRange *list = NULL, *el;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];

while (lineFileRow(lf, row))
    {
    AllocVar(el);
    el->chromIx = sqlUnsigned(row[0]);
    el->start = sqlUnsigned(row[1]);
    el->end = sqlUnsigned(row[2]);
    el->fileOffset = lineFileTell(lf);
    slAddHead(&list, el);
    }
slReverse(&list);
struct chromRange *next;
for (el = list; el != NULL; el = next)
    {
    next = el->next;
    if (next == NULL)
	{
	bits64 fileSize = *retFileSize = lineFileTell(lf);
        el->fileSize = fileSize - el->fileOffset;
	}
    else
        el->fileSize = next->fileOffset - el->fileOffset;
    }
lineFileClose(&lf);
return list;
}

int chromRangeCmp(const void *va, const void *vb)
/* Compare to sort based on chromIx,start,-end. */
{
const struct chromRange *a = *((struct chromRange **)va);
const struct chromRange *b = *((struct chromRange **)vb);
int dif;
dif = a->chromIx - b->chromIx;
if (dif == 0)
    dif = a->start - b->start;
if (dif == 0)
    dif = b->end - a->end;
return dif;
}

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
#define nodeHeaderSize (4)

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
uglyf("rWriteIndexLevel blockSize=%d, childNodeSize=%d, offsetOfFirstChild=%llu, curLevel=%d, destLevel=%d slCount(tree)=%d\n", blockSize, childNodeSize, offsetOfFirstChild, curLevel, destLevel, slCount(tree->children));
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

static void rWriteLeaves(int blockSize, int lNodeSize, struct rTree *tree, int curLevel,
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
    for (i=countOne; i<blockSize; ++i)
	repeatCharOut(f, 0, indexSlotSize);
    }
else
    {
    /* Otherwise recurse on children. */
    struct rTree *el;
    for (el = tree->children; el != NULL; el = el->next)
	rWriteLeaves(blockSize, lNodeSize, el, curLevel+1, leafLevel, f);
    }
}

static void writeLeaves(int blockSize, int lNodeSize, struct rTree *tree, int leafLevel, FILE *f)
/* Write out leaf-level nodes. */
{
rWriteLeaves(blockSize, lNodeSize, tree, 0, leafLevel, f);
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

struct rTree *rTreeFromChromRangeArray( struct lm *lm, int blockSize,
	void *itemArray, int itemSize, bits64 itemCount, 
	struct crTreeRange (*fetchKey)(const void *va),
	bits64 (*fetchOffset)(const void *va), bits64 (*fetchSize)(const void *va),
	int *retLevelCount)
{
char *items = itemArray;
struct rTree *el, *list=NULL, *tree = NULL;

/* Make first level above leaf. */
bits64 i;
for (i=0; i<itemCount; i += blockSize)
    {
    int oneSize = itemCount-i;
    if (oneSize > blockSize)
        oneSize = blockSize;
    lmAllocVar(lm, el);
    slAddHead(&list, el);

    /* Figure out area spanned. */
    char *startItem = items + itemSize * i;
    char *endItem = startItem + itemSize*(oneSize-1);
    struct crTreeRange key = (*fetchKey)(startItem);
    el->startChromIx = el->endChromIx = key.chromIx;
    el->startBase = key.start;
    el->endBase = key.end;
    el->startFileOffset = (*fetchOffset)(startItem);
    el->endFileOffset = (*fetchOffset)(endItem) + (*fetchSize)(endItem);
    int j;
    for (j=1; j<oneSize; ++j)
        {
	void *item = items + itemSize*(i+j);
	key = (*fetchKey)(item);
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
verbose(1, "Made %d primary index nodes out of %llu items\n", slCount(list), itemCount);

/* Now iterate through making more and more condensed versions until have just one. */
int levelCount = 1;
tree = list;
while (tree->next != NULL)
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

static void writeTreeToOpenFile(struct rTree *tree, int levelCount, FILE *f)
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
bits64 offset = fileHeaderSize;
bits64 iNodeSize = indexNodeSize(blockSize);
bits64 lNodeSize = leafNodeSize(blockSize);
for (i=0; i<levelCount; ++i)
    {
    levelOffsets[i] = offset;
    offset += levelSizes[i] * iNodeSize;
    uglyf("level %d: size %d, offset %llu\n", i, levelSizes[i], levelOffsets[i]);
    }

uglyf("%d levels.  Level sizes are", levelCount);
for (i=0; i<levelCount; ++i) uglyf(" %d", levelSizes[i]);
uglyf("\n");

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
writeLeaves(blockSize, leafNodeSize(blockSize), tree, levelCount-2, f);
}

void crTreeFileBulkIndexToOpenFile(
	void *itemArray, int itemSize, bits64 itemCount, 
	bits32 blockSize,
	struct crTreeRange (*fetchKey)(const void *va),
	bits64 (*fetchOffset)(const void *va), bits64 (*fetchSize)(const void *va), 
	bits64 totalFileSize, FILE *f)
/* Create a b+ tree index from a sorted array, writing output starting at current position
 * of an already open file.  See rTreeFileCreate for explanation of parameters. */
{
int levelCount;
struct lm *lm = lmInit(0);
struct rTree *tree = rTreeFromChromRangeArray(lm, blockSize, 
	itemArray, itemSize, itemCount, fetchKey, fetchOffset, fetchSize, &levelCount);
bits32 magic = crTreeSig;
bits32 reserved = 0;
writeOne(f, magic);
writeOne(f, blockSize);
writeOne(f, itemCount);
writeOne(f, tree->startChromIx);
writeOne(f, tree->startBase);
writeOne(f, tree->endChromIx);
writeOne(f, tree->endBase);
writeOne(f, totalFileSize);
writeOne(f, reserved);
writeOne(f, reserved);
writeTreeToOpenFile(tree, levelCount, f);
lmCleanup(&lm);
}

void crTreeFileCreate(
	void *itemArray, 	/* Sorted array of things to index. */
	int itemSize, 		/* Size of each element in array. */
	bits64 itemCount, 	/* Number of elements in array. */
	bits32 blockSize,	/* R tree block size - # of children for each node. */
	struct crTreeRange (*fetchKey)(const void *va),   /* Givenitem, return key. */
	bits64 (*fetchOffset)(const void *va), 		 /* Given item, return file offset */
	bits64 (*fetchSize)(const void *va), 		 /* Given item, return size in file */
	bits64 totalFileSize,				 /* Total size of file we are indexing. */
	char *fileName)                                  /* Name of output file. */
/* Create a r tree index file from a sorted array. */
{
/* Open file and write header. */
FILE *f = mustOpen(fileName, "wb");
crTreeFileBulkIndexToOpenFile(itemArray, itemSize, itemCount, blockSize, fetchKey, 
	fetchOffset, fetchSize, totalFileSize, f);
carefulClose(&f);
}

struct crTreeFile
/* R tree index file handle. */
    {
    struct crTreeFile *next;	/* Next in list of index files if any. */
    char *fileName;		/* Name of file - for error reporting. */
    FILE *f;			/* Open file pointer. */
    boolean isSwapped;		/* If TRUE need to byte swap everything. */
    bits64 rootOffset;		/* Offset of root block. */
    bits32 blockSize;		/* Size of block. */
    bits64 itemCount;		/* Number of items indexed. */
    bits32 startChromIx;	/* First chromosome in file. */
    bits32 startBase;		/* Starting base position. */
    bits32 endChromIx;		/* Ending chromosome in file. */
    bits32 endBase;		/* Ending base position. */
    bits64 fileSize;		/* Total size of index file. */
    };

struct crTreeFile *crTreeFileOpen(char *fileName)
/* Open up r-tree index file - reading header and verifying things. */
{
/* Open file and allocate structure to hold info from header etc. */
struct crTreeFile *crt = needMem(sizeof(*crt));
crt->fileName = cloneString(fileName);
FILE *f = mustOpen(fileName, "rb");
crt->f = f;

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = FALSE;
mustReadOne(f, magic);
if (magic != crTreeSig)
    {
    magic = byteSwap32(magic);
    isSwapped = crt->isSwapped = TRUE;
    if (magic != crTreeSig)
       errAbort("%s is not a crt b-plus tree index file", fileName);
    }

/* Read rest of defined bits of header, byte swapping as needed. */
crt->blockSize = readBits32(f, isSwapped);
crt->itemCount = readBits64(f, isSwapped);
crt->startChromIx = readBits32(f, isSwapped);
crt->startBase = readBits32(f, isSwapped);
crt->endChromIx = readBits32(f, isSwapped);
crt->endBase = readBits32(f, isSwapped);
crt->fileSize = readBits64(f, isSwapped);

/* Skip over reserved bits of header. */
bits32 reserved32;
mustReadOne(f, reserved32);
mustReadOne(f, reserved32);

/* Save position of root block of r tree. */
crt->rootOffset = ftell(f);

return crt;
}

struct crTreeBlock
/* A block in a file. */
   {
   struct crTreeBlock *next;	/* Next in list. */
   bits64	offset;		/* Start offset of block. */
   bits64	size;		/* Size of block. */
   };

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

static inline boolean crTreeOverlaps(int qChrom, int qStart, int qEnd, 
	int rStartChrom, int rStartBase, int rEndChrom, int rEndBase)
{
return cmpTwoBits32(qChrom, qStart, rEndChrom, rEndBase) > 0 &&
       cmpTwoBits32(qChrom, qEnd, rStartChrom, rStartBase) < 0;
}

static void rFindOverlappingBlocks(struct crTreeFile *crf, bits64 indexFileOffset,
	bits32 chromIx, bits32 start, bits32 end, struct crTreeBlock **retList)
/* Recursively find blocks with data. */
{
FILE *f = crf->f;

/* Seek to start of block. */
fseek(f, indexFileOffset, SEEK_SET);

/* Read block header. */
UBYTE isLeaf;
UBYTE reserved;
bits16 i, childCount;
mustReadOne(f, isLeaf);
mustReadOne(f, reserved);
boolean isSwapped = crf->isSwapped;
childCount = readBits16(f, isSwapped);

uglyf("rFindOverlappingBlocks %llu %u:%u-%u.  childCount %d. isLeaf %d\n", indexFileOffset, chromIx, start, end, (int)childCount, (int)isLeaf);

if (isLeaf)
    {
    /* Loop through node adding overlapping leaves to block list. */
    for (i=0; i<childCount; ++i)
        {
	bits32 startChromIx = readBits32(f, isSwapped);
	bits32 startBase = readBits32(f, isSwapped);
	bits32 endChromIx = readBits32(f, isSwapped);
	bits32 endBase = readBits32(f, isSwapped);
	bits64 offset = readBits64(f, isSwapped);
	bits64 size = readBits64(f, isSwapped);
	if (crTreeOverlaps(chromIx, start, end, startChromIx, startBase, endChromIx, endBase))
	    {
	    struct crTreeBlock *block;
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
	startChromIx[i] = readBits32(f, isSwapped);
	startBase[i] = readBits32(f, isSwapped);
	endChromIx[i] = readBits32(f, isSwapped);
	endBase[i] = readBits32(f, isSwapped);
	offset[i] = readBits64(f, isSwapped);
	}

    /* Recurse into child nodes that we overlap. */
    for (i=0; i<childCount; ++i)
	{
	if (crTreeOverlaps(chromIx, start, end, startChromIx[i], startBase[i], 
		endChromIx[i], endBase[i]))
	    {
	    rFindOverlappingBlocks(crf, offset[i], chromIx, start, end, retList);
	    }
	}
    }
}

struct crTreeBlock *crTreeFindOverlappingBlocks(struct crTreeFile *crf, 
	bits32 chromIx, bits32 start, bits32 end)
/* Return list of file blocks that between them contain all items that overlap
 * start/end on chromIx.  Also there will be likely some non-overlapping items
 * in these blocks too. When done, use slListFree to dispose of the result. */
{
struct crTreeBlock *blockList = NULL;

/* Deal with case where there are not enough items to even build an index. */
if (crf->itemCount <= crf->blockSize)
    {
    if (crf->itemCount != 0)
        {
	if (crTreeOverlaps(chromIx, start, end, 
		crf->startChromIx, crf->startBase, crf->endChromIx, crf->endBase)) 
	    {
	    AllocVar(blockList);
	    blockList->offset = 0;
	    blockList->size = crf->fileSize;
	    }
	}
    }
else
    {
    rFindOverlappingBlocks(crf, crf->rootOffset, chromIx, start, end, &blockList);
    }

slReverse(&blockList);
return blockList;
}

struct crTreeRange chromRangeKey(const void *va)
/* Get key fields. */
{
const struct chromRange *a = *((struct chromRange **)va);
struct crTreeRange ret;
ret.chromIx = a->chromIx;
ret.start = a->start;
ret.end = a->end;
return ret;
}

bits64 chromRangeOffset(const void *va)
{
const struct chromRange *a = *((struct chromRange **)va);
return a->fileOffset;
}

bits64 chromRangeSize(const void *va)
{
const struct chromRange *a = *((struct chromRange **)va);
return a->fileSize;
}

void rt1dCreate(char *inBed, char *outTree)
/* rt1dCreate - create a one dimensional range tree. */
{
bits64 fileSize;
struct chromRange **array, *el, *list = chromRangeLoadAll(inBed, &fileSize);
int count = slCount(list);
verbose(1, "read %d from %s\n", count, inBed);

/* Make array of pointers out of linked list. */
AllocArray(array, count);
int i;
for (i=0, el=list; i<count; ++i, el=el->next)
    array[i] = el;

#ifdef NEEDS_TO_BE_SORTED_ALREADY_IN_FILE
/* Sort array */
qsort(array, count, sizeof(array[0]), chromRangeCmp);
#endif /* NEEDS_TO_BE_SORTED_ALREADY_IN_FILE */
/* Sort array */

crTreeFileCreate(array, sizeof(array[0]), count, blockSize, 
	chromRangeKey, chromRangeOffset, chromRangeSize, fileSize, outTree);
}

void rt1dFind(char *tabFile, char *treeFile, bits32 chromIx, bits32 start, bits32 end)
/* rt1dCreate - find items in 1-D range tree. */
{
struct lineFile *lf = lineFileOpen(tabFile, TRUE);
struct crTreeFile *crf = crTreeFileOpen(treeFile);
struct crTreeBlock *block, *blockList = crTreeFindOverlappingBlocks(crf, chromIx, start, end);
uglyf("Got %d overlapping blocks\n", slCount(blockList));
for (block = blockList; block != NULL; block = block->next)
    {
    uglyf("block->offset %llu, block->size %llu\n", block->offset, block->size);
    lineFileSeek(lf, block->offset, SEEK_SET);
    bits64 sizeUsed = 0;
    while (sizeUsed < block->size)
        {
	char *line;
	int size;
	if (!lineFileNext(lf, &line, &size))
	    errAbort("Couldn't read %s\n", lf->fileName);
	char *parsedLine = cloneString(line);
	char *row[3];
	if (chopLine(parsedLine, row) != ArraySize(row))
	    errAbort("Badly formatted line of %s\n%s", lf->fileName, line);
	bits32 bedChromIx = sqlUnsigned(row[0]);
	bits32 bedStart = sqlUnsigned(row[1]);
	bits32 bedEnd = sqlUnsigned(row[2]);
	if (bedChromIx == chromIx && rangeIntersection(bedStart, bedEnd, start, end) > 0)
	    fprintf(stdout, "%s\n", line);
	freeMem(parsedLine);
	sizeUsed += size;
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
blockSize = optionInt("blockSize", blockSize);
if (argc < 2)
    usage();
char *command = argv[1];
if (sameWord(command, "create"))
    {
    if (argc != 4)
        usage();
    rt1dCreate(argv[2], argv[3]);
    }
else if (sameWord(command, "find"))
    {
    if (argc != 7)
        usage();
    rt1dFind(argv[2], argv[3], sqlUnsigned(argv[4]), sqlUnsigned(argv[5]), sqlUnsigned(argv[6]));
    }
else
    usage();
return 0;
}
