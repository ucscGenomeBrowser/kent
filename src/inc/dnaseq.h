/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
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

aaSeq *translateSeq(struct dnaSeq *inSeq, int offset, boolean stop);
/* Return a translated sequence.  Offset is position of first base to
 * translate. If stop is TRUE then stop at first stop codon.  (Otherwise 
 * represent stop codons as 'Z'). */

boolean seqIsDna(bioSeq *seq);
/* Make educated guess whether sequence is DNA or protein. */

bioSeq *whichSeqIn(bioSeq **seqs, int seqCount, char *letters);
/* Figure out which if any sequence letters is in. */

Bits *maskFromUpperCaseSeq(bioSeq *seq);
/* Allocate a mask for sequence and fill it in based on
 * sequence case. */

#endif /* DNASEQ_H */

