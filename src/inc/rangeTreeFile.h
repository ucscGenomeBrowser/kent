/* rangeTreeFile - This module is a way of reading and writing
 * rangeTrees, as well as creating the union and intersection of 
 * two range tree files.
 * See rangeTree and rbTree for more info. */

#ifndef RANGETREEFILE_H
#define RANGETREEFILE_H

#ifndef RANGETREE_H
#include "rangeTree.h"
#endif

struct rangeStartSize
/* A utility struct for reading and writing arrays of ranges to/from disk */
    {
    bits32 start;
    bits32 size;
    };

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

unsigned rangeArraySize(struct rangeStartSize *r, int n);
/* calculate the total size of the array */

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

unsigned rangeTreeFileIntersection(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, struct rangeStartSize **pRange, int *n, boolean saveMem);
/* Create intersection of array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges in r2, saving them in array r and returning 
 * the number of merged ranges in n.
 * Note that the ranges are as stored on disk (start,size), 
 * not as in the rangeTree (start,end).
 * Free array with freez(&r)
 * Returns total size of ranges in r. */

unsigned rangeTreeFileUnion(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, struct rangeStartSize **pRange, int *n, boolean saveMem);
/* Create union of array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges in r2, saving them in array r and returning 
 * the number of merged ragnes in n. 
 * Note that the ranges are as stored on disk (start,size)
 * not as in the rangeTree (start,end).
 * Free array with freez(&r)
 * Returns total size of ranges in r. */

unsigned rangeTreeFileUnionToFile(struct rangeStartSize *r1, struct rangeStartSize *r2, int n1, int n2, FILE *of, int *n);
/* Create union of array of 'n1' ranges (start,size) in r1 with 
 * 'n2' ranges in r2, writing them to output file 'of' and returning 
 * the number of merged ranges written in 'n'. 
 * Note that the ranges are as stored on disk (start,size)
 * not as in the rangeTree (start,end).
 * Writes the ranges one-by-one.
 * Returns total size of ranges in merged file. */

#endif /* RANGETREEFILEFILE_H */

