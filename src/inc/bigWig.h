/* bigWig - interface to binary file with wiggle-style values (that is a bunch of
 * small non-overlapping regions each associated with a floating point value. 
 *
 * There are several ways to use this module.   To create a new bigWig file use the
 * bigWigCreate function, which takes as input an ascii file in fixedStep, variableStep
 * or bedGraph format.
 *
 * To get a section of of a bigWig for display on a browser, use the bigWigSummaryValues
 * function, which will fill in an array of doubles with on value for each pixel that
 * you want to display.
 *
 * To read all the data out of a bigWig you use bigWigChromData, which fetches
 * the data one chromosome at a time in a uniform large format.  Use bigWigChromList
 * to get a list of all chromosomes represented in the file. 
 *
 * See also the module bwgInternal that has a description of they structure of
 * a bigWig file, and lower level routines used to implement this interface.
 */

#ifndef BIGWIG_H
#define BIGWIG_H


void bigWigFileCreate(
	char *inName, 		/* Input file in ascii wiggle format. */
	char *chromSizes, 	/* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,		/* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot,	/* Number of items in lowest level of tree.  512 is good. */
	char *outName);
/* Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format. */

struct bigWigFile;	/* Just an anonymous pointer from interface point of view. */

struct bigWigFile *bigWigFileOpen(char *fileName);
/* Open up a big wig file. */

void bigWigFileClose(struct bigWigFile **pBwf);
/* Close down a big wig file. */

struct bigWigInterval
/* Data on a single interval. */
    {
    struct bigWigInterval *next;	/* Next in list. */
    bits32 start, end;			/* Position in chromosome, half open. */
    double val;				/* Value at that position. */
    };

struct bigWigInterval *bigWigIntervalQuery(struct bigWigFile *bwf, char *chrom, int start, int end,
	struct lm *lm);
/* Get data for interval.  Return list allocated out of lm. */

struct bigWigChromInfo
/* Pair of a name and a 32-bit integer. Used to assign IDs to chromosomes. */
    {
    struct bigWigChromInfo *next;
    char *name;		/* Chromosome name */
    bits32 id;		/* Chromosome ID - a small number usually */
    bits32 size;	/* Chromosome size in bases */
    };

struct bigWigChromInfo *bigWigChromList(struct bigWigFile *bwf);
/* Return all chromosomes in file.  Dispose of this with bigWigChromInfoFreeList. */

void bigWigChromInfoFreeList(struct bigWigChromInfo **pList);
/* Free a list of bigWigChromInfo's */

bits32 bigWigChromSize(struct bigWigFile *bwf, char *chrom);
/* Returns size of given chromosome. */

enum bigWigSummaryType
/* Way to summarize data. */
    {
    bigWigSumMean = 0,	/* Average value */
    bigWigSumMax = 1,	/* Maximum value */
    bigWigSumMin = 2,	/* Minimum value */
    bigWigSumDataCoverage = 3,  /* Bases in region containing actual data. */
    };

boolean bigWigSummaryArray(char *fileName, char *chrom, bits32 start, bits32 end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues);
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan(0) depending on the application.)  Returns FALSE if no data
 * at that position. */

double bigWigSingleSummary(char *fileName, char *chrom, int start, int end,
    enum bigWigSummaryType summaryType, double defaultVal);
/* Return the summarized single value for a range. */

#endif /* BIGWIG_H */

