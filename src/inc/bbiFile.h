/* bbiFile - Big Binary Indexed file.  Stuff that's common between bigWig and bigBed. */

#ifndef BBIFILE_H
#define BBIFILE_H

#include "cirTree.h"

/* bigWig/bigBed file structure:
 *     fixedWidthHeader
 *         magic# 		4 bytes
 *         version              2 bytes
 *	   zoomLevels		2 bytes
 *         chromosomeTreeOffset	8 bytes
 *         fullDataOffset	8 bytes
 *	   fullIndexOffset	8 bytes
 *         fieldCount           2 bytes (for bigWig 0)
 *         definedFieldCount    2 bytes (for bigWig 0)
 *         autoSqlOffset        8 bytes (for bigWig 0) (0 if no autoSql information)
 *         totalSummaryOffset   8 bytes (0 in earlier versions of file lacking totalSummary)
 *         uncompressBufSize    4 bytes (Size of uncompression buffer.  0 if uncompressed.)
 *         reserved             8 bytes (0 for now)
 *     zoomHeaders		there are zoomLevels number of these
 *         reductionLevel	4 bytes
 *	   reserved		4 bytes
 *	   dataOffset		8 bytes
 *         indexOffset          8 bytes
 *     autoSql string (zero terminated - only present if autoSqlOffset non-zero)
 *     totalSummary - summary of all data in file - only present if totalSummaryOffset non-zero
 *         basesCovered        8 bytes
 *         minVal              8 bytes float (for bigBed minimum depth of coverage)
 *         maxVal              8 bytes float (for bigBed maximum depth of coverage)
 *         sumData             8 bytes float (for bigBed sum of coverage)
 *         sumSquared          8 bytes float (for bigBed sum of coverage squared)
 *     chromosome b+ tree       bPlusTree index
 *     full data
 *         sectionCount		8 bytes (item count for bigBeds)
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
 *     magic# 		4 bytes - same as magic number at start of header
 */

#ifndef CIRTREE_H
#include "cirTree.h"
#endif

#define bbiCurrentVersion 4
/* Version history (of file format, not utilities - corresponds to version field in header)
 *    1 - Initial release
 *    1 - Unfortunately when attempting a transparent change to encoders, made the sectionCount 
 *        field inconsistent, sometimes not present, sometimes 32 bits.  Since offset positions
 *        in index were still accurate this did not break most applications, but it did show
 *        up in the summary section of the Table Browser.
 *    2 - Made sectionCount consistently 64 bits. Also fixed missing zoomCount in first level of
 *        zoom in files made by bedToBigBed and bedGraphToBigWig.  (The older wigToBigWig was fine.)
 *        Added totalSummary section.
 *    3 - Adding zlib compression.  Only active if uncompressBufSize is non-zero in header.
 *    4 - Fixed problem in encoder for the max field in zoom levels higher than the first one.
 *        Added an extra sig at end of file.
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
    bits64 totalSummaryOffset;	/* Offset to total summary information if any.  (On older files have to calculate) */
    bits32 uncompressBufSize;	/* Size of uncompression buffer, 0 if uncompressed */
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

struct bbiChromUsage
/* Information on how many items per chromosome etc.  Used by multipass bbiFile writers. */
    {
    struct bbiChromUsage *next;
    char *name;	/* chromosome name. */
    bits32 itemCount;	/* Number of items for this chromosome. */
    bits32 id;	/* Unique ID for chromosome. */
    bits32 size;	/* Size of chromosome. */
    };


