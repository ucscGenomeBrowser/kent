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

struct dnaSeq
/* A dna sequence in one-character per base format. */
    {
    struct dnaSeq *next;  /* Next in list. */
    char *name;           /* Name of sequence. */
    DNA *dna;             /* Sequence base by base. */
    int size;             /* Size of sequence. */
    };

struct dnaSeq *newDnaSeq(DNA *dna, int size, char *name);
/* Create a new DNA seq. */

void freeDnaSeq(struct dnaSeq **pSeq);
/* Free up DNA seq.  */

void freeDnaSeqList(struct dnaSeq **pSeqList);
/* Free up list of DNA sequences. */

#endif /* DNASEQ_H */

