/* bwgInternal - stuff to create and use bigWig files.  Generally you'll want to use the
 * simpler interfaces in the bigWig module instead.  This file is good reading though
 * if you want to extend the bigWig interface, or work with bigWig files directly
 * without going through the Kent library. */

#ifndef BIGWIGFILE_H
#define BIGWIGFILE_H

/* bigWig file structure:
 *     fixedWidthHeader
 *         magic# 		4 bytes
 *	   zoomLevels		4 bytes
 *         chromosomeTreeOffset	8 bytes
 *         fullDataOffset	8 bytes
 *	   fullIndexOffset	8 bytes
 *         reserved            32 bytes
 *     zoomHeaders		there are zoomLevels number of these
 *         reductionLevel	4 bytes
 *	   reserved		4 bytes
 *	   dataOffset		8 bytes
 *         indexOffset          8 bytes
 *     chromosome b+ tree       bPlusTree index
 *     full data
 *         sectionCount		4 bytes
 *         section data		section count sections, of three types
 *     full index               ciTree index
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
 *         zoom index        	ciTree index
 */

enum bwgSectionType 
/* Code to indicate section type. */
    {
    bwgTypeBedGraph=1,
    bwgTypeVariableStep=2,
    bwgTypeFixedStep=3,
    };

struct bwgBedGraphItem
/* An bedGraph-type item in a bwgSection. */
    {
    struct bwgBedGraphItem *next;	/* Next in list. */
    bits32 start,end;		/* Range of chromosome covered. */
    float val;			/* Value. */
    };

struct bwgVariableStepItem
/* An variableStep type item in a bwgSection. */
    {
    struct bwgVariableStepItem *next;	/* Next in list. */
    bits32 start;		/* Start position in chromosome. */
    float val;			/* Value. */
    };

struct bwgFixedStepItem
/* An fixedStep type item in a bwgSection. */
    {
    struct bwgFixedStepItem *next;	/* Next in list. */
    float val;			/* Value. */
    };

union bwgItem
/* Union of item pointers for all possible section types. */
    {
    struct bwgBedGraphItem *bedGraph;
    struct bwgVariableStepItem *variableStep;
    struct bwgFixedStepItem *fixedStep;
    };

struct bwgSection
/* A section of a bigWig file - all on same chrom.  This is a somewhat fat data
 * structure used by the bigWig creation code.  See also bwgSection for the
 * structure returned by the bigWig reading code. */
    {
    struct bwgSection *next;		/* Next in list. */
    char *chrom;			/* Chromosome name. */
    bits32 start,end;			/* Range of chromosome covered. */
    enum bwgSectionType type;
    union bwgItem itemList;		/* List of items in this section. */
    bits32 itemStep;			/* Step within item if applicable. */
    bits32 itemSpan;			/* Item span if applicable. */
    bits16 itemCount;			/* Number of items in section. */
    bits32 chromId;			/* Unique small integer value for chromosome. */
    bits64 fileOffset;			/* Offset of section in file. */
    };

struct bwgSectionOnDisk
/* The part of bwgSection that ends up on disk - in the same order as written on disk. */
    {
    bits32 chromId;			/* Unique small integer value for chromosome. */
    bits32 start,end;			/* Range of chromosome covered. */
    bits32 itemStep;			/* Step within item if applicable. */
    bits32 itemSpan;			/* Item span if applicable. */
    UBYTE type;				/* Item type */
    UBYTE reserved;			/* Reserved for future use. */
    bits16 itemCount;			/* Number of items in section. */
    };

struct bwgSection *bwgParseWig(char *fileName, int maxSectionSize, struct lm *lm);
/* Parse out ascii wig file - allocating memory in lm. */

struct bbiChromInfo;		/* Declared in bbiFile.h. */

void bwgDumpSummary(struct bbiSummary *sum, FILE *f);
/* Write out summary info to file. */

struct hash *bwgChromSizesFromFile(char *fileName);
/* Read two column file into hash keyed by chrom. (Here mostly for use
 * with bwgMakeChromInfo.) */

void bigWigChromInfoKey(const void *va, char *keyBuf);
/* Get key field out of bbiChromInfo. */

void *bigWigChromInfoVal(const void *va);
/* Get val field out of bbiChromInfo. */

int bwgAverageResolution(struct bwgSection *sectionList);
/* Return the average resolution seen in sectionList. */

void bwgAttachUnzoomedCir(struct bbiFile *bwf);
/* Make sure unzoomed cir is attached. */

void bwgAddRangeToSummary(bits32 chromId, bits32 chromSize, bits32 start, bits32 end, 
	float val, int reduction, struct bbiSummary **pOutList);
/* Add chromosome range to summary - putting it onto top of list if possible, otherwise
 * expanding list. */

bits64 bwgTotalSummarySize(struct bbiSummary *list);
/* Return size on disk of all summaries. */

struct bbiSummary *bwgReduceSectionList(struct bwgSection *sectionList, 
	struct bbiChromInfo *chromInfoArray, int reduction);
/* Reduce section by given amount. */

struct bbiSummary *bwgReduceSummaryList(struct bbiSummary *inList, 
	struct bbiChromInfo *chromInfoArray, int reduction);
/* Reduce summary list to another summary list. */

bits64 bwgWriteSummaryAndIndex(struct bbiSummary *summaryList, 
	int blockSize, int itemsPerSlot, FILE *f);
/* Write out summary and index to summary, returning start position of
 * summary index. */

#define bwgSummaryFreeList slFreeList

struct bbiFile *bigWigFileOpen(char *fileName);
/* Open up a big wig file. */

void bigWigFileClose(struct bbiFile **pBwf);
/* Close down a big wig file. */


#endif /* BIGWIGFILE_H */
