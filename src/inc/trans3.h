/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* trans3 - a sequence and three translated reading frames. 
 * In the gfServer/gfClient system these are found in a
 * t3Hash that has as values lists of trans3.  These reflect
 * the fragments of the genome actually loaded in memory
 * to perform the alignment. */

#ifndef TRANS3_H
#define TRANS3_H

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif

#ifndef HASH_H
#include "hash.h"
#endif

struct trans3
/* A sequence and three translations of it. */
     {
     struct trans3 *next;		/* Next in list. */
     char *name;			/* Name (not allocated here) */
     struct dnaSeq *seq;		/* Untranslated sequence.  Not allocated here. */
     aaSeq *trans[3];			/* Translated sequences.  Allocated here*/
     int start,end;			/* Start/end of sequence in a larger context. */
     int nibSize;			/* Size of nib file this is embedded in. */
     boolean isRc;			/* Has been reverse complemented? */
     };

struct trans3 *trans3New(struct dnaSeq *seq);
/* Create a new set of translated sequences. */

void trans3Free(struct trans3 **pT3);
/* Free a trans3 structure. */

void trans3FreeList(struct trans3 **pList);
/* Free a list of dynamically allocated trans3's */

struct trans3 *trans3Find(struct hash *t3Hash, char *name, int start, int end);
/* Find trans3 in hash which corresponds to sequence of given name and includes
 * bases between start and end. */

void trans3Offset(struct trans3 *t3List, AA *aa, int *retOffset, int *retFrame);
/* Figure out offset of peptide in context of larger sequences. */

int trans3GenoPos(char *pt, bioSeq *seq, struct trans3 *t3List, boolean isEnd);
/* Convert from position in one of three translated frames in
 * t3List to genomic offset. If t3List is NULL then just use seq
 * instead. */

int trans3Frame(char *pt, struct trans3 *t3List);
/* Figure out which frame pt is in or 0 if no frame. */

#endif /* TRANS3_H */
