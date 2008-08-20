/* genomeRangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals) across a whole
 * genome (multiple chromosomes or scaffolds). 
 * It is a hash table container mapping chrom to rangeTree.
 * Most of the work is performed by rangeTree, this container
 * enables local memory and stack to be shared by many rangeTrees
 * so it should be able to handle genomes with a very large 
 * number of scaffolds. See rangeTree for more information. */

#ifndef GENOMERANGETREE_H
#define GENOMERANGETREE_H

#ifndef HASH_H
#include "hash.h"
#endif

#ifndef DYSTRING_H
#include "dystring.h"
#endif

#ifndef RANGETREE_H
#include "rangeTree.h"
#endif

struct genomeRangeTree
/* A structure that allows efficient random access of ranges of bases covered
 * by a particular feature genome-wide.  Implemented as a range tree for each
 * scaffold or chromosome in a genome. */
    {
    struct genomeRangeTree *next;  /* Next genomeRangeTree in list if any. */
    struct hash *hash;             /* Hash of rangeTrees keyed by chromosome/scaffold name. */
    struct rbTreeNode *stack[128]; /* Stack for binary search. */
    struct lm *lm;                 /* Local memory pool for tree nodes and ranges. */
    };

struct genomeRangeTreeFile 
/* A structure which stores header, index, and file state information
 * for a genomeRangeTree saved to a file. */
    {
    struct genomeRangeTreeFile *next;
    char *name;
    FILE *file;
    struct genomeRangeTree *tree;
    bits32 sig;
    bits32 version;
    bits32 headerLen;
    bits32 numChroms;
    bits32 valDataSize;
    bits32 valDataType;
    bits32 reserved1;
    bits32 reserved2;
    boolean isSwapped;
    struct hashEl *chromList; /* Ordered list of (chrom,rangeTree) hashEls  */
    struct hash *nodes;       /* hash of (chrom, nodes) */
    struct hash *offset;      /* hash of (chrom, offset) */
    };


struct genomeRangeTree *genomeRangeTreeNew();
/* Create a new, empty, genomeRangeTree. Uses the default hash size.
 * Free with genomeRangeTreeFree. */

struct genomeRangeTree *genomeRangeTreeNewSize(int hashPowerOfTwoSize);
/* Create a new, empty, genomeRangeTree. 
 * Free with genomeRangeTreeFree. */

void genomeRangeTreeFree(struct genomeRangeTree **pTree);
/* Free up genomeRangeTree. */

struct rbTree *genomeRangeTreeFindRangeTree(struct genomeRangeTree *tree, char *chrom);
/* Find the rangeTree for this chromosome, if any. Returns NULL if chrom not found.
 * Free with genomeRangeTreeFree. */

struct rbTree *genomeRangeTreeFindOrAddRangeTree(struct genomeRangeTree *tree, char *chrom);
/* Find the rangeTree for this chromosome, or add new chrom and empty rangeTree if not found.
 * Free with genomeRangeTreeFree. */

