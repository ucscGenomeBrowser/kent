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

struct bwgSection *bwgParseWig(char *fileName, int maxSectionSize, struct lm *lm);
/* Parse out ascii wig file - allocating memory in lm. */

struct bwgSummary
/* A summary type item. */
    {
    struct bwgSummary *next;
    bits32 chromId;		/* ID of associated chromosome. */
    bits32 start,end;		/* Range of chromosome covered. */
    bits32 validCount;		/* Count of (bases) with actual data. */
    float minVal;		/* Minimum value of items */
    float maxVal;		/* Maximum value of items */
    float sumData;		/* sum of values for each base. */
    float sumSquares;		/* sum of squares for each base. */
    bits64 fileOffset;		/* Offset of summary in file. */
    };

void bwgDumpSummary(struct bwgSummary *sum, FILE *f);
/* Write out summary info to file. */

struct hash *bwgChromSizesFromFile(char *fileName);
/* Read two column file into hash keyed by chrom. (Here mostly for use
 * with bwgMakeChromInfo.) */

struct bigWigChromInfo;

void bwgMakeChromInfo(struct bwgSection *sectionList, struct hash *chromSizeHash,
	int *retChromCount, struct bigWigChromInfo **retChromArray,
	int *retMaxChromNameSize);
/* Fill in chromId field in sectionList.  Return array of chromosome name/ids. 
 * The chromSizeHash is keyed by name, and has int values. */

int bwgAverageResolution(struct bwgSection *sectionList);
/* Return the average resolution seen in sectionList. */

struct bwgSummary *bwgReduceSectionList(struct bwgSection *sectionList, 
	struct bigWigChromInfo *chromInfoArray, int reduction);
/* Reduce section by given amount. */

#define bwgSummaryFreeList slFreeList


struct bigWigFile *bigWigFileOpen(char *fileName);
/* Open up a big wig file. */

void bigWigFileClose(struct bigWigFile **pBwf);
/* Close down a big wig file. */


#endif /* BIGWIGFILE_H */
