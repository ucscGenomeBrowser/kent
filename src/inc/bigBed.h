/* bigBed - interface to binary file with bed-style values (that is a bunch of
 * possibly overlapping regions.
 *
 * This shares a lot with the bigWig module. */

#ifndef BIGBED_H
#define BIGBED_H

#include "asParse.h"

#ifndef BBIFILE
#include "bbiFile.h"
#endif

struct bigBedInterval
/* A partially parsed out bed record plus some extra fields.  Use this directly
 * or convert it to an array of characters with bigBedIntervalToRow. */
    {
    struct bigBedInterval *next;	/* Next in list. */
    bits32 start, end;		/* Range inside chromosome - half open zero based. */
    char *rest;			/* Rest of line. May be NULL*/
    bits32 chromId;             /* ID of chromosome.  */
    };

/*** Routines to open & close bigBed files, and to do chromosome range queries on them. ***/

struct bbiFile *bigBedFileOpen(char *fileName);
/* Open up big bed file.   Free this up with bbiFileClose. */

#define bigBedFileClose(a) bbiFileClose(a)

struct bigBedInterval *bigBedIntervalQuery(struct bbiFile *bbi, char *chrom,
	bits32 start, bits32 end, int maxItems, struct lm *lm);
/* Get data for interval.  Return list allocated out of lm.  Set maxItems to maximum
 * number of items to return, or to 0 for all items. */

int bigBedIntervalToRow(struct bigBedInterval *interval, char *chrom, char *startBuf, char *endBuf,
	char **row, int rowSize);
/* Convert bigBedInterval into an array of chars equivalent to what you'd get by
 * parsing the bed file. The startBuf and endBuf are used to hold the ascii representation of
 * start and end.  Note that the interval->rest string will have zeroes inserted as a side effect.
 * Returns number of fields in row.  */

boolean bigBedSummaryArray(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	enum bbiSummaryType summaryType, int summarySize, double *summaryValues);
/* Fill in summaryValues with  data from indicated chromosome range in bigBed file.
 * Be sure to initialize summaryValues to a default value, which will not be touched
 * for regions without data in file.  (Generally you want the default value to either
 * be 0.0 or nan("") depending on the application.)  Returns FALSE if no data
 * at that position. */

boolean bigBedSummaryArrayExtended(struct bbiFile *bbi, char *chrom, bits32 start, bits32 end,
	int summarySize, struct bbiSummaryElement *summary);
/* Get extended summary information for summarySize evenely spaced elements into
 * the summary array. */

/*** Some routines for accessing bigBed items via name. ***/

struct bigBedInterval *bigBedNameQuery(struct bbiFile *bbi, char *name, struct lm *lm);
/* Return list of intervals matching file. These intervals will be allocated out of lm. */

struct bigBedInterval *bigBedMultiNameQuery(struct bbiFile *bbi, char **names, 
    int nameCount, struct lm *lm);
/* Fetch all records matching any of the names. Return list is allocated out of lm. */

int bigBedIntervalToRowLookupChrom(struct bigBedInterval *interval, 
    struct bigBedInterval *prevInterval, struct bbiFile *bbi,
    char *chromBuf, int chromBufSize, char *startBuf, char *endBuf, char **row, int rowSize);
/* Convert bigBedInterval to array of chars equivalend to what you'd get by parsing the
 * bed file.  If you already know what chromosome the interval is on use the simpler
 * bigBedIntervalToRow.  This one will look up the chromosome based on the chromId field
 * of the interval,  which is relatively time consuming.  To avoid doing this unnecessarily
 * pass in a non-NULL prevInterval,  and if the chromId is the same on prevInterval as this,
 * it will avoid the lookup.  The chromBufSize should be at greater or equal to 
 * bbi->chromBpt->keySize+1.  The startBuf and endBuf are used to hold the ascii representation of
 * start and end, and should be 16 bytes.  Note that the interval->rest string will have zeroes 
 * inserted as a side effect.  Returns number of fields in row.  */

void bigBedIntervalListToBedFile(struct bbiFile *bbi, struct bigBedInterval *intervalList, FILE *f);
/* Write out big bed interval list to bed file, looking up chromosome. */

void bigBedAttachNameIndex(struct bbiFile *bbi);
/* Attach name index part of bbiFile to bbi.  Not normally needed to call directly. */

/** Routines to access other data from a bigBed file. */

bits64 bigBedItemCount(struct bbiFile *bbi);
/* Return total items in file. */

char *bigBedAutoSqlText(struct bbiFile *bbi);
/* Get autoSql text if any associated with file.  Do a freeMem of this when done. */

struct asObject *bigBedAs(struct bbiFile *bbi);
/* Get autoSql object definition if any associated with file. */

struct asObject *bigBedAsOrDefault(struct bbiFile *bbi);
// Get asObject associated with bigBed - if none exists in file make it up from field counts.

struct asObject *bigBedFileAsObjOrDefault(char *fileName);
// Get asObject associated with bigBed file, or the default.

boolean bigBedFileCheckSigs(char *fileName);
/* check file signatures at beginning and end of file */

struct bptFile *bigBedOpenExtraIndex(struct bbiFile *bbi, char *fieldName);
/* Return index associated with fieldName.  Aborts if no such index. */

struct slName *bigBedListExtraIndexes(struct bbiFile *bbi);
/* Return list of names of extra indexes beyond primary chrom:start-end one" */

#endif /* BIGBED_H */

