/* bamFile -- interface to binary alignment format files using Heng Li's samtools lib. */

#ifndef BAMFILE_H
#define BAMFILE_H

#ifdef USE_BAM

// bam.h is incomplete without _IOLIB set to 1, 2 or 3.  2 is used by Makefile.generic:
#ifndef _IOLIB
#define _IOLIB 2
#endif
#include "bam.h"
#include "sam.h"

#else // no USE_BAM
typedef struct { } bam1_t;
typedef struct { } samfile_t;
typedef int (*bam_fetch_f)(const bam1_t *b, void *data);

#define COMPILE_WITH_SAMTOOLS "%s: in order to use this functionality you must " \
    "install the samtools library (<A HREF=\"http://samtools.sourceforge.net\" " \
    "TARGET=_BLANK>http://samtools.sourceforge.net</A>) and recompile kent/src with " \
    "USE_BAM=1 in your environment " \
    "(see <A HREF=\"http://genomewiki.ucsc.edu/index.php/Build_Environment_Variables\" " \
    "TARGET=_BLANK>http://genomewiki.ucsc.edu/index.php/Build_Environment_Variables</A>)."

#endif// USE_BAM

#ifndef SAMALIGNMENT_H
#include "samAlignment.h"
#endif

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif

#ifndef JKSQL_H
#include "jksql.h"
#endif 

char *bamFileNameFromTable(struct sqlConnection *conn, char *table, char *bamSeqName);
/* Return file name from table.  If table has a seqName column, then grab the 
 * row associated with bamSeqName (which is not nec. in chromInfo, e.g. 
 * bam file might have '1' not 'chr1'). */

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

char *bamGetQuerySequence(const bam1_t *bam, boolean useStrand);
/* Return the nucleotide sequence encoded in bam.  The BAM format 
 * reverse-complements query sequence when the alignment is on the - strand,
 * so if useStrand is given we rev-comp it back to restore the original query 
 * sequence. */

UBYTE *bamGetQueryQuals(const bam1_t *bam, boolean useStrand);
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

struct ffAli *bamToFfAli(const bam1_t *bam, struct dnaSeq *target, int targetOffset,
			 boolean useStrand, char **retQSeq);
/* Convert from bam to ffAli format.  If retQSeq is non-null, set it to the 
 * query sequence into which ffAli needle pointers point. */

bam1_t *bamClone(const bam1_t *bam);
/* Return a newly allocated copy of bam. */

void bamShowTags(const bam1_t *bam);
/* Print out tags in HTML: bold key, no type indicator for brevity. */

char *bamGetTagString(const bam1_t *bam, char *tag, char *buf, size_t bufSize);
/* If bam's tags include the given 2-character tag, place the value into 
 * buf (zero-terminated, trunc'd if nec) and return a pointer to buf,
 * or NULL if tag is not present. */

#endif//ndef BAMFILE_H
