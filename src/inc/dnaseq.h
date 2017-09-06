/* dnaSeq - stuff to manage DNA sequences. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef DNASEQ_H
#define DNASEQ_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

#ifndef BITS_H
#include "bits.h"
#endif

struct dnaSeq
/* A dna sequence in one-character per base format. */
    {
    struct dnaSeq *next;  /* Next in list. */
    char *name;           /* Name of sequence. */
    DNA *dna;             /* Sequence base by base. */
    int size;             /* Size of sequence. */
    Bits* mask;           /* Repeat mask (optional) */
    };

typedef struct dnaSeq bioSeq;	/* Preferred use if either DNA or protein. */
typedef struct dnaSeq aaSeq;	/* Preferred use if protein. */

struct dnaSeq *newDnaSeq(DNA *dna, int size, char *name);
/* Create a new DNA seq. */

struct dnaSeq *cloneDnaSeq(struct dnaSeq *seq);
/* Duplicate dna sequence in RAM. */

void freeDnaSeq(struct dnaSeq **pSeq);
/* Free up DNA seq.  */
#define dnaSeqFree freeDnaSeq

void freeDnaSeqList(struct dnaSeq **pSeqList);
/* Free up list of DNA sequences. */
#define dnaSeqFreeList freeDnaSeqList

char *dnaSeqCannibalize(struct dnaSeq **pSeq);
/* Return the already-allocated dna string and free the dnaSeq container. */

aaSeq *translateSeqN(struct dnaSeq *inSeq, unsigned offset, unsigned size, boolean stop);
/* Return a translated sequence.  Offset is position of first base to
 * translate. If size is 0 then use length of inSeq. */

aaSeq *translateSeq(struct dnaSeq *inSeq, unsigned offset, boolean stop);
/* Return a translated sequence.  Offset is position of first base to
 * translate. If stop is TRUE then stop at first stop codon.  (Otherwise 
 * represent stop codons as 'Z'). */

void aaSeqZToX(aaSeq *aa);
/* If seq has a 'Z' for stop codon, possibly followed by other bases, change the 'Z' to an X
 * (compatible with dnautil's aminoAcidTable) and truncate there. */

boolean seqIsDna(bioSeq *seq);
/* Make educated guess whether sequence is DNA or protein. */

boolean seqIsLower(bioSeq *seq);
/* Return TRUE if sequence is all lower case. */

bioSeq *whichSeqIn(bioSeq **seqs, int seqCount, char *letters);
/* Figure out which if any sequence letters is in. */

Bits *maskFromUpperCaseSeq(bioSeq *seq);
/* Allocate a mask for sequence and fill it in based on
 * sequence case. */

struct hash *dnaSeqHash(struct dnaSeq *seqList);
/* Return hash of sequences keyed by name. */

int dnaSeqCmpName(const void *va, const void *vb);
/* Compare to sort based on sequence name. */

#endif /* DNASEQ_H */

