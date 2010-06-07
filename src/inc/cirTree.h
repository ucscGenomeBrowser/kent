/* cirTree chromosome id r tree.  Part of a system to index chromosome ranges - things of
 * form chrN:start-end.  Generally you'll be using the crTree module - which
 * makes use of this module and the bPlusTree module - rather than this module directly.
 * This module works with chromosomes mapped to small integers rather than chromosomes
 * as strings, saving space and speeding things up in the process, but requiring the
 * separate bPlusTree to map the names to IDs. 
 *   This module implements a one dimensional R-tree index treating the chromosome ID
 * as the most significant part of a two-part key, and the base position as the least 
 * significant part of the key.  */

#ifndef CIRTREE_H
#define CIRTREE_H

struct cirTreeFile
/* R tree index file handle. */
    {
    struct cirTreeFile *next;	/* Next in list of index files if any. */
    char *fileName;		/* Name of file - for error reporting. */
    struct udcFile *udc;			/* Open file pointer. */
    boolean isSwapped;		/* If TRUE need to byte swap everything. */
    bits64 rootOffset;		/* Offset of root block. */
    bits32 blockSize;		/* Size of block. */
    bits64 itemCount;		/* Number of items indexed. */
    bits32 startChromIx;	/* First chromosome in file. */
    bits32 startBase;		/* Starting base position. */
    bits32 endChromIx;		/* Ending chromosome in file. */
    bits32 endBase;		/* Ending base position. */
    bits64 fileSize;		/* Total size of index file. */
    bits32 itemsPerSlot;	/* Max number of items to put in each index slot at lowest level. */
    };

struct cirTreeFile *cirTreeFileOpen(char *fileName);
/* Open up r-tree index file - reading header and verifying things. */

void cirTreeFileClose(struct cirTreeFile **pCrt);
/* Close and free up cirTree file opened with cirTreeFileAttach. */

struct cirTreeFile *cirTreeFileAttach(char *fileName, struct udcFile *udc);
/* Open up r-tree index file on previously open file, with cirTree
 * header at current file position. */

void cirTreeFileDetach(struct cirTreeFile **pCrt);
/* Detach and free up cirTree file opened with cirTreeFileAttach. */

struct fileOffsetSize *cirTreeFindOverlappingBlocks(struct cirTreeFile *crf, 
	bits32 chromIx, bits32 start, bits32 end);
/* Return list of file blocks that between them contain all items that overlap
 * start/end on chromIx.  Also there will be likely some non-overlapping items
 * in these blocks too. When done, use slListFree to dispose of the result. */

struct cirTreeRange
/* A chromosome id and an interval inside it. */
    {
    bits32 chromIx;	/* Chromosome id. */
    bits32 start;	/* Start position in chromosome. */
    bits32 end;		/* One past last base in interval in chromosome. */
    };

void cirTreeFileBulkIndexToOpenFile(
	void *itemArray, int itemSize, bits64 itemCount, 
	bits32 blockSize, bits32 itemsPerSlot,
	void *context,
	struct cirTreeRange (*fetchKey)(const void *va, void *context),
	bits64 (*fetchOffset)(const void *va, void *context), 
	bits64 endFileOffset, FILE *f);
/* Create a r tree index from a sorted array, writing output starting at current position
 * of an already open file.  See cirTreeFileCreate for explanation of parameters. */

void cirTreeFileCreate(
	void *itemArray, 	/* Sorted array of things to index.  Sort on chromIx,start. */
	int itemSize, 		/* Size of each element in array. */
	bits64 itemCount, 	/* Number of elements in array. */
	bits32 blockSize,	/* R tree block size - # of children for each node. */
	bits32 itemsPerSlot,	/* Number of items to put in each index slot at lowest level. */
	void *context,		/* Context pointer for use by fetch call-back functions. */
	struct cirTreeRange (*fetchKey)(const void *va, void *context),/* Given item, return key. */
	bits64 (*fetchOffset)(const void *va, void *context), /* Given item, return file offset */
	bits64 endFileOffset,				 /* Last position in file we index. */
	char *fileName);                                 /* Name of output file. */
/* Create a r tree index file from a sorted array. */

#endif /* CIRTREE_H */
