/* bigBed - interface to binary file with bed-style values (that is a bunch of
 * possibly overlapping regions.
 *
 * This shares a lot with the bigWig module. */

#ifndef BIGBED_H
#define BIGBED_H

struct bigBedInterval
/* A partially parsed out bed record plus some extra fields. */
    {
    struct bigBedInterval *next;	/* Next in list. */
    bits32 start, end;		/* Range inside chromosome - half open zero based. */
    char *rest;			/* Rest of line. May be NULL*/
    };

struct bigWigFile;	/* Declare here but real declaration is in bgwInternal.h. */

struct bigWigFile *bigBedFileOpen(char *fileName);
/* Open up big bed file.  Note - this is the same return structure as bigWigFileOpen,
 * and can be used in most (but not all) bigWig functions.  All but the bigWigIntervalQuery
 * and the bigWigSummaryArray. Use the bigBed equivalents for these - defined below. */

#define bigBedFileClose bigWigFileClose

struct bigBedInterval *bigBedIntervalQuery(struct bigWigFile *bwf, char *chrom, int start, int end,
	struct lm *lm);
/* Get data for interval.  Return list allocated out of lm. */

boolean bigBedSummaryArray(char *fileName, char *chrom, bits32 start, bits32 end,
	enum bigWigSummaryType summaryType, int summarySize, double *summaryValues);
/* Fill in summaryValues with  data from indicated chromosome range in bigBed file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */

#endif /* BIGBED_H */

