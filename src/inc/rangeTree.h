/* rangeTree - This module is a way of keeping track of
 * non-overlapping ranges (half-open intervals). It is
 * based on the self-balancing rbTree code.  Use it in
 * place of a bitmap when the total number of ranges
 * is significantly smaller than the number of bits would
 * be. */

#ifndef RANGETREE_H
#define RANGETREE_H

#ifndef RBTREE_H
#include "rbTree.h"
#endif

struct range
/* An interval in a list of intervals. */
    {
    struct range *next;
    int start,end;	/* Zero based half open interval. */
    void *val;		/* Some value associated with range. */
    };

struct rangeStartSize
/* A utility struct for reading and writing arrays of ranges to/from disk */
    {
    bits32 start;
    bits32 size;
    };

struct rbTree *rangeTreeNew();
/* Create a new, empty, rangeTree.  Free with rbFreeTree. */

#define rangeTreeFree(a) rbTreeFree(a)
/* Free up range tree.  */

int rangeCmp(void *va, void *vb);
/* Return -1 if a before b,  0 if a and b overlap,
 * and 1 if a after b. */


struct range *rangeTreeAddVal(struct rbTree *tree, int start, int end, void *val, void *(*mergeVals)(void *existing, void *new) );
/* Add range to tree, merging with existing ranges if need be. 
 * If this is a new range, set the value to this val.
 * If there are existing items for this range, and if mergeVals function is not null, 
 * apply mergeVals to the existing values and this new val, storing the result as the val
 * for this range (see rangeTreeAddValCount() and rangeTreeAddValList() below for examples). */

struct range *rangeTreeAdd(struct rbTree *tree, int start, int end);
/* Add range to tree, merging with existing ranges if need be. */

struct range *rangeTreeAddValCount(struct rbTree *tree, int start, int end);
/* Add range to tree, merging with existing ranges if need be. 
 * Set range val to count of elements in the range. Counts are pointers to 
 * ints allocated in tree localmem */

struct range *rangeTreeAddValList(struct rbTree *tree, int start, int end, void *val);
/* Add range to tree, merging with existing ranges if need be. 
 * Add val to the list of values (if any) in each range.
 * val must be valid argument to slCat (ie, be a struct with a 'next' pointer as its first member) */

boolean rangeTreeOverlaps(struct rbTree *tree, int start, int end);
/* Return TRUE if start-end overlaps anything in tree */

int rangeTreeOverlapSize(struct rbTree *tree, int start, int end);
/* Return the total size of intersection between interval
 * from start to end, and items in range tree. Sadly not
 * thread-safe.
 * On 32 bit machines be careful not to overflow
 * range of start, end or total size return value. */

int rangeTreeOverlapTotalSize(struct rbTree *tree);
/* Return the total size of all ranges in range tree.
 * Sadly not thread-safe. 
 * On 32 bit machines be careful not to overflow
 * range of start, end or total size return value. */

struct range *rangeTreeFindEnclosing(struct rbTree *tree, int start, int end);
/* Find item in range tree that encloses range between start and end 
 * if there is any such item. */

struct range *rangeTreeAllOverlapping(struct rbTree *tree, int start, int end);
/* Return list of all items in range tree that overlap interval start-end.
 * Do not free this list, it is owned by tree.  However it is only good until
 * next call to rangeTreeFindInRange or rangTreeList. Not thread safe. */

struct range *rangeTreeMaxOverlapping(struct rbTree *tree, int start, int end);
/* Return item that overlaps most with start-end. Not thread safe.  Trashes list used
 * by rangeTreeAllOverlapping. */

struct range *rangeTreeList(struct rbTree *tree);
/* Return list of all ranges in tree in order.  Not thread safe. 
 * No need to free this when done, memory is local to tree. */

struct rbTree *rangeTreeNewDetailed(struct lm *lm, struct rbTreeNode *stack[128]);
/* Allocate rangeTree on an existing local memory & stack.  This is for cases
 * where you want a lot of trees, and don't want the overhead for each one. 
 * Note, to clean these up, just do freez(&rbTree) rather than rbFreeTree(&rbTree). */

