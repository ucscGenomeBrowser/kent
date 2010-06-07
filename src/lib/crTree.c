/* crTree chromosome r tree. This module creates and uses a disk-based index that can find items
 * that overlap with a chromosome range - something of the form chrN:start-end - with a
 * minimum of disk access.  It is implemented with a combination of bPlusTrees and r-trees. 
 * The items being indexed can overlap with each other.  
 * 
 * There's two main sides to using this module - creating an index, and using it.
 *
 * The first step of index creation is actually to insure that the file being indexed
 * is ordered by chromosome,start,end.  For a .bed file you can insure this
 * with the command:
 *     sort -k1,1 -k2,2n -k3,3n unsorted.bed > sorted.bed
 * Note that the the chromosome field is sorted alphabetically and the start and end
 * fields are sorted numerically.
 *
 * Once this is done then the index creation program scans the input file, and
 * makes a list of crTreeItems, one for each item in the file, and passed this
 * to the function crTreeFileCreate. A crTreeItem just contains the chromosome
 * range and file offset for an item.
 *
 * Using an index is done in two steps.  First you open the index with crTreeFileOpen,
 * and then you use crTreeFindOverlappingBlocks to find parts of the file the overlap
 * with your query range.  The result of a crTreeFindOverlappingBlocks call is a list
 * of regions in the file.  These regions typically include some non-overlapping items
 * as well.  It is up to the caller to parse through the resulting region list to
 * convert it from just bytes on disk into the memory data structure.  During this
 * parsing you should ignore items that don't overlap your range of interest.
 *
 * The programs crTreeIndexBed and crTreeSearchBed create and search a crTree index
 * for a bed file, and are useful examples to view for other programs that want to
 * use the crTree system. */


#include "common.h"
#include "hash.h"
#include "udc.h"
#include "sig.h"
#include "bPlusTree.h"
#include "cirTree.h"
#include "crTree.h"

#define chromOffsetPos 8	// Where in file chromosome b-tree lives
#define cirOffsetPos 16		// Where in file interval r-tree lives
#define crHeaderSize 64		// Size of file header 

struct name32
/* Pair of a name and a 32-bit integer. Used to assign IDs to chromosomes. */
    {
    struct name32 *next;
    char *name;
    bits32 val;
    };

static int name32Cmp(const void *va, const void *vb)
/* Compare to sort on name. */
{
const struct name32 *a = ((struct name32 *)va);
const struct name32 *b = ((struct name32 *)vb);
return strcmp(a->name, b->name);
}

static void name32Key(const void *va, char *keyBuf)
/* Get key field. */
{
const struct name32 *a = ((struct name32 *)va);
strcpy(keyBuf, a->name);
}

static void *name32Val(const void *va)
/* Get key field. */
{
const struct name32 *a = ((struct name32 *)va);
return (void*)(&a->val);
}

struct crTreeRange
/* A chromosome id and an interval inside it. */
    {
    char *chrom;	/* Chromosome id. String memory owned in hash. */
    bits32 start;	/* Start position in chromosome. */
    bits32 end;		/* One past last base in interval in chromosome. */
    };

struct ciContext 
/* A structure that carries around context for the fetchKey and fetchOffset callbacks. */
    {
    struct crTreeRange (*fetchKey)(const void *va);   /* Given item, return key. */
    bits64 (*fetchOffset)(const void *va); 		 /* Given item, return file offset */
    };

struct ciItem
/* Small wrapper around crItem. Contains the key values precomputed, and the chromosome index
 * for each key. */
     {
     void *item;			/* Underlying cr item. */
     struct crTreeRange key;		/* The key for this item. */
     bits32 chromIx;			/* Associated chromosome index. */
     };

static struct cirTreeRange ciItemFetchKey(const void *va, void *context)
/* Given item, return key. */
{
const struct ciItem *a = ((struct ciItem *)va);
struct cirTreeRange ret;
ret.chromIx = a->chromIx;
ret.start = a->key.start;
ret.end = a->key.end;
return ret;
}

static bits64 ciItemFetchOffset(const void *va, void *context)
/* Given item, return file offset */
{
const struct ciItem *a = ((struct ciItem *)va);
const struct ciContext *c = context;
return (c->fetchOffset)(a->item);
}

static bits32 mustFindChromIx(char *chrom, struct name32 *array, bits32 chromCount)
/* Do a binary search on array to find the index associated with chrom.  Print
 * error message and abort if not found. */
{
struct name32 key;
key.name = chrom;
struct name32 *el = bsearch(&key, array, chromCount, sizeof(array[0]), name32Cmp);
if (el == NULL)
    errAbort("%s not found in mustFindChromIx\n", chrom);
return el->val;
}

