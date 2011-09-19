/* bamFile -- interface to binary alignment format files using Heng Li's samtools lib. */

#ifndef BAMFILE_H
#define BAMFILE_H

#include "dnaseq.h"
#include "dystring.h"

#ifdef USE_BAM

// bam.h is incomplete without _IOLIB set to 1, 2 or 3.  2 is used by Makefile.generic:
#ifndef _IOLIB
#define _IOLIB 2
#endif
#include "bam.h"
#include "sam.h"

#else // no USE_BAM
typedef struct { } bam1_t;
typedef struct { } bam_index_t;
typedef struct { } samfile_t;
typedef int (*bam_fetch_f)(const bam1_t *b, void *data);

#define COMPILE_WITH_SAMTOOLS "%s: in order to use this functionality you must " \
    "install the samtools library (<A HREF=\"http://samtools.sourceforge.net\" " \
    "TARGET=_BLANK>http://samtools.sourceforge.net</A>) and recompile kent/src with " \
    "USE_BAM=1 in your environment " \
    "(see <A HREF=\"http://genomewiki.ucsc.edu/index.php/Build_Environment_Variables\" " \
    "TARGET=_BLANK>http://genomewiki.ucsc.edu/index.php/Build_Environment_Variables</A>)."

#endif // USE_BAM


boolean bamFileExists(char *bamFileName);
/* Return TRUE if we can successfully open the bam file and its index file. */

samfile_t *bamOpen(char *fileOrUrl, char **retBamFileName);
/* Return an open bam file as well as the filename of the bam. */

void bamFetchAlreadyOpen(samfile_t *samfile, bam_index_t *idx, char *bamFileName, 
			 char *position, bam_fetch_f callbackFunc, void *callbackData);
/* With the open bam file, return items the same way with the callbacks as with bamFetch() */
/* except in this case use an already-open bam file and index (use bam_index_load and free() for */
/* the index). It seems a little strange to pass the filename in with the open bam, but */
/* it's just used to report errors. */

void bamFetch(char *fileOrUrl, char *position, bam_fetch_f callbackFunc, void *callbackData,
	samfile_t **pSamFile);
/* Open the .bam file, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData.
 * This handles BAM files with "chr"-less sequence names, e.g. from Ensembl. 
 * The pSamFile parameter is optional.  If non-NULL it will be filled in, just for
 * the benefit of the callback function, with the open samFile.  */

void bamClose(samfile_t **pSamFile);
/* Close down a samefile_t */

boolean bamIsRc(const bam1_t *bam);
/* Return TRUE if alignment is on - strand. */

INLINE int bamUnpackCigarElement(unsigned int packed, char *retOp)
/* Given an unsigned int containing a number of bases and an offset into an
 * array of BAM-enhanced-CIGAR ASCII characters (operations), store operation 
 * char into *retOp (retOp must not be NULL) and return the number of bases. */
{
#ifdef USE_BAM
// decoding lifted from samtools bam.c bam_format1(), long may it remain stable:
#define BAM_DOT_C_OPCODE_STRING "MIDNSHP"
int n = packed>>BAM_CIGAR_SHIFT;
int opcode = packed & BAM_CIGAR_MASK;
if (opcode >= strlen(BAM_DOT_C_OPCODE_STRING))
    errAbort("bamUnpackCigarElement: unrecognized opcode %d. "
	     "(I only recognize 0..%lu [" BAM_DOT_C_OPCODE_STRING "])  "
	     "Perhaps samtools bam.c's bam_format1 encoding changed?  If so, update me.",
	     opcode, (unsigned long)(strlen(BAM_DOT_C_OPCODE_STRING)-1));
*retOp = BAM_DOT_C_OPCODE_STRING[opcode];
return n;
#else // no USE_BAM
errAbort(COMPILE_WITH_SAMTOOLS, "bamUnpackCigarElement");
return 0;
#endif// USE_BAM
}

void bamGetSoftClipping(const bam1_t *bam, int *retLow, int *retHigh, int *retClippedQLen);
/* If retLow is non-NULL, set it to the number of "soft-clipped" (skipped) bases at
 * the beginning of the query sequence and quality; likewise for retHigh at end.
 * For convenience, retClippedQLen is the original query length minus soft clipping
 * (and the length of the query sequence that will be returned). */

void bamUnpackQuerySequence(const bam1_t *bam, boolean useStrand, char *qSeq);
/* Fill in qSeq with the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so if useStrand is given we rev-comp it back to restore the original query 
 * sequence. */

char *bamGetQuerySequence(const bam1_t *bam, boolean useStrand);
/* Return the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so if useStrand is given we rev-comp it back to restore the original query 
 * sequence. */

UBYTE *bamGetQueryQuals(const bam1_t *bam, boolean useStrand);
/* Return the base quality scores encoded in bam as an array of ubytes. */

void bamUnpackCigar(const bam1_t *bam, struct dyString *dyCigar);
/* Unpack CIGAR string into dynamic string */

char *bamGetCigar(const bam1_t *bam);
/* Return a BAM-enhanced CIGAR string, decoded from the packed encoding in bam. */

void bamShowCigarEnglish(const bam1_t *bam);
/* Print out cigar in English e.g. "20 (mis)Match, 1 Deletion, 3 (mis)Match" */

void bamShowFlagsEnglish(const bam1_t *bam);
/* Print out flags in English, e.g. "Mate is on '-' strand; Properly paired". */

int bamGetTargetLength(const bam1_t *bam);
/* Tally up the alignment's length on the reference sequence from
 * bam's packed-int CIGAR representation. */

bam1_t *bamClone(const bam1_t *bam);
/* Return a newly allocated copy of bam. */

void bamShowTags(const bam1_t *bam);
/* Print out tags in HTML: bold key, no type indicator for brevity. */

char *bamGetTagString(const bam1_t *bam, char *tag, char *buf, size_t bufSize);
/* If bam's tags include the given 2-character tag, place the value into 
 * buf (zero-terminated, trunc'd if nec) and return a pointer to buf,
 * or NULL if tag is not present. */

void bamUnpackAux(const bam1_t *bam, struct dyString *dy);
/* Unpack the tag:type:val part of bam into dy */

#endif//ndef BAMFILE_H