struct range rangeReadOne(FILE *f, boolean isSwapped);
/* Returns a single range from the file.
 * Returns range start and end. 
 * Does not read val. */

void rangeWriteOne(struct range *r, FILE *f);
/* Write out one range structure to binary file f.
 * This only writes start and size. */

void rangeReadArray(FILE *f, struct rangeStartSize *r, int n, boolean isSwapped);
/* Read 'n' elements of range array (start,size) from file 'f'. */

void rangeWriteArray(struct rangeStartSize *r, int n, FILE *f);
/* Write 'n' elements of range array (start,size) to file 'f'. */

void rangeReadWriteN(FILE *inF, int n, boolean isSwapped, FILE *outF);
/* Read 'n' ranges in from file 'inF' and write them to file 'outF'.
 * Reads and writes ranges one at a time. */

struct range rangeReadOneWithVal(FILE *f, boolean isSwapped, void *(*valReadOne)(FILE *f, boolean isSwapped));
/* Read one range structure from binary file f, including range val.
 * Returns start, end.
 * valWriteOne should point to a function which writes the value (not called if null).
 * Returns val if valWriteOne is not null */

void rangeWriteOneWithVal(struct range *r, FILE *f, void (*valWriteOne)(void *val, FILE *f));
/* Write out one range structure to binary file f.
 * Writes start and size.
 * valWriteOne should point to a function which writes the value (not called if null). */

void rangeTreeReadNodes(FILE *f, struct rbTree *rt, int numNodes, boolean isSwapped);
/* Reads numNodes ranges from the file and adds them to rangeTree rt.
 * Does not read range val.  */

void rangeTreeWriteNodes(struct rbTree *tree, FILE *f);
/* Write out one rangeTree structure to binary file f. 
 * Note this does not include the name, which is stored only in index. 
 * Ranges are written in start sequence (depth-first tree traversal).
 * Writes start and size but not val. 
 * Not thread-safe. */

void rangeTreeReadNodesWithVal(FILE *f, struct rbTree *rt, int numNodes, boolean isSwapped, void *(*valReadOne)(FILE *f, boolean isSwapped));
/* Reads numNodes ranges from the file and adds them to rangeTree rt.
 * If rt contains no nodes, and since rangeTree was saved to disk 
 * implying its ranges are already non-overlapping, it is safe 
 * to use a mergeVal function which simply assigns the stored value 
 * to the range since the existing val must always be null.
 * Produces an error if the rt contains nodes already. */

void rangeTreeReadNodesWithValMerge(FILE *f, struct rbTree *rt, int numNodes, boolean isSwapped, void *(*valReadOne)(FILE *f, boolean isSwapped), void *(*mergeVals)(void *existing, void *new) );
/* Reads numNodes ranges from the file and adds them to rangeTree rt.
 * Reads range values using valReadOne function, if function is non-null.
 * Input rangeTree rt could already have nodes, so mergeVals is called 
 * to merge values read from disk to values in the tree.  */

void rangeTreeWriteNodesWithVal(struct rbTree *tree, FILE *f, void (*valWriteOne)(void *val, FILE *f));
/* Write out one rangeTree structure to binary file f. 
 * Note this does not include the name, which is stored only in index. 
 * Ranges are written in start sequence (depth-first tree traversal).
 * Writes start and size.
 * valWriteOne should be a function which writes the range val. Not called if null.
 * Not thread-safe. */

int rangeTreeSizeInFile(struct rbTree *tree);
/* Returns size of rangeTree written in binary file format.
 * Includes start and size. 
 * Does not include val. */

int rangeTreeSizeInFileWithVal(struct rbTree *tree, int (*rangeValSizeInFile)(void *val));
/* Returns size of rangeTree written in binary file format.
 * Includes start, size, and val. 
 * rangeValSizeInFile should refer to a function which calculates the size of the val 
 * in a binary file. Not called if null. */

int rangeTreeFileMerge(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, FILE *of);
/* Merge n1 ranges from array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges (start,size) in r2, writing them to output file 'of'. 
 * Note that the ranges are as stored on disk (start,size) not (start,end).
 * Returns number of nodes in merged file. */


#endif /* RANGETREE_H */

