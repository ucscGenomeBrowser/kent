/* FeatureBits - convert features tables to bitmaps. */
#ifndef FEATUREBITS_H
#define FEATUREBITS_H

#ifndef BITS_H
#include "bits.h"
#endif

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

struct featureBits *fbGetRange(char *trackQualifier, char *chrom,
    int chromStart, int chromEnd);
/* Get features in range. */

boolean fbUnderstandTrack(char *track);
/* Return TRUE if can turn track into a set of ranges or bits. */

void fbOrTableBits(Bits *bits, char *trackQualifier, char *chrom, 
	int chromSize, struct sqlConnection *conn);
/* Ors in features in track on chromosome into bits.  */

void fbOptions(char *track);
/* Print out an HTML table with radio buttons for featureBits options. */

char *fbOptionsToQualifier();
/* Translate CGI variable created by fbOptions() to a featureBits qualifier. */

#endif /* FEATUREBITS_H */

