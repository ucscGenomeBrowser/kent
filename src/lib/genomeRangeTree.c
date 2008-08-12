/* genomeRangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals) across a whole
 * genome (multiple chromosomes or scaffolds). 
 * It is a hash table container mapping chrom to rangeTree.
 * Most of the work is performed by rangeTree, this container
 * enables local memory and stack to be shared by many rangeTrees
 * so it should be able to handle genomes with a very large 
 * number of scaffolds. See rangeTree for more information. */

#include "common.h"
#include "sig.h"
#include "localmem.h"
#include "rbTree.h"
#include "hash.h"
#include "rangeTree.h"
#include "genomeRangeTree.h"
#include <limits.h>


struct rbTree *genomeRangeTreeFindRangeTree(struct genomeRangeTree *tree, char *chrom)
/* Find the rangeTree for this chromosome, if any. Returns NULL if chrom not found.
 * Free with genomeRangeTreeFree. */
{
return hashFindVal(tree->hash, chrom);
}

struct rbTree *genomeRangeTreeFindOrAddRangeTree(struct genomeRangeTree *tree, char *chrom)
/* Find the rangeTree for this chromosome, or add new chrom and empty rangeTree if not found.
 * Free with genomeRangeTreeFree. */
{
struct hashEl *hel;
hel = hashStore(tree->hash, chrom);
if (hel->val == NULL) /* need to add a new rangeTree */
    hel->val = rangeTreeNewDetailed(tree->lm, tree->stack);
return (struct rbTree *)hel->val;
}


