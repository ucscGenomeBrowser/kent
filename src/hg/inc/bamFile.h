/* bamFILE -- interface to binary alignment format files using Heng Li's samtools lib. */

#ifndef BAMFILE_H
#define BAMFILE_H

// bam.h is incomplete without _IOLIB set to 1, 2 or 3.  2 is used by Makefile.generic:
#define _IOLIB 2
#include "bam.h"
#include "sam.h"

void bamFetch(char *db, char *table, char *position, bam_fetch_f callbackFunc, void *callbackData);
/* Open the .bam file given in db.table, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData. 
 * Note: if sequences in .bam file don't begin with "chr" but db's do, skip the "chr"
 * at the beginning of the position. */

#endif//ndef BAMFILE_H
