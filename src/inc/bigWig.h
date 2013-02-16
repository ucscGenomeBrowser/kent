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

#ifndef BITS_H
#include "bits.h"
#endif

void bigWigFileCreate(
	char *inName, 		/* Input file in ascii wiggle format. */
	char *chromSizes, 	/* Two column tab-separated file: <chromosome> <size>. */
	int blockSize,		/* Number of items to bundle in r-tree.  1024 is good. */
	int itemsPerSlot,	/* Number of items in lowest level of tree.  512 is good. */
	boolean clipDontDie,	/* If TRUE then clip items off end of chrom rather than dying. */
	boolean compress,	/* If TRUE then compress data. */
	char *outName);
/* Convert ascii format wig file (in fixedStep, variableStep or bedGraph format) 
 * to binary big wig format. */

struct bbiFile *bigWigFileOpen(char *fileName);
/* Open up big wig file.   Free this up with bbiFileClose */

#define bigWigFileClose(a) bbiFileClose(a)

struct bbiInterval *bigWigIntervalQuery(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end,
	struct lm *lm);
/* Get data for interval.  Return list allocated out of lm. */

int bigWigIntervalDump(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end, int maxCount,
	FILE *out);
/* Print out info on bigWig parts that intersect chrom:start-end.   Set maxCount to 0 if you 
 * don't care how many are printed.  Returns number printed. */

boolean bigWigSummaryArray(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues);
/* Fill in summaryValues with  data from indicated chromosome range in bigWig file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan(0) depending on the application.)  Returns FALSE if no data
 * at that position. */

boolean bigWigSummaryArrayExtended(struct bbiFile *bwf, char *chrom, bits32 start, bits32 end,
	int summarySize, struct bbiSummaryElement *summary);
/* Get extended summary information for summarySize evenely spaced elements into
 * the summary array. */

double bigWigSingleSummary(struct bbiFile *bwf, char *chrom, int start, int end,
    enum bbiSummaryType summaryType, double defaultVal);
/* Return the summarized single value for a range. */

boolean isBigWig(char *fileName);
/* Peak at a file to see if it's bigWig */

boolean bigWigFileCheckSigs(char *fileName);
/* check file signatures at beginning and end of file */

/* bigWigValsOnChrom - a little system for optimizing bigWig use when doing a pass over the
 * whole chromosome.   How it is used typically is:
 *      struct bigWigValsOnChrom *chromVals = bigWigValsOnChromNew();
 *      for (chrom = chromList; chrom != NULL; chrom = chrom->next)
 *          {
 *          if (bigWigValsOnChromFetchData(chromVals, chrom->name, bigWig))
 *              // do stuff using the valBuf, or covBuf fields which have
 *              // the big wig data unpacked into them. Can use chromSize and chrom too 
 *          }
 *       bigWigValsOnChromFree(&chromVals);  */

struct bigWigValsOnChrom
/* Object for bulk access a chromosome at a time.  This is faster than
 * doing bigWigInterval queries when you have ~3000 or more queries. */
     {
     struct bigWigValsOnChrom *next;
     char *chrom;	/* Current chromosome. */
     long chromSize;	/* Size of current chromosome. */
     long bufSize;	/* Size of allocated buffer */
     double *valBuf;	/* A value for each base on chrom. Zero where no data. */
     Bits *covBuf;	/* A bit for each base with data. */
     };

struct bigWigValsOnChrom *bigWigValsOnChromNew();
/* Allocate new empty bigWigValsOnChromStructure. */

void bigWigValsOnChromFree(struct bigWigValsOnChrom **pChromVals);
/* Free up bigWigValsOnChrom */

boolean bigWigValsOnChromFetchData(struct bigWigValsOnChrom *chromVals, char *chrom, 
	struct bbiFile *bigWig);
/* Fetch data for chromosome from bigWig. Returns FALSE if not data on that chrom. */

#endif /* BIGWIG_H */