enum bbiSummaryType
/* Way to summarize data. */
    {
    bbiSumMean = 0,	/* Average value */
    bbiSumMax = 1,	/* Maximum value */
    bbiSumMin = 2,	/* Minimum value */
    bbiSumCoverage = 3,  /* Bases in region containing actual data. */
    bbiSumStandardDeviation = 4, /* Standard deviation in window. */
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

struct bbiSummaryElement bbiTotalSummary(struct bbiFile *bbi);
/* Return summary of entire file! */

/****** Write side of things - implemented in bbiWrite.c ********/

struct bbiBoundsArray
/* Minimum info needed for r-tree indexer - where a section lives on disk and the
 * range it covers. */
    {
    bits64 offset;		/* Offset within file. */
    struct cirTreeRange range;	/* What is covered. */
    };

struct cirTreeRange bbiBoundsArrayFetchKey(const void *va, void *context);
/* Fetch bbiBoundsArray key for r-tree */

bits64 bbiBoundsArrayFetchOffset(const void *va, void *context);
/* Fetch bbiBoundsArray file offset for r-tree */

struct bbiSumOutStream
/* Buffer output to file so have a chance to compress. */
    {
    struct bbiSummaryOnDisk *array;
    int elCount;
    int allocCount;
    FILE *f;
    boolean doCompress;
    };

struct bbiSumOutStream *bbiSumOutStreamOpen(int allocCount, FILE *f, boolean doCompress);
/* Open new bbiSumOutStream. */

void bbiSumOutStreamClose(struct bbiSumOutStream **pStream);
/* Free up bbiSumOutStream */

void bbiSumOutStreamWrite(struct bbiSumOutStream *stream, struct bbiSummary *sum);
/* Write out next one to stream. */

void bbiOutputOneSummaryFurtherReduce(struct bbiSummary *sum, 
	struct bbiSummary **pTwiceReducedList, 
	int doubleReductionSize, struct bbiBoundsArray **pBoundsPt, 
	struct bbiBoundsArray *boundsEnd, bits32 chromSize, struct lm *lm, 
	struct bbiSumOutStream *stream);
/* Write out sum to file, keeping track of minimal info on it in *pBoundsPt, and also adding
 * it to second level summary. */

struct bbiSummary *bbiSummarySimpleReduce(struct bbiSummary *list, int reduction, struct lm *lm);
/* Do a simple reduction - where among other things the reduction level is an integral
 * multiple of the previous reduction level, and the list is sorted. Allocate result out of lm. */

#define bbiMaxZoomLevels 10	/* Maximum zoom levels produced by writers. */

void bbiWriteDummyHeader(FILE *f);
/* Write out all-zero header, just to reserve space for it. */

void bbiWriteDummyZooms(FILE *f);
/* Write out zeroes to reserve space for ten zoom levels. */

void bbiSummaryElementWrite(FILE *f, struct bbiSummaryElement *sum);
/* Write out summary element to file. */

void bbiWriteChromInfo(struct bbiChromUsage *usageList, int blockSize, FILE *f);
/* Write out information on chromosomes to file. */

void bbiWriteFloat(FILE *f, float val);
/* Write out floating point val to file.  Mostly to convert from double... */

struct hash *bbiChromSizesFromFile(char *fileName);
/* Read two column file into hash keyed by chrom. */

bits64 bbiTotalSummarySize(struct bbiSummary *list);
/* Return size on disk of all summaries. */

void bbiChromUsageFree(struct bbiChromUsage **pUsage);
/* free a single bbiChromUsage structure */

void bbiChromUsageFreeList(struct bbiChromUsage **pList);
/* free a list of bbiChromUsage structures */

struct bbiChromUsage *bbiChromUsageFromBedFile(struct lineFile *lf, 
	struct hash *chromSizesHash, int *retMinDiff, double *retAveSize, bits64 *retBedCount);
/* Go through bed file and collect chromosomes and statistics. Free with bbiChromUsageFreeList */

int bbiCountSectionsNeeded(struct bbiChromUsage *usageList, int itemsPerSlot);
/* Count up number of sections needed for data. */

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
	int blockSize, int itemsPerSlot, boolean doCompress, FILE *f);
/* Write out summary and index to summary, returning start position of
 * summary index. */

boolean bbiFileCheckSigs(char *fileName, bits32 sig, char *typeName);
/* check file signatures at beginning and end of file */

time_t bbiUpdateTime(struct bbiFile *bbi);
/* return bbi->udc->updateTime */

#endif /* BBIFILE_H */
