/* trans3 - a sequence and three translated reading frames. */
#ifndef TRANS3_H
#define TRANS3_H

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

#endif /* TRANS3_H */
