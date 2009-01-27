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

/***** Stuff to get stuff out of an existing big wig file. *****/

enum bigWigSummaryType
/* Way to summarize data. */
    {
    bigWigSumMean = 0,	/* Average value */
    bigWigSumMax = 1,	/* Maximum value */
    bigWigSumMin = 2,	/* Minimum value */
    };

boolean bigWigSummaryValues(char *fileName, char *chrom, int start, int end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues);
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan(0) depending on the application.)  Returns FALSE if no data
 * at that position. */

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

struct bigWigInterval *bigWigChromData(struct bigWigFile *bwf, char *chrom, struct lm *lm);
/* Get all data for a chromosome. The returned list will be allocated in lm. */

struct slName *bigWigChromList(struct bigWigFile *bwf);
/* Return all chromosomes in file.  Dispose of this with slFree. */

#endif /* BIGWIG_H */

