/* FeatureBits - convert features tables to bitmaps. */
#ifndef FEATUREBITS_H
#define FEATUREBITS_H

#ifndef BITS_H
#include "bits.h"
#endif

void fbOrTableBits(Bits *bits, char *trackQualifier, char *chrom, 
	int chromSize, struct sqlConnection *conn);
/* Ors in features in track on chromosome into bits.  */

#endif /* FEATUREBITS_H */

