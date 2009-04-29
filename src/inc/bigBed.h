/* bigBed - interface to binary file with bed-style values (that is a bunch of
 * possibly overlapping regions.
 *
 * This shares a lot with the bigWig module. */

#ifndef BIGBED_H
#define BIGBED_H

#ifndef BBIFILE
#include "bbiFile.h"
#endif

struct bigBedInterval
/* A partially parsed out bed record plus some extra fields. */
    {
    struct bigBedInterval *next;	/* Next in list. */
    bits32 start, end;		/* Range inside chromosome - half open zero based. */
    char *rest;			/* Rest of line. May be NULL*/
    };

struct ppBed
/* A partially parsed out bed record plus some extra fields. */
    {
    struct ppBed *next;	/* Next in list. */
    char *chrom;		/* Chromosome name (not allocated here) */
    bits32 start, end;		/* Range inside chromosome - half open zero based. */
    char *rest;			/* The rest of the bed. */
    bits64 fileOffset;		/* File offset. */
    bits32 chromId;		/* Chromosome ID. */
    };

struct bbiFile *bigBedFileOpen(char *fileName);
/* Open up big bed file.   Free this up with bbiFileFree */

struct bigBedInterval *bigBedIntervalQuery(struct bbiFile *bbi, char *chrom, 
	bits32 start, bits32 end, int maxItems, struct lm *lm);
/* Get data for interval.  Return list allocated out of lm.  Set maxItems to maximum
 * number of items to return, or to 0 for all items. */

int bigBedIntervalToRow(struct bigBedInterval *interval, char *chrom, char *startBuf, char *endBuf,
	char **row, int rowSize);
/* Convert bigBedInterval into an array of chars equivalent to what you'd get by
 * parsing the bed file. The startBuf and endBuf are used to hold the ascii representation of
 * start and end.  Note that the interval->rest string will have zeroes inserted as a side effect. 
 * Returns number of fields in row.  */

boolean bigBedSummaryArray(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues);
/* Fill in summaryValues with  data from indicated chromosome range in bigBed file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */

boolean bigBedSummaryArrayExtended(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	int summarySize, struct bbiSummaryElement *summary);
/* Get extended summary information for summarySize evenely spaced elements into
 * the summary array. */

bits64 bigBedItemCount(struct bbiFile *bbi);
/* Return total items in file. */

struct asObject *bigBedAs(struct bbiFile *bbi);
/* Get autoSql object definition if any associated with file. */

void bigBedFileCreate(
	char *inName, 	  /* Input file in a tabular bed format <chrom><start><end> + whatever. */
	char *chromSizes, /* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,	  /* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot, /* Number of items in lowest level of tree.  64 is good. */
	bits16 definedFieldCount,  /* Number of defined bed fields - 3-16 or so.  0 means all fields
				    * are the defined bed ones. */
	char *asFileName, /* If non-null points to a .as file that describes fields. */
	char *outName);   /* BigBed output file name. */
/* Convert tab-separated bed file to binary indexed, zoomed bigBed version. */

void bigBedFileCreateReadInfile(
	char *inName, 	  /* Input file in a tabular bed format <chrom><start><end> + whatever. */
	char *chromSizes, /* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,	  /* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot, /* Number of items in lowest level of tree.  64 is good. */
	bits16 definedFieldCount,  /* Number of defined bed fields - 3-16 or so.  0 means all fields
				    * are the defined bed ones. */
	char *asFileName, /* If non-null points to a .as file that describes fields. */
	char *outName,    /* BigBed output file name. */
	struct ppBed **ppbList,   /* Input bed data, will be sorted. */
	bits64 *count,            /* size of input pbList */
	double *averageSize,      /* average size of elements in pbList */
	struct hash **pChromHash,  /* Hash containing sizes of all chroms. */
	bits16 *fieldCount,       /* actual field count from input data. */
	struct asObject **pAs,    /* If non-null contains as object that describes fields. */
	bits64 *fullSize);         /* full size of ppBed on disk */
/* Load data to prepare bigBed. */

void bigBedFileCreateDetailed(
	struct ppBed *pbList, 	  /* Input bed data. Must be sorted. */
	bits64 pbCount,           /* size of input pbList */
	double pbAverageSize,     /* average size of elements in pbList */
	char *inName,             /* Input file name (for error message reporting) */
	struct hash *chromHash,   /* Hash containing sizes of all chroms. */
	int blockSize,	  /* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot, /* Number of items in lowest level of tree.  64 is good. */
	bits16 definedFieldCount, /* Number of defined bed fields - 3-16 or so.  0 means all fields
				    * are the defined bed ones. */
	bits16 fieldCount,        /* actual field count from input data. */
	char *asFileName,         /* If non-null points to a .as file that describes fields. */
	struct asObject *as,      /* If non-null contains as object that describes fields. */
	bits64 fullSize,          /* full size of ppBed on disk */
	char *outName);            /* BigBed output file name. */
/* create zoomed bigBed version from ppBed list. */

struct ppBed *ppBedLoadOne(char **row, int fieldCount, struct lineFile *lf, struct hash *chromHash, struct lm *lm, struct asObject *as, bits64 *diskSize);
/* Return a ppBed record from a line of bed file in lf.
   Return the disk size it would occupy in *diskSize.
   row is a preallocated array of pointers to the individual fields in this row to load.
   fieldCount is the number of fields.
   lf is the lineFile the row is coming from, used for error messages and parsing fields.
   chromHash is a hash of the chromosome sizes.
   lm is localMem to allocate ppBed memory from - don't ppBedFreeList or slFree
   list!
   as is the autoSql object describing this bed file or NULL if standard bed.
   */

#endif /* BIGBED_H */

