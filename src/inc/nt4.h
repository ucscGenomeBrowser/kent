/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
#ifndef NT4_H
#define NT4_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

struct nt4Seq
/* A packed (2 bits per nucleotide) sequence.  'N's are
 * converted to 'T's. */
    {
    struct nt4Seq *next;	/* Next in list. */
    bits32 *bases;              /* Packed bases. */
    int baseCount;              /* Number of bases. */
    char *name;                 /* Name of sequence. */
    };

struct nt4Seq *newNt4(DNA *dna, int size, char *name);
/* Create a new DNA seq with 2 bits per base pair. */

void freeNt4(struct nt4Seq **pSeq);
/* Free up DNA seq with 2 bits per base pair */

struct nt4Seq *loadNt4(char *fileName, char *seqName);
/* Load up an nt4 sequence from a file. */

void  saveNt4(char *fileName, DNA *dna, bits32 dnaSize);
/* Save dna in an NT4 file. */

int nt4BaseCount(char *fileName);
/* Return number of bases in NT4 file. */

DNA *nt4Unpack(struct nt4Seq *n, int start, int size);
/* Create an unpacked section of nt4 sequence.  */

DNA *nt4LoadPart(char *nt4FileName, int start, int size);
/* Load part of an nt4 file. */

#endif /* _4NT_H */

