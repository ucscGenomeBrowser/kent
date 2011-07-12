/* bamFile -- interface to binary alignment format files using Heng Li's samtools lib. */

#ifndef BAMFILE_H
#define BAMFILE_H

#ifndef BAMUDC_H
#include "bamUdc.h"
#endif

#ifdef USE_BAM

#ifndef SAMALIGNMENT_H
#include "samAlignment.h"
#endif

#endif

char *bamFileNameFromTable(struct sqlConnection *conn, char *table, char *bamSeqName);
/* Return file name from table.  If table has a seqName column, then grab the 
 * row associated with bamSeqName (which can be e.g. '1' not 'chr1' if that is the
 * case in the bam file). */

boolean bamFileExists(char *bamFileName);
/* Return TRUE if we can successfully open the bam file and its index file. */

void bamFetch(char *fileOrUrl, char *position, bam_fetch_f callbackFunc, void *callbackData,
	samfile_t **pSamFile);
/* Open the .bam file, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData.
 * This handles BAM files with "chr"-less sequence names, e.g. from Ensembl. 
 * The pSamFile parameter is optional.  If non-NULL it will be filled in, just for
 * the benefit of the callback function, with the open samFile.  */

struct samAlignment *bamFetchSamAlignment(char *fileOrUrl, char *chrom, int start, int end,
	struct lm *lm);
/* Fetch region as a list of samAlignments - which is more or less an unpacked
 * bam record.  Results is allocated out of lm, since it tends to be large... */

struct samAlignment *bamReadNextSamAlignments(samfile_t *fh, int count, struct lm *lm);
/* Read next count alignments in SAM format, allocated in lm.  May return less than
 * count at end of file. */

samfile_t *bamOpen(char *fileOrUrl, char **retBamFileName);
/* Return an open bam file, dealing with FUSE caching if need be. 
 * Return parameter if NON-null will return the file name after FUSing */

struct ffAli *bamToFfAli(const bam1_t *bam, struct dnaSeq *target, int targetOffset,
			 boolean useStrand, char **retQSeq);
/* Convert from bam to ffAli format.  If retQSeq is non-null, set it to the 
 * query sequence into which ffAli needle pointers point. */

#endif//ndef BAMFILE_H