struct range *genomeRangeTreeAdd(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. */

struct range *genomeRangeTreeAddVal(struct genomeRangeTree *tree, char *chrom, int start, int end, void *val, void *(*mergeVals)(void *existing, void*new));
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. 
 * If this is a new range, set the value to this val.
 * If there are existing items for this range, and if mergeVals function is not null, 
 * apply mergeVals to the existing values and this new val, storing the result as the val
 * for this range (see rangeTreeAddValCount() and rangeTreeAddValList() below for examples). */

struct range *genomeRangeTreeAddValCount(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. 
 * Set range val to count of elements in the range. Counts are pointers to 
 * ints allocated in tree localmem */

struct range *genomeRangeTreeAddValList(struct genomeRangeTree *tree, char *chrom, int start, int end, void *val);
/* Add range to tree, merging with existing ranges if need be. 
 * Adds new rangeTree if chrom not found. 
 * Add val to the list of values (if any) in each range.
 * val must be valid argument to slCat (ie, be a struct with a 'next' pointer as its first member) */

boolean genomeRangeTreeOverlaps(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Return TRUE if start-end overlaps anything in tree. */

int genomeRangeTreeOverlapSize(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Return the total size of intersection between interval
 * from start to end, and items in range tree. Sadly not
 * thread-safe. */

struct range *genomeRangeTreeFindEnclosing(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Find item in range tree that encloses range between start and end 
 * if there is any such item. */

struct range *genomeRangeTreeAllOverlapping(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Return list of all items in range tree that overlap interval start-end.
 * Do not free this list, it is owned by tree.  However it is only good until
 * next call to rangeTreeFindInRange or rangeTreeList. Not thread safe. */

struct range *genomeRangeTreeMaxOverlapping(struct genomeRangeTree *tree, char *chrom, int start, int end);
/* Return item that overlaps most with start-end. Not thread safe.  Trashes list used
 * by rangeTreeAllOverlapping. */

struct range *genomeRangeTreeList(struct genomeRangeTree *tree, char *chrom);
/* Return list of all ranges in single rangeTree in order.  Not thread safe. 
 * No need to free this when done, memory is local to tree. */

struct genomeRangeTree *genomeRangeTreeRead(char *fileName);
/* Read in the genomeRangeTree data for each chromosome and
 * return the genomeRangeTree.
 * Squawk and die if there is a problem. */

void genomeRangeTreeWrite(struct genomeRangeTree *tree, char *fileName);
/* Write out genomeRangeTree including: 
 * header portion
 * index of chromosomes
 * data for each range tree */

struct dyString *genomeRangeTreeToString(struct genomeRangeTree *tree);
/* Return a string representation of the genomeRangeTree.
 * Useful for testing.
 * Not thread-safe; uses globals */

/*
 * genomeRangeTreeFile 
 */

struct genomeRangeTreeFile *genomeRangeTreeFileNew(struct genomeRangeTree *tree, char *fileName);
/* Create a genomeRangeTreeFile to save a genomeRangeTree in 'fileName'. 
 * Opens the file.
 * Call genomeRangeTreeFileWriteHeader() to write the header data only.
 * Call genomeRangeTreeFileWriteData() to write the data portion only.  */

struct genomeRangeTreeFile *genomeRangeTreeFileReadHeader(char *fileName);
/* Creates a genomeRangeTreeFile to read a genomeRangeTree from 'fileName'.
 * Opens the file, reads in header and index. 
 * Leaves file handle open at begining of data portion.
 * Returns a genomeRangeTreeFile containing file handle and index into contents.
 * To read genomeRangeTree data use: genomeRangeTreeFileReadData().
 * To return genomeRangeTree and close file and index use: genomeRangeTreeFileFree()
 * Squawk and die if there is a problem. */

struct genomeRangeTree *genomeRangeTreeFileReadData(struct genomeRangeTreeFile *f);
/* Read in the genomeRangeTree data for each chromosome and
 * return the genomeRangeTree.
 * File handle is left open pointing at the end of the file.
 * To close and free the genomeRangeTreeFile use: genomeRangeTreeFileFree().
 * Squawk and die if there is a problem. */

void genomeRangeTreeFileWriteHeader(struct genomeRangeTreeFile *f);
/* Write out genomeRangeTree header including: 
 *  header portion
 *  index of chromosomes.
 * To close the file use: genomeRangeTreeFileFree(). */

void genomeRangeTreeFileWriteData(struct genomeRangeTreeFile *f);
/* Write out genomeRangeTree data for each chromosome in chroms. */

struct genomeRangeTree *genomeRangeTreeFileFree(struct genomeRangeTreeFile **pFile);
/* Free up the resources associated with a genomeRangeTreeFile.
 * Close the file.
 * Return the genomeRangeTree. */

void genomeRangeTreeFileUnionDetailed(struct genomeRangeTreeFile *tf1, struct genomeRangeTreeFile *tf2, char *outFile, int *numChroms, int *nodes, unsigned *size, boolean saveMem, boolean orDirectToFile);
/* Create union of two saved genomeRangeTrees through a linear file scan.
 * Writes resulting genomeRangeTree to outFile. 
 * The resulting file cannot be safely read until the operation is complete. The header
 * information at the beginning of the file has to be updated after all the data is written
 * since the number of nodes in the final merged rangeTree is not known until the ranges are merged.
 * To enforce this, the header is written with a zero initial 'sig' field so that it cannot
 * be read as a genomeRangeTree file. The header information and 'sig' is re-written with 
 * correct data at the end of the process via an 'fseek' operation to the beginning of the file. 
 * If outFile is null, does not output the file. 
 * The number of nodes in the resulting tree is returned in n.
 * If size is not NULL, this will return the total size of the resulting ranges (adds 'n' 
 * calculations to run time of program). */

void genomeRangeTreeFileUnion(struct genomeRangeTreeFile *tf1, struct genomeRangeTreeFile *tf2, char *outFile);
/* Combine two saved genomeRangeTrees in a logical 'or' through a linear file scan.
 * Writes resulting genomeRangeTree to outFile. 
 * The resulting file cannot be safely read until the operation is complete. The header
 * information at the beginning of the file has to be updated after all the data is written
 * since the final merged rangeTree sizes are not known until the ranges are merged.
 * To enforce this, the header is written with a zero initial 'sig' field so that it cannot
 * be read as a genomeRangeTree file. The header information and 'sig' is re-written with 
 * correct data at the end of the process via an 'fseek' operation to the beginning of the file. 
 * If outFile is null, does not output the file. */

void genomeRangeTreeFileIntersectionDetailed(struct genomeRangeTreeFile *tf1, struct genomeRangeTreeFile *tf2, char *outFile, int *numChroms, int *nodes, unsigned *size, boolean saveMem);
/* Create intersection genomeRangeTree from two saved genomeRangeTrees in a logical 'and' through a linear file scan.
 * Writes resulting genomeRangeTree to outFile if outFile is non-null.
 * Returns number of nodes in n.
 * Returns total size of ranges if size is non-null.
 * The resulting file cannot be safely read until the operation is complete. The header
 * information at the beginning of the file has to be updated after all the data is written
 * since the final merged rangeTree sizes are not known until the ranges are merged.
 * To enforce this, the header is written with a zero initial 'sig' field so that it cannot
 * be read as a genomeRangeTree file. The header information and 'sig' is re-written with 
 * correct data at the end of the process via an 'fseek' operation to the beginning of the file. */


#endif /* GENOMERANGETREE_H */

