/* bbiFile - Big Binary Indexed file.  Stuff that's common between bigWig and bigBed. */

#ifndef BBIFILE_H
#define BBIFILE_H

/* bigWig/bigBed file structure:
 *     fixedWidthHeader
 *         magic# 		4 bytes
 *         version              2 bytes
 *	   zoomLevels		2 bytes
 *         chromosomeTreeOffset	8 bytes
 *         fullDataOffset	8 bytes
 *	   fullIndexOffset	8 bytes
 *         fieldCount           2 bytes (bigBed only)
 *         definedFieldCount    2 bytes (bigBed only)
 *         autoSqlOffset             8 bytes (bigBed only)
 *         reserved            20 bytes
 *     zoomHeaders		there are zoomLevels number of these
 *         reductionLevel	4 bytes
 *	   reserved		4 bytes
 *	   dataOffset		8 bytes
 *         indexOffset          8 bytes
 *     autoSql string (zero terminated)
 *     chromosome b+ tree       bPlusTree index
 *     full data
 *         sectionCount		4 bytes (item count for bigBeds)
 *         section data		section count sections, of three types (bed data for bigBeds)
 *     full index               cirTree index
 *     zoom info             one of these for each zoom level
 *         zoom data
 *             zoomCount	4 bytes
 *             zoom data	there are zoomCount of these items
 *                 chromId	4 bytes
 *	           chromStart	4 bytes
 *                 chromEnd     4 bytes
 *                 validCount	4 bytes
 *                 minVal       4 bytes float 
 *                 maxVal       4 bytes float
 *                 sumData      4 bytes float
 *                 sumSquares   4 bytes float
 *         zoom index        	cirTree index
 */


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
    struct udcFile *udc;	/* Open UDC file handle. */
    bits32 typeSig;		/* bigBedSig or bigWigSig for now. */
    boolean isSwapped;		/* If TRUE need to byte swap everything. */
    struct bptFile *chromBpt;	/* Index of chromosomes. */
    bits16 version;		/* Version number - initially 1. */
    bits16 zoomLevels;		/* Number of zoom levels. */
    bits64 chromTreeOffset;	/* Offset to chromosome index. */
    bits64 unzoomedDataOffset;	/* Start of unzoomed data. */
    bits64 unzoomedIndexOffset;	/* Start of unzoomed index. */
    bits16 fieldCount;		/* Number of columns in bed version. */
    bits16 definedFieldCount;   /* Number of columns using bed standard definitions. */
    bits64 asOffset;		/* Offset to embedded null-terminated AutoSQL file. */
    struct cirTreeFile *unzoomedCir;	/* Unzoomed data index in memory - may be NULL. */
    struct bbiZoomLevel *levelList;	/* List of zoom levels. */
    };


struct bbiFile *bbiFileOpen(char *fileName, bits32 sig, char *typeName);
/* Open up big wig or big bed file. */

void bbiFileClose(struct bbiFile **pBwf);
/* Close down a big wig/big bed file. */

struct fileOffsetSize *bbiOverlappingBlocks(struct bbiFile *bbi, struct cirTreeFile *ctf,
	char *chrom, bits32 start, bits32 end, bits32 *retChromId);
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

void bbiChromInfoKey(const void *va, char *keyBuf);
/* Get key field out of bbiChromInfo. */

void *bbiChromInfoVal(const void *va);
/* Get val field out of bbiChromInfo. */


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

#define bbiSummaryFreeList slFreeList


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

struct bbiSummaryElement
/* An element of a summary from the user side. */
    {
    bits64 validCount;		/* Count of (bases) with actual data. */
    double minVal;		/* Minimum value of items */
    double maxVal;		/* Maximum value of items */
    double sumData;		/* sum of values for each base. */
    double sumSquares;		/* sum of squares for each base. */
    };

boolean bbiSummaryArrayExtended(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	BbiFetchIntervals fetchIntervals,
	int summarySize, struct bbiSummaryElement *summary);
/* Fill in summary with  data from indicated chromosome range in bigWig/bigBed file. 
 * Returns FALSE if no data at that position. */

boolean bbiSummaryArray(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	BbiFetchIntervals fetchIntervals,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues);
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */

/****** Write side of things - implemented in bbiWrite.c ********/

void bbiWriteFloat(FILE *f, float val);
/* Write out floating point val to file.  Mostly to convert from double... */

struct hash *bbiChromSizesFromFile(char *fileName);
/* Read two column file into hash keyed by chrom. */

bits64 bbiTotalSummarySize(struct bbiSummary *list);
/* Return size on disk of all summaries. */

void bbiAddToSummary(bits32 chromId, bits32 chromSize, bits32 start, bits32 end, 
	bits32 validCount, double minVal, double maxVal, double sumData, double sumSquares,  
	int reduction, struct bbiSummary **pOutList);
/* Add data range to summary - putting it onto top of list if possible, otherwise
 * expanding list. */

void bbiAddRangeToSummary(bits32 chromId, bits32 chromSize, bits32 start, bits32 end, 
	double val, int reduction, struct bbiSummary **pOutList);
/* Add chromosome range to summary - putting it onto top of list if possible, otherwise
 * expanding list. */

struct bbiSummary *bbiReduceSummaryList(struct bbiSummary *inList, 
	struct bbiChromInfo *chromInfoArray, int reduction);
/* Reduce summary list to another summary list. */

bits64 bbiWriteSummaryAndIndex(struct bbiSummary *summaryList, 
	int blockSize, int itemsPerSlot, FILE *f);
/* Write out summary and index to summary, returning start position of
 * summary index. */

#endif /* BBIFILE_H */
