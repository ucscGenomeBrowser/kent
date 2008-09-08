/* genomeRangeTreeFile - This module is a way of serializing
 * and saving genomeRangeTrees, and for modifying saved range trees
 * by creating a file representing the intersection or union of two
 * saved genomeRangeTrees.
 * Also see genomeRangeTree and rangeTree for more information. */

#ifndef GENOMERANGETREEFILE_H
#define GENOMERANGETREEFILE_H

//#ifndef RANGETREE_H
//#include "rangeTree.h"
//#endif

#ifndef GENOMERANGETREE_H
#include "genomeRangeTree.h"
#endif


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


struct genomeRangeTree *genomeRangeTreeRead(char *fileName);
/* Read in the genomeRangeTree data for each chromosome and
 * return the genomeRangeTree.
 * Squawk and die if there is a problem. */

void genomeRangeTreeWrite(struct genomeRangeTree *tree, char *fileName);
/* Write out genomeRangeTree including: 
 * header portion
 * index of chromosomes
 * data for each range tree */

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

void genomeRangeTreeFileStats(char *fileName, int *numChroms, int *nodes, int *size);
/* Calculates the number of chroms, ranges, and total size of ranges in the genomeRangeTree file.
 * Performs a linear scan of the file. */

int genomeRangeTreeFileChromSeek(struct genomeRangeTreeFile *tf, char *chrom);
/* Seek the tree file to the start of the chromosome data.
 * Returns the number of nodes in the chromosome. 
 * If this chromosome is not in this tree, does not do a seek
 * and returns 0 for number of nodes.
 */


#endif /* GENOMERANGETREEFILE_H */

