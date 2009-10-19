/* bamFILE -- interface to binary alignment format files using Heng Li's samtools lib. */

#ifndef BAMFILE_H
#define BAMFILE_H

// bam.h is incomplete without _IOLIB set to 1, 2 or 3.  2 is used by Makefile.generic:
#define _IOLIB 2
#include "bam.h"
#include "sam.h"

void bamIgnoreStrand();
/* Change the behavior of this lib to disregard item strand. 
 * If called, this should be called before any other bam functions. */

char *bamFileNameFromTable(char *db, char *table, char *bamSeqName);
/* Return file name from table.  If table has a seqName column, then grab the 
 * row associated with bamSeqName (which is not nec. in chromInfo, e.g. 
 * bam file might have '1' not 'chr1'). */

boolean bamFileExists(char *bamFileName);
/* Return TRUE if we can successfully open the bam file and its index file. */

void bamFetch(char *bamFileName, char *position, bam_fetch_f callbackFunc, void *callbackData);
/* Open the .bam file, fetch items in the seq:start-end position range,
 * and call callbackFunc on each bam item retrieved from the file plus callbackData. 
 * Note: if sequences in .bam file don't begin with "chr" but cart position does, pass in 
 * cart position + strlen("chr") to match the .bam file sequence names. */

boolean bamIsRc(const bam1_t *bam);
/* Return TRUE if alignment is on - strand.  If bamIgnoreStrand has been called,
 * then this always returns FALSE. */

INLINE int bamUnpackCigarElement(unsigned int packed, char *retOp)
/* Given an unsigned int containing a number of bases and an offset into an
 * array of BAM-enhanced-CIGAR ASCII characters (operations), store operation 
 * char into *retOp (retOp must not be NULL) and return the number of bases. */
{
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
}

char *bamGetQuerySequence(const bam1_t *bam);
/* Return the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so here we rev-comp it back to restore the original query sequence. */

UBYTE *bamGetQueryQuals(const bam1_t *bam);
/* Return the base quality scores encoded in bam as an array of ubytes. */

char *bamGetCigar(const bam1_t *bam);
/* Return a BAM-enhanced CIGAR string, decoded from the packed encoding in bam. */

void bamShowCigarEnglish(const bam1_t *bam);
/* Print out cigar in English e.g. "20 (mis)Match, 1 Deletion, 3 (mis)Match" */

void bamShowFlagsEnglish(const bam1_t *bam);
/* Print out flags in English, e.g. "Mate is on '-' strand; Properly paired". */

int bamGetTargetLength(const bam1_t *bam);
/* Tally up the alignment's length on the reference sequence from
 * bam's packed-int CIGAR representation. */

struct ffAli *bamToFfAli(const bam1_t *bam, struct dnaSeq *target, int targetOffset);
/* Convert from bam to ffAli format. */

bam1_t *bamClone(const bam1_t *bam);
/* Return a newly allocated copy of bam. */

void bamShowTags(const bam1_t *bam);
/* Print out tags in HTML: bold key, no type indicator for brevity. */

char *bamGetTagString(const bam1_t *bam, char *tag, char *buf, size_t bufSize);
/* If bam's tags include the given 2-character tag, place the value into 
 * buf (zero-terminated, trunc'd if nec) and return a pointer to buf,
 * or NULL if tag is not present. */

#endif//ndef BAMFILE_H