static void crTreeFileCreateLow(
	char **chromNames,	/* All chromosome (or contig) names */
	int chromCount,		/* Number of chromosomes. */
	void *itemArray, 	/* Sorted array of things to index. */
	int itemSize, 		/* Size of each element in array. */
	bits64 itemCount, 	/* Number of elements in array. */
	bits32 blockSize,	/* R tree block size - # of children for each node. */
	bits32 itemsPerSlot,	/* Number of items to put in each index slot at lowest level. */
	struct crTreeRange (*fetchKey)(const void *va),   /* Given item, return key. */
	bits64 (*fetchOffset)(const void *va), 		 /* Given item, return file offset */
	bits64 initialDataOffset,			 /* Offset of 1st piece of data in file. */
	bits64 totalDataSize,				 /* Total size of data we are indexing. */
	char *fileName)                                 /* Name of output file. */
/* Create a r tree index file from an array of chromosomes and an array of items with
 * basic bed (chromosome,start,end) and file offset information. */
{
// uglyf("crTreeFileCreate %s itemCount=%llu, chromCount=%d\n", fileName, itemCount, chromCount);
/* Open file and write header. */
FILE *f = mustOpen(fileName, "wb");
bits32 magic = crTreeSig;
bits32 reserved32 = 0;
bits64 chromOffset = crHeaderSize;
bits64 cirOffset = 0;
bits64 reserved64 = 0;
writeOne(f, magic);
writeOne(f, reserved32);
writeOne(f, chromOffset);
writeOne(f, cirOffset);	       /* Will fill this back in later */
writeOne(f, reserved64);
writeOne(f, reserved64);
writeOne(f, reserved64);
writeOne(f, reserved64);
writeOne(f, reserved64);

/* Convert array of chromosomes to a sorted array of name32s.  Also
 * figure out maximum chromosome name size. */
struct name32 *name32Array;
AllocArray(name32Array, chromCount);
bits32 chromIx;
int maxChromNameSize = 0;
for (chromIx=0; chromIx<chromCount; ++chromIx)
    {
    struct name32 *name32 = &name32Array[chromIx];
    char *name = chromNames[chromIx];
    name32->name = name;
    int nameSize = strlen(name);
    if (nameSize > maxChromNameSize)
        maxChromNameSize = nameSize;
    }
qsort(name32Array, chromCount, sizeof(name32Array[0]), name32Cmp);
for (chromIx=0; chromIx<chromCount; ++chromIx)
    {
    struct name32 *name32 = &name32Array[chromIx];
    name32->val = chromIx;
    }

/* Write out bPlusTree index of chromosome IDs. */
int chromBlockSize = min(blockSize, chromCount);
bptFileBulkIndexToOpenFile(name32Array, sizeof(name32Array[0]), chromCount, chromBlockSize,
    name32Key, maxChromNameSize, name32Val, sizeof(name32Array[0].val), f);
	
/* Convert itemArray to ciItemArray.  This is mainly to avoid having to do the chromosome to
 * chromosome index conversion for each item.  The cost is some memory though.... */
struct ciItem *ciItemArray;
AllocArray(ciItemArray, itemCount);
bits64 itemIx;
char *itemPos = itemArray;
char *lastChrom = "";
bits32 lastChromIx = 0;
for (itemIx=0; itemIx < itemCount; ++itemIx)
    {
    struct ciItem *ciItem = &ciItemArray[itemIx];
    ciItem->item = itemPos;
    ciItem->key = (*fetchKey)(itemPos);
    if (!sameString(lastChrom, ciItem->key.chrom))
        {
	lastChrom = ciItem->key.chrom;
	lastChromIx = mustFindChromIx(lastChrom, name32Array, chromCount);
	}
    ciItem->chromIx = lastChromIx;
    itemPos += itemSize;
    }

/* Record starting position of r tree and write it out. */
cirOffset = ftell(f);
struct ciContext context;
ZeroVar(&context);
context.fetchKey = fetchKey;
context.fetchOffset = fetchOffset;
cirTreeFileBulkIndexToOpenFile(ciItemArray, sizeof(ciItemArray[0]), itemCount, blockSize, 
	itemsPerSlot, &context, ciItemFetchKey, ciItemFetchOffset, totalDataSize, f);

/* Seek back and write offset to r tree. */
fseek(f, cirOffsetPos, SEEK_SET);
writeOne(f, cirOffset);


/* Clean up */
freez(&name32Array);
carefulClose(&f);
}

int crTreeItemCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,start,end. */
{
const struct crTreeItem *a = ((struct crTreeItem *)va);
const struct crTreeItem *b = ((struct crTreeItem *)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->start - b->start;
if (dif == 0)
    dif = a->end - b->end;
return dif;
}

struct crTreeRange crTreeItemKey(const void *va)
/* Get key fields. */
{
const struct crTreeItem *a = *((struct crTreeItem **)va);
struct crTreeRange ret;
ret.chrom = a->chrom;
ret.start = a->start;
ret.end = a->end;
return ret;
}

bits64 crTreeItemOffset(const void *va)
/* Get offset of item in file. */
{
const struct crTreeItem *a = *((struct crTreeItem **)va);
return a->fileOffset;
}

void crTreeFileCreateInputCheck(struct crTreeItem *itemList, struct hash *chromHash, 
	bits32 blockSize, bits32 itemsPerSlot, bits64 endPosition, char *fileName)
/* Do sanity checking on itemList and chromHash and endPosition.  Make sure that itemList is
 * sorted properly mostly. */
{
struct crTreeItem *item, *next;
for (item = itemList; item != NULL; item = next)
    {
    next = item->next;
    if (next != NULL)
	{
        if (crTreeItemCmp(item, next) > 0)
	    errAbort("Out of order itemList in crTreeFileCreateInputCheck");
	if (item->fileOffset > next->fileOffset)
	    errAbort("Out of sequence itemList in crTreeFileCreateInputCheck");
	}
    }
}


void crTreeFileCreate(
	struct crTreeItem *itemList,  /* List of all items - sorted here and in underlying file. */
	struct hash *chromHash,	      /* Hash of all chromosome names. */
	bits32 blockSize,	/* R tree block size - # of children for each node. 1024 is good. */
	bits32 itemsPerSlot,	/* Number of items to put in each index slot at lowest level.
				 * Typically either blockSize/2 or 1. */
	bits64 endPosition,	/* File offset after have read all items in file. */
	char *fileName)        /* Name of output file. */
/* Create a cr tree index of file. The itemList contains the position of each item in the
 * chromosome and in the file being indexed.  Both the file and the itemList must be sorted
 * by chromosome (alphabetic), start (numerical), end (numerical). 
 *    We recommend you run crTreeFileCreateInputCheck on the input parameters right before
 * calling this if you have problems. */
{
/* We can't handle empty input... */
if (itemList == NULL)
    errAbort("crTreeFileCreate can't handle empty itemList.");

/* Make array of pointers out of linked list. */
struct crTreeItem **itemArray;
bits32 itemCount = slCount(itemList);
AllocArray(itemArray, itemCount);
struct crTreeItem *item;
int itemIx;
for (itemIx=0, item=itemList; itemIx<itemCount; ++itemIx, item=item->next)
    itemArray[itemIx] = item;

/* Make up chromosome array. */
int chromCount = chromHash->elCount;
char **chromArray;
AllocArray(chromArray, chromCount);
struct hashEl *el, *list = hashElListHash(chromHash);
bits32 chromIx;
for (el = list, chromIx=0; el != NULL; el = el->next, ++chromIx)
    chromArray[chromIx] = el->name;
slFreeList(&list);

/* Call function to make index file. */
crTreeFileCreateLow(chromArray, chromCount, itemArray, sizeof(itemArray[0]), itemCount,
	blockSize, itemsPerSlot, crTreeItemKey, crTreeItemOffset, itemList->fileOffset, 
	endPosition, fileName);
}

/****** Start of reading and searching (as opposed to file creation) code *******/

struct crTreeFile *crTreeFileOpen(char *fileName)
/* Open up r-tree index file - reading headers and verifying things. */
{
/* Open file and allocate structure to hold info from header etc. */
struct udcFile *udc = udcFileOpen(fileName, udcDefaultDir());
struct crTreeFile *crt = needMem(sizeof(*crt));
fileName = crt->fileName = cloneString(fileName);
crt->udc = udc;

/* Read magic number at head of file and use it to see if we are proper file type, and
 * see if we are byte-swapped. */
bits32 magic;
boolean isSwapped = FALSE;
udcMustReadOne(udc, magic);
if (magic != crTreeSig)
    {
    magic = byteSwap32(magic);
    isSwapped = crt->isSwapped = TRUE;
    if (magic != crTreeSig)
       errAbort("%s is not a chromosome r-tree index file", fileName);
    }

/* Read rest of high level header including notably the offsets to the
 * chromosome and range indexes. */
bits32 reserved32;
udcMustReadOne(udc, reserved32);
crt->chromOffset = udcReadBits64(udc, isSwapped);
crt->cirOffset = udcReadBits64(udc, isSwapped);

/* Read in the chromosome index header. */
udcSeek(udc, crt->chromOffset);
crt->chromBpt = bptFileAttach(fileName, udc);

/* Read in range index header. */
udcSeek(udc, crt->cirOffset);
crt->cir = cirTreeFileAttach(fileName, udc);

return crt;
}

void crTreeFileClose(struct crTreeFile **pCrt)
/* Close and free up crTree file opened with crTreeFileAttach. */
{
struct crTreeFile *crt = *pCrt;
if (crt != NULL)
    {
    cirTreeFileDetach(&crt->cir);
    bptFileDetach(&crt->chromBpt);
    udcFileClose(&crt->udc);
    freez(&crt->fileName);
    freez(pCrt);
    }
}


struct fileOffsetSize *crTreeFindOverlappingBlocks(struct crTreeFile *crt, 
	char *chrom, bits32 start, bits32 end)
/* Return list of file blocks that between them contain all items that overlap
 * start/end on chromIx.  Also there will be likely some non-overlapping items
 * in these blocks too. When done, use slListFree to dispose of the result. */
{
/* Find chromosome index.  Return NULL if no such chromosome*/
bits32 chromIx;
if (!bptFileFind(crt->chromBpt, chrom, strlen(chrom), &chromIx, sizeof(chromIx)))
    return NULL;

return cirTreeFindOverlappingBlocks(crt->cir, chromIx, start, end);
}


