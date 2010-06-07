/* FeatureBits - convert features tables to bitmaps. */
#ifndef FEATUREBITS_H
#define FEATUREBITS_H

#ifndef BITS_H
#include "bits.h"
#endif
#include "hdb.h"
#include "cart.h"
#include "bed.h"

struct featureBits
/* A part of a sequence. */
    {
    struct featureBits *next;
    char *name;		/* Symbolic name for feature. */
    char *chrom;	/* Chromosome name. */
    int start, end;	/* Start/end half open zero based. */
    char strand;	/* + or - or ? */
    };

void featureBitsFree(struct featureBits **pBits);
/* Free up feature bits. */

void featureBitsFreeList(struct featureBits **pList);
/* Free up a list of featureBits */

struct featureBits *fbGetRange(char *db, char *trackQualifier, char *chrom,
                               int chromStart, int chromEnd);
/* Get features in range. */

struct featureBits *fbGetRangeQuery(char *db, char *trackQualifier,
	char *chrom, int chromStart, int chromEnd, char *sqlConstraints,
	boolean clipToWindow, boolean filterOutNoUTR);
/* Get features in range that match sqlConstraints. */

boolean fbUnderstandTrack(char *db, char *track);
/* Return TRUE if can turn track into a set of ranges or bits. */

void fbOrBits(Bits *bits, int bitSize, struct featureBits *fbList,
	int bitOffset);
/* Or in bits.   Bits should have bitSize bits.  */

void fbOrTableBits(char *db, Bits *bits, char *trackQualifier, char *chrom, 
	int chromSize, struct sqlConnection *conn);
/* Ors in features in track on chromosome into bits.  */

void fbOrTableBitsQueryMinSize(char *db, Bits *bits, char *trackQualifier, char *chrom, 
        int chromSize, struct sqlConnection *conn, char *sqlConstraints,
	boolean clipToWindow, boolean filterOutNoUTR, int minSize);
/* Ors in features matching sqlConstraints in track on chromosome into bits. 
   Skips featureBits that are less than minSize. minSize is useful for introns where
   things less than a given threshold are alignment gaps rather than introns. */

void fbOrTableBitsQuery(char *db, Bits *bits, char *trackQualifier, char *chrom, 
	int chromSize, struct sqlConnection *conn, char *sqlConstraints,
	boolean clipToWindow, boolean filterOutNoUTR);
/* Ors in features matching sqlConstraints in track on chromosome into bits. */



void fbOptions(char *db, char *track);
/* Print out an HTML table with radio buttons for featureBits options. */

void fbOptionsHti(struct hTableInfo *hti);
/* Print out an HTML table with radio buttons for featureBits options.
 * Use defaults from CGI. */

void fbOptionsHtiCart(struct hTableInfo *hti, struct cart *cart);
/* Print out an HTML table with radio buttons for featureBits options. 
 * Use defaults from CGI and cart. */

char *fbOptionsToQualifier();
/* Translate CGI variable created by fbOptions() to a featureBits qualifier. */

struct featureBits *fbFromBed(char *db, char *trackQualifier, struct hTableInfo *hti,
	struct bed *bedList, int chromStart, int chromEnd,
	boolean clipToWindow, boolean filterOutNoUTR);
/* Translate a list of bed items into featureBits. */

struct bed *fbToBedOne(struct featureBits *fb);
/* Translate a featureBits item into (scoreless) bed 6. */

struct bed *fbToBed(struct featureBits *fbList);
/* Translate a list of featureBits items into (scoreless) bed 6. */

void bitsToBed(char *db, Bits *bits, char *chrom, int chromSize, FILE *bed, FILE *fa, 
	       int minSize);
/* Write out runs of bits of at least minSize as items in a bed file. */

#endif /* FEATUREBITS_H */

