/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* dnaSeq.c - stuff to manage DNA sequences. */
#include "common.h"
#include "dnaseq.h"


struct dnaSeq *newDnaSeq(DNA *dna, int size, char *name)
/* Create a new DNA seq. */
{
struct dnaSeq *seq;

seq = needMem(sizeof(*seq));
if (name != NULL)
    seq->name = cloneString(name);
seq->dna = dna;
seq->size = size;
return seq;
}

void freeDnaSeq(struct dnaSeq **pSeq)
/* Free up DNA seq. (And unlink underlying resource node.) */
{
struct dnaSeq *seq = *pSeq;
if (seq == NULL)
    return;
freeMem(seq->name);
freeMem(seq->dna);
freez(pSeq);
}

void freeDnaSeqList(struct dnaSeq **pSeqList)
/* Free up list of DNA sequences. */
{
struct dnaSeq *seq, *next;

for (seq = *pSeqList; seq != NULL; seq = next)
    {
    next = seq->next;
    freeDnaSeq(&seq);
    }
*pSeqList = NULL;
}

aaSeq *translateSeq(struct dnaSeq *inSeq, int offset, boolean stop)
/* Return a translated sequence.  Offset is position of first base to
 * translate. If stop is TRUE then stop at first stop codon.  (Otherwise 
 * represent stop codons as 'Z'). */
{
aaSeq *seq;
DNA *dna = inSeq->dna;
AA *pep, aa;
int inSize = inSeq->size;
int i, lastCodon = inSize - 3;
int txSize = (inSize-offset)/3;
int actualSize = 0;
char buf[256];

AllocVar(seq);
seq->dna = pep = needLargeMem(txSize+1);
for (i=offset; i <= lastCodon; i += 3)
    {
    aa = lookupCodon(dna+i);
    if (aa == 0)
	{
        if (stop)
	    break;
	else
	    aa = 'Z';
	}
    *pep++ = aa;
    ++actualSize;
    }
*pep = 0;
assert(actualSize <= txSize);
seq->size = actualSize;
seq->name = cloneString(inSeq->name);
return seq;
}

bioSeq *whichSeqIn(bioSeq **seqs, int seqCount, char *letters)
/* Figure out which if any sequence letters is in. */
{
aaSeq *seq;
int i;

uglyf("whichSeqIn seqCount %d letters %x\n", seqCount, letters);
for (i=0; i<seqCount; ++i)
    {
    seq = seqs[i];
    uglyf("  seq %x to %x\n", seq->dna, seq->dna + seq->size);
    if (seq->dna <= letters && letters < seq->dna + seq->size)
        return seq;
    }
internalErr();
}

