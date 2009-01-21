/* crTree chromosome r tree. This module creates and uses a disk-based index that can find items
 * that overlap with a chromosome range - something of the form chrN:start-end - with a
 * minimum of disk access.  It is implemented with a combination of bPlusTrees and r-trees. 
 * The items being indexed can overlap with each other.  */

#define crTreeSig 0x2369ADE1

struct crTreeFile
/* R tree index file handle. */
    {
    struct crTreeFile *next;	/* Next in list of index files if any. */
    char *fileName;		/* Name of file - for better error reporting. */
    FILE *f;			/* Open file handle. */
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

struct crTreeRange
/* A chromosome id and an interval inside it. */
    {
    char *chrom;	/* Chromosome id. String memory owned elsewhere. */
    bits32 start;	/* Start position in chromosome. */
    bits32 end;		/* One past last base in interval in chromosome. */
    };

void crTreeFileCreate(
	char **chromNames,	/* All chromosome (or contig) names */
	int chromCount,		/* Number of chromosomes. */
	void *itemArray, 	/* Sorted array of things to index. Must sort by chrom,start. */
	int itemSize, 		/* Size of each element in array. */
	bits64 itemCount, 	/* Number of elements in array. */
	bits32 blockSize,	/* R tree block size - # of children for each node. */
	bits32 itemsPerSlot,	/* Number of items to put in each index slot at lowest level. */
	struct crTreeRange (*fetchKey)(const void *va),   /* Given item, return key. */
	bits64 (*fetchOffset)(const void *va), 		 /* Given item, return file offset */
	bits64 initialDataOffset,			 /* Offset of 1st piece of data in file. */
	bits64 totalDataSize,				 /* Total size of data we are indexing. */
	char *fileName);                                 /* Name of output file. */
/* Create a r tree index file from an array of chromosomes and an array of items with
 * basic bed (chromosome,start,end) and file offset information. */

