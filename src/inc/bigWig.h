/* bigWig - interface to binary file with wiggle-style values (that is a bunch of
 * small non-overlapping regions each associated with a floating point value. 
 *
 * There are several ways to use this module.   To create a new bigWig file use the
 * bigWigCreate function, which takes as input an ascii file in fixedStep, variableStep
 * or bedGraph format.
 *
 * To get a section of of a bigWig for display on a browser, use the bigWigSummaryArray
 * function, which will fill in an array of doubles with on value for each pixel that
 * you want to display.
 *
 * To read all the data out of a bigWig get the chromosome info with bbiChromList
 * and then fetch all of it for each chromosome using bigWigIntervalQuery.
 *
 * See also the module bbiFile that has a description of they structure of
 * a bigWig file, and lower level routines used to implement this interface.
 */

#ifndef BIGWIG_H
#define BIGWIG_H

#ifndef BBIFILE
#include "bbiFile.h"
#endif


void bigWigFileCreate(
	char *inName, 		/* Input file in ascii wiggle format. */
	char *chromSizes, 	/* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,		/* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot,	/* Number of items in lowest level of tree.  512 is good. */
	boolean clipDontDie,	/* If TRUE then clip items off end of chrom rather than dying. */
	char *outName);
/* Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format. */

struct bbiFile *bigWigFileOpen(char *fileName);
/* Open up big wig file.   Free this up with bbiFileClose */

struct bbiInterval *bigWigIntervalQuery(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end,
	struct lm *lm);
/* Get data for interval.  Return list allocated out of lm. */

int bigWigIntervalDump(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end, int maxCount,
	FILE *out);
/* Print out info on bigWig parts that intersect chrom:start-end.   Set maxCount to 0 if you 
 * don't care how many are printed.  Returns number printed. */

boolean bigWigSummaryArray(char *fileName, char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues);
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan(0) depending on the application.)  Returns FALSE if no data
 * at that position. */

boolean bigWigSummaryArrayExtended(char *fileName, char *chrom, bits32 start, bits32 end,
	int summarySize, struct bbiSummaryElement *summary);
/* Get extended summary information for summarySize evenely spaced elements into
 * the summary array. */

double bigWigSingleSummary(char *fileName, char *chrom, int start, int end,
    enum bbiSummaryType summaryType, double defaultVal);
/* Return the summarized single value for a range. */

#endif /* BIGWIG_H */

