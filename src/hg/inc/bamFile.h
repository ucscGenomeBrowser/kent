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
	     opcode, strlen(BAM_DOT_C_OPCODE_STRING)-1);
*retOp = BAM_DOT_C_OPCODE_STRING[opcode];
return n;
}

char *bamGetQuerySequence(const bam1_t *bam);
/* Return the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so here we rev-comp it back to restore the original query sequence. */

char *bamGetCigar(const bam1_t *bam);
/* Return a BAM-enhanced CIGAR string, decoded from the packed encoding in bam. */

int bamGetTargetLength(const bam1_t *bam);
/* Tally up the alignment's length on the reference sequence from
 * bam's packed-int CIGAR representation. */

#endif//ndef BAMFILE_H
