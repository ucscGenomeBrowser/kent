/* bbiFile - Big Binary Indexed file.  Stuff that's common between bigWig and bigBed. */

#ifndef BBIFILE_H
#define BBIFILE_H

struct bbiZoomLevel
/* A zoom level in bigWig file. */
    {
    struct bbiZoomLevel *next;		/* Next in list. */
    bits32 reductionLevel;		/* How many bases per item */
    bits32 reserved;			/* Zero for now. */
    bits64 dataOffset;			/* Offset of data for this level in file. */
    bits64 indexOffset;			/* Offset of index for this level in file. */
    };

struct bbiZoomLevel *bbiBestZoom(struct bbiZoomLevel *levelList, int desiredReduction);
/* Return zoom level that is the closest one that is less than or equal to 
 * desiredReduction. */

struct bbiFile 
/* An open bbiFile */
    {
    struct bbiFile *next;	/* Next in list. */
    char *fileName;		/* Name of file - for better error reporting. */
    FILE *f;			/* Open file handle. */
    bits32 typeSig;		/* bigBedSig or bigWigSig for now. */
    boolean isSwapped;		/* If TRUE need to byte swap everything. */
    struct bptFile *chromBpt;	/* Index of chromosomes. */
    bits32 zoomLevels;		/* Number of zoom levels. */
    bits64 chromTreeOffset;	/* Offset to chromosome index. */
    bits64 unzoomedDataOffset;	/* Start of unzoomed data. */
    bits64 unzoomedIndexOffset;	/* Start of unzoomed index. */
    struct cirTreeFile *unzoomedCir;	/* Unzoomed data index in memory - may be NULL. */
    struct bbiZoomLevel *levelList;	/* List of zoom levels. */
    };


struct bbiFile *bbiFileOpen(char *fileName, bits32 sig, char *typeName);
/* Open up big wig or big bed file. */

void bbiFileClose(struct bbiFile **pBwf);
/* Close down a big wig/big bed file. */

struct fileOffsetSize *bbiOverlappingBlocks(struct bbiFile *bbi, struct cirTreeFile *ctf,
	char *chrom, bits32 start, bits32 end);
/* Fetch list of file blocks that contain items overlapping chromosome range. */
 
struct bbiChromIdSize
/* We store an id/size pair in chromBpt bPlusTree */
    {
    bits32 chromId;	/* Chromosome ID */
    bits32 chromSize;	/* Chromosome Size */
    };

struct bbiChromInfo
/* Pair of a name and a 32-bit integer. Used to assign IDs to chromosomes. */
    {
    struct bbiChromInfo *next;
    char *name;		/* Chromosome name */
    bits32 id;		/* Chromosome ID - a small number usually */
    bits32 size;	/* Chromosome size in bases */
    };

struct bbiChromInfo *bbiChromList(struct bbiFile *bbi);
/* Return all chromosomes in file.  Dispose of this with bbiChromInfoFreeList. */

void bbiChromInfoFreeList(struct bbiChromInfo **pList);
/* Free a list of bbiChromInfo's */

bits32 bbiChromSize(struct bbiFile *bbi, char *chrom);
/* Returns size of given chromosome. */

enum bbiSummaryType
/* Way to summarize data. */
    {
    bbiSumMean = 0,	/* Average value */
    bbiSumMax = 1,	/* Maximum value */
    bbiSumMin = 2,	/* Minimum value */
    bbiSumCoverage = 3,  /* Bases in region containing actual data. */
    };

enum bbiSummaryType bbiSummaryTypeFromString(char *string);
/* Return summary type given a descriptive string. */

char *bbiSummaryTypeToString(enum bbiSummaryType type);
/* Convert summary type from enum to string representation. */

struct bbiSummary
/* A summary type item. */
    {
    struct bbiSummary *next;
    bits32 chromId;		/* ID of associated chromosome. */
    bits32 start,end;		/* Range of chromosome covered. */
    bits32 validCount;		/* Count of (bases) with actual data. */
    float minVal;		/* Minimum value of items */
    float maxVal;		/* Maximum value of items */
    float sumData;		/* sum of values for each base. */
    float sumSquares;		/* sum of squares for each base. */
    bits64 fileOffset;		/* Offset of summary in file. */
    };

struct bbiSummaryOnDisk
/* The part of the summary that ends up on disk - in the same order written to disk. */
    {
    bits32 chromId;		/* ID of associated chromosome. */
    bits32 start,end;		/* Range of chromosome covered. */
    bits32 validCount;		/* Count of (bases) with actual data. */
    float minVal;		/* Minimum value of items */
    float maxVal;		/* Maximum value of items */
    float sumData;		/* sum of values for each base. */
    float sumSquares;		/* sum of squares for each base. */
    };

boolean bbiSummaryArrayFromZoom(struct bbiZoomLevel *zoom, struct bbiFile *bbi, 
	char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues);
/* Look up region in index and get data at given zoom level.  Summarize this data
 * in the summaryValues array.  Only update summaryValues we actually do have data. */

struct bbiInterval
/* Data on a single interval. */
    {
    struct bbiInterval *next;	/* Next in list. */
    bits32 start, end;			/* Position in chromosome, half open. */
    double val;				/* Value at that position. */
    };

typedef struct bbiInterval *(*BbiFetchIntervals)(struct bbiFile *bbi, char *chrom, 
					    bits32 start, bits32 end, struct lm *lm);
/* A callback function that returns a bbiInterval list. */

void bbiAttachUnzoomedCir(struct bbiFile *bbi);
/* Make sure unzoomed cir is attached. */

boolean bbiSummaryArray(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	BbiFetchIntervals fetchIntervals,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues);
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */

#endif /* BBIFILE_H */
