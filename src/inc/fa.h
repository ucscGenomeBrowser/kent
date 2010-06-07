/* Routines for reading and writing fasta format sequence files.
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef FA_H
#define FA_H

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif

#ifndef LINEFILE_H
#include "linefile.h"
#endif

struct dnaSeq *faReadDna(char *fileName);
/* Open fa file and read a single dna sequence from it. */

aaSeq *faReadAa(char *fileName);
/* Open fa file and read a single dna sequence from it. */

bioSeq *faReadSeq(char *fileName, boolean isDna);
/* Read a dna or protein sequence. */

struct dnaSeq *faReadAllDna(char *fileName);
/* Return list of all DNA sequences in FA file. */

struct dnaSeq *faReadAllPep(char *fileName);
/* Return list of all Peptide sequences in FA file. */

struct dnaSeq *faReadAllSeq(char *fileName, boolean isDna);
/* Return list of all sequences in FA file. */

struct dnaSeq *faReadAllMixed(char *fileName);
/* Read in mixed case fasta file, preserving case. */

struct hash *faReadAllIntoHash(char *fileName, enum dnaCase dnaCase);
/* Return hash of all sequences in FA file.  */

struct dnaSeq *faReadAllMixedInLf(struct lineFile *lf);
/* Read in mixed case sequence from open fasta file. */

struct dnaSeq *faReadOneDnaSeq(FILE *f, char *name, boolean mustStartWithSign);
/* Read one sequence from FA file. Assumes positioned at or before
 * the '>' at start of sequence. */  

boolean faReadNext(FILE *f, char *defaultName, boolean mustStartWithComment, 
    char **retCommentLine, struct dnaSeq **retSeq);
/* Read next sequence from .fa file. Return sequence in retSeq.  If retCommentLine is non-null
 * return the '>' line in retCommentLine.   The whole thing returns FALSE at end of file. 
 * Assumes positioned at or before the '>' at start of sequence.  File must have been
 * opened in binary mode! Note: sequence is mapped to lower case */

boolean faReadMixedNext(FILE *f, boolean preserveCase, char *defaultName, 
    boolean mustStartWithComment, char **retCommentLine, struct dnaSeq **retSeq);
/* Read next sequence from .fa file. Return sequence in retSeq.  If retCommentLine is non-null
 * return the '>' line in retCommentLine.   The whole thing returns FALSE at end of file. Provides flag for preserving case in sequence */

struct dnaSeq *faFromMemText(char *text);
/* Return a sequence from a .fa file that's been read into
 * a string in memory. This cannabalizes text, which should
 * be allocated with needMem.  This buffer becomes part of
 * the returned dnaSeq, which may be freed normally with
 * freeDnaSeq. */

bioSeq *faSeqFromMemText(char *text, boolean isDna);
/* Convert fa in memory to bioSeq. This cannabalizes text
 * as does faFromMemText above. */

bioSeq *faNextSeqFromMemText(char **pText, boolean isDna);
/* Convert fa in memory to bioSeq.  Update *pText to point to next
 * record.  Returns NULL when no more sequences left. */

bioSeq *faNextSeqFromMemTextRaw(char **pText);
/* Same as faNextSeqFromMemText, but will leave in 
 * letters even if they aren't in DNA or protein alphabed. */

bioSeq *faSeqListFromMemText(char *text, boolean isDna);
/* Convert fa's in memory into list of dnaSeqs. */

bioSeq *faSeqListFromMemTextRaw(char *text);
/* Convert fa's in memory into list of dnaSeqs without
 * converting chars to N's. */

boolean faFastReadNext(FILE *f, DNA **retDna, int *retSize, char **retName);
/* Read in next FA entry as fast as we can. Return FALSE at EOF. 
 * The returned DNA and name will be overwritten by the next call
 * to this function. */

boolean faSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName);
/* Read in next FA entry as fast as we can. Faster than that old,
 * pokey faFastReadNext. Return FALSE at EOF. 
 * The returned DNA and name will be overwritten by the next call
 * to this function. */

boolean faPepSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName);
/* Read in next peptide FA entry as fast as we can.  */

boolean faSomeSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName, boolean isDna);
/* Read in DNA or Peptide FA record. */

boolean faMixedSpeedReadNext(struct lineFile *lf, DNA **retDna, int *retSize, char **retName);
/* Read in DNA or Peptide FA record in mixed case.   Allow any upper or lower case
 * letter, or the dash character in. */

void faToProtein(char *poly, int size);
/* Convert possibly mixed-case protein to upper case.  Also
 * convert any strange characters to 'X'.  Does not change size.
 * of sequence. */

void faToDna(char *poly, int size);
/* Convert possibly mixed-case DNA to lower case.  Also turn
 * any strange characters to 'n'.  Does not change size.
 * of sequence. */

void faFreeFastBuf();
/* Free up buffers used in fa fast and speedreading. */

void faWrite(char *fileName, char *startLine, DNA *dna, int dnaSize);
/* Write out FA file or die trying. */

void faWriteNext(FILE *f, char *startLine, DNA *dna, int dnaSize);
/* Write next sequence to fa file. */

void faWriteAll(char *fileName, bioSeq *seqList);
/* Write out all sequences in list to file. */

#endif /* FA_H */