struct range *genomeRangeTreeAdd(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. */
{
return rangeTreeAdd(genomeRangeTreeFindOrAddRangeTree(tree,chrom), start, end);
}

struct range *genomeRangeTreeAddVal(struct genomeRangeTree *tree, char *chrom, int start, int end, void *val, void *(*mergeVals)(void *existing, void*new))
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. 
 * If this is a new range, set the value to this val.
 * If there are existing items for this range, and if mergeVals function is not null, 
 * apply mergeVals to the existing values and this new val, storing the result as the val
 * for this range (see rangeTreeAddValCount() and rangeTreeAddValList() below for examples). */
{
return rangeTreeAddVal(genomeRangeTreeFindOrAddRangeTree(tree,chrom), start, end, val, mergeVals);
}

struct range *genomeRangeTreeAddValCount(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. 
 * Set range val to count of elements in the range. Counts are pointers to 
 * ints allocated in tree localmem */
{
return rangeTreeAddValCount(genomeRangeTreeFindOrAddRangeTree(tree,chrom), start, end);
}

struct range *genomeRangeTreeAddValList(struct genomeRangeTree *tree, char *chrom, int start, int end, void *val)
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. 
 * Add val to the list of values (if any) in each range.
 * val must be valid argument to slCat (ie, be a struct with a 'next' pointer as its first member) */
{
return rangeTreeAddValList(genomeRangeTreeFindOrAddRangeTree(tree,chrom), start, end, val);
}

boolean genomeRangeTreeOverlaps(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Return TRUE if start-end overlaps anything in tree */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeOverlaps(t, start, end) : FALSE;
}

struct range *genomeRangeTreeFindEnclosing(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Find item in range tree that encloses range between start and end 
 * if there is any such item. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeFindEnclosing(t, start, end) : NULL;
}

struct range *genomeRangeTreeAllOverlapping(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Return list of all items in range tree that overlap interval start-end.
 * Do not free this list, it is owned by tree.  However it is only good until
 * next call to rangeTreeFindInRange or rangeTreeList. Not thread safe. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeAllOverlapping(t, start, end) : NULL;
}

struct range *genomeRangeTreeMaxOverlapping(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Return item that overlaps most with start-end. Not thread safe.  Trashes list used
 * by rangeTreeAllOverlapping. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeMaxOverlapping(t, start, end) : NULL;
}

int genomeRangeTreeOverlapSize(struct genomeRangeTree *tree, char *chrom, int start, int end)
/* Return the total size of intersection between interval
 * from start to end, and items in range tree. Sadly not
 * thread-safe. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeOverlapSize(t, start, end) : 0;
}

struct range *genomeRangeTreeList(struct genomeRangeTree *tree, char *chrom)
/* Return list of all ranges in single rangeTree in order.  Not thread safe. 
 * No need to free this when done, memory is local to tree. */
{
struct rbTree *t;
return (t=genomeRangeTreeFindRangeTree(tree,chrom)) ? rangeTreeList(t) : NULL;
}

struct genomeRangeTree *genomeRangeTreeNew()
/* Create a new, empty, genomeRangeTree. Uses the default hash size.
 * Free with genomeRangeTreeFree. */
{
return genomeRangeTreeNewSize(0);
}

struct genomeRangeTree *genomeRangeTreeNewSize(int hashPowerOfTwoSize)
/* Create a new, empty, genomeRangeTree. 
 * Free with genomeRangeTreeFree. */
{
struct genomeRangeTree *t;
AllocVar(t); 
t->hash = hashNew(hashPowerOfTwoSize);
t->lm = lmInit(0);
return t;
}

void genomeRangeTreeFree(struct genomeRangeTree **pTree)
/* Free up genomeRangeTree.  */
{
lmCleanup(&((*pTree)->lm));  /* clean up all the memory for all nodes for all trees */
freeHash(&((*pTree)->hash)); /* free the hash table including names (trees are freed by lmCleanup) */
freez(pTree);                /* free this */
}

void genomeRangeTreeWriteOne(struct genomeRangeTree *tree, FILE *f)
/* Write out genomeRangeTree including: 
 * header portion
 * index of chromosomes
 * data for each range tree */
{
bits32 sig = genomeRangeTreeSig;
bits32 version = 0;
bits32 numChroms = tree->hash->elCount;
bits32 valDataSize = 0; /* zero for no data, (later: -ve for variable size, +ve specifies fixed size */
bits32 valDataType = 0; /* unused (later: signature for type ) */
bits32 reserved1 = 0;
bits32 reserved2 = 0;
/* Figure out location of first byte past header (location of the index). */
bits32 headerLen = sizeof(sig) + sizeof(version) + sizeof(headerLen) + sizeof(numChroms)
    + sizeof(valDataSize) + sizeof(valDataType) + sizeof(reserved1) + sizeof(reserved2);
bits32 offset = 0;
bits32 nodes = 0;
long long counter = 0; /* check for 32 bit overflow */
struct hashEl *el, *elList;

/* Write out fixed parts of header. */
writeOne(f, sig);
writeOne(f, version);
writeOne(f, headerLen);
writeOne(f, numChroms);
writeOne(f, valDataSize);
writeOne(f, valDataType);
writeOne(f, reserved1);
writeOne(f, reserved2);

/* Figure out location of first byte past index (location of the data).
 * Each index entry contains 4 bytes of offset information
 * and the name of the sequence, which is variable length. */
offset = headerLen;

/* get a sorted list of all chroms */
elList = hashElListHash(tree->hash);
slNameSort((struct slName **)&elList);

for ( el = elList ; el ; el = el->next )
    {
    int nameLen = strlen(el->name);
    if (nameLen > 255)
        errAbort("name %s too long", el->name);
    /* index is list of triples: (name nodes offset) */
    offset += nameLen + 1 + sizeof(nodes) + sizeof(offset);
    }

/* Write out index. */
for (el = elList; el ; el = el->next)
    {
    int size = rangeTreeSizeInFile(el->val);
    nodes = ((struct rbTree *)el->val)->n;
    writeString(f, el->name);
    writeOne(f, nodes);
    writeOne(f, offset);
    offset += size;
    counter += (long long)size;
    if (counter > UINT_MAX )
        errAbort("Error in %s, index overflow at %s. The genomeRangeTree format "
                "does not support trees larger than %dGb, \n"
                "please split up into smaller files.\n", __FILE__, 
                el->name, UINT_MAX/1000000000);
    }

/* Write out trees. */
for (el = elList; el ; el = el->next)
    {
    rangeTreeWriteNodes(el->val,f);
    }
hashElFreeList(&elList);
}

struct genomeRangeTree *genomeRangeTreeRead(char *fileName)
/* Open file, read in header, index, and trees.  
 * Squawk and die if there is a problem. */
{
bits32 sig, version, headerLen, numChroms, valDataSize, valDataType, reserved1, reserved2;
boolean isSwapped = FALSE;
int i;
struct genomeRangeTree *tree;
FILE *f = mustOpen(fileName, "rb");
struct chromInfo {
    struct chromInfo *next;
    char *name;
    bits32 nodes;
    bits32 offset;
    } *chromList = NULL, *chrom;

/* Allocate header verify signature, and read in
 * the constant-length bits. */
tree = genomeRangeTreeNew();
mustReadOne(f, sig);
if (sig == genomeRangeTreeSwapSig)
    isSwapped = TRUE;
else if (sig != genomeRangeTreeSig)
    errAbort("%s doesn't have a valid genomeRangeTreeSig", fileName);
version = readBits32(f, isSwapped);
if (version != 0)
    {
    errAbort("Can only handle version 0 of this file. This is version %d", (int)version);
    }
headerLen = readBits32(f, isSwapped);
numChroms = readBits32(f, isSwapped);
valDataSize = readBits32(f, isSwapped);
valDataType = readBits32(f, isSwapped);
reserved1 = readBits32(f, isSwapped);
reserved2 = readBits32(f, isSwapped);
if (valDataSize != 0)
    errAbort("Can only handle valDataSize of 0. This has %d.\n", (int)valDataSize);
if (valDataType != 0)
    errAbort("Can only handle valDataType of 0. This has %d.\n", (int)valDataType);

/* Read in index. */
for (i=0; i < numChroms ; ++i)
    {
    char name[256];
    if (!fastReadString(f, name))
        errAbort("%s is truncated", fileName);
    AllocVar(chrom);
    chrom->name = cloneString(name);
    chrom->nodes = readBits32(f, isSwapped);
    chrom->offset = readBits32(f, isSwapped);
    slSafeAddHead(&chromList, chrom);
    }
slReverse(chromList);

/* Read in nodes one tree at a time */
for (chrom = chromList ; chrom ; chrom = chrom->next)
    {
    rangeTreeReadNodes(f, genomeRangeTreeFindOrAddRangeTree(tree, chrom->name), chrom->nodes, isSwapped);
    }

carefulClose(&f);
return tree;
}


