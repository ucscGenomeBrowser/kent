/* crTree chromosome r tree. This module creates and uses an index that can find items
 * that overlap with a chromosome range - something of the form chrN:start-end. */

#include "common.h"
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

void crTreeFileCreate(
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
    name32->val = chromIx;
    int nameSize = strlen(name);
    if (nameSize > maxChromNameSize)
        maxChromNameSize = nameSize;
    }
qsort(name32Array, chromCount, sizeof(name32Array[0]), name32Cmp);

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

