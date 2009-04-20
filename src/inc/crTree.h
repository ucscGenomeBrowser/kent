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
 * makes a list of crTreeItems, one for each item in the file. A crTreeItem just 
 * contains the chromosome range and file offset for an item. In the process of scanning, 
 * build up a hash containing the chromosome names (using hashStoreName) rather than allocating
 * a separate string for each chromosome name in each item.  Pass the list and the
 * hash to crTreeFileCreate.
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

#ifndef CRTREE_H
#define CRTREE_H

struct crTreeFile
/* Chromosome R tree index file handle. */
    {
    struct crTreeFile *next;	/* Next in list of index files if any. */
    char *fileName;		/* Name of file - for better error reporting. */
    struct udcFile *udc;			/* Open file handle. */
    struct bptFile *chromBpt;	/* Index of chromosomes. */
    struct cirTreeFile *cir;	/* Index of ranges. */
    boolean isSwapped;		/* If TRUE need to byte swap everything. */
    bits64 chromOffset;		/* Offset of chromosome index. */
    bits64 cirOffset;		/* Offset of range index. */
    };

struct crTreeFile *crTreeFileOpen(char *fileName);
/* Open up r-tree index file - reading headers and verifying things. */

void crTreeFileClose(struct crTreeFile **pCrt);
/* Close and free up crTree file opened with crTreeFileAttach. */

struct fileOffsetSize *crTreeFindOverlappingBlocks(struct crTreeFile *crt, 
	char *chrom, bits32 start, bits32 end);
/* Return list of file blocks that between them contain all items that overlap
 * start/end on chromIx.  Also there will be likely some non-overlapping items
 * in these blocks too. When done, use slListFree to dispose of the result. */

struct crTreeItem
/* A chromosome and an interval inside it. */
    {
    struct crTreeItem *next;  /* Next in singly linked list. */
    char *chrom;	/* Name of chromosome not allocated here. */
    bits32 start;	/* Start position in chromosome. */
    bits32 end;	/* One past last base in interval in chromosome. */
    bits64 fileOffset;	/* Offset of item in file we are indexing. */
    };

void crTreeFileCreateInputCheck(struct crTreeItem *itemList, struct hash *chromHash, 
	bits32 blockSize, bits32 itemsPerSlot, bits64 endPosition, char *fileName);
/* Do sanity checking on itemList and chromHash and endPosition.  Make sure that itemList is
 * sorted properly mostly. */

void crTreeFileCreate(
	struct crTreeItem *itemList,  /* List of all items - sorted here and in underlying file. */
	struct hash *chromHash,	      /* Hash of all chromosome names. */
	bits32 blockSize,	/* R tree block size - # of children for each node. 1024 is good. */
	bits32 itemsPerSlot,	/* Number of items to put in each index slot at lowest level.
				 * Typically either blockSize/2 or 1. */
	bits64 endPosition,	/* File offset after have read all items in file. */
	char *fileName);        /* Name of output file. */
/* Create a cr tree index of file. The itemList contains the position of each item in the
 * chromosome and in the file being indexed.  Both the file and the itemList must be sorted
 * by chromosome (alphabetic), start (numerical), end (numerical). */

#endif /* CRTREE_H */

