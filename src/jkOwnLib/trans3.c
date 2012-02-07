/* trans3 - a sequence and three translated reading frames. */
/* Copyright 2000-2003 Jim Kent.  All rights reserved. */

#include "common.h"
#include "dnaseq.h"
#include "trans3.h"


struct trans3 *trans3New(struct dnaSeq *seq)
/* Create a new set of translated sequences. */
{
struct trans3 *t3;
int frame;
int lastPos = seq->size - 1;

AllocVar(t3);
t3->name = seq->name;
t3->seq = seq;
t3->end = seq->size;
for (frame=0; frame<3; ++frame)
    {
    /* Position and frame are the same except in the
     * very rare case where we are trying to translate 
     * something less than 3 bases.  In this case this
     * somewhat cryptic construction will force it to
     * return empty sequences for the missing frames
     * avoiding an assert in translateSeq. */
    int pos = frame;
    if (pos > lastPos) pos = lastPos;
    t3->trans[frame] = translateSeq(seq, pos, FALSE);
    }
return t3;
}

void trans3Free(struct trans3 **pT3)
/* Free a trans3 structure. */
{
struct trans3 *t3 = *pT3;
if (t3 != NULL)
    {
    freeDnaSeq(&t3->trans[0]);
    freeDnaSeq(&t3->trans[1]);
    freeDnaSeq(&t3->trans[2]);
    freez(pT3);
    }
}

void trans3FreeList(struct trans3 **pList)
/* Free a list of dynamically allocated trans3's */
{
struct trans3 *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    trans3Free(&el);
    }
*pList = NULL;
}

struct trans3 *trans3Find(struct hash *t3Hash, char *name, int start, int end)
/* Find trans3 in hash which corresponds to sequence of given name and includes
 * bases between start and end. */
{
struct trans3 *t3;
for (t3 = hashFindVal(t3Hash, name); t3 != NULL; t3 = t3->next)
    {
    if (t3->start <= start && t3->end >= end)
        return t3;
    }
internalErr();
return NULL;
}

void trans3Offset(struct trans3 *t3List, AA *aa, int *retOffset, int *retFrame)
/* Figure out offset of peptide in context of larger sequences. */
{
struct trans3 *t3;
int frame;
aaSeq *seq;

for (t3 = t3List; t3 != NULL; t3 = t3->next)
    {
    for (frame = 0; frame < 3; ++frame)
        {
	seq = t3->trans[frame];
	if (seq->dna <= aa && aa < seq->dna + seq->size)
	    {
	    *retOffset = aa - seq->dna + t3->start/3;
	    *retFrame = frame;
	    return;
	    }
	}
    }
internalErr();
}

int trans3GenoPos(char *pt, bioSeq *seq, struct trans3 *t3List, boolean isEnd)
/* Convert from position in one of three translated frames in
 * t3List to genomic offset. If t3List is NULL then just use seq
 * instead. */
{
int offset, frame;
if (t3List != NULL)
    {
    /* Special processing at end. The end coordinate is
     * not included.  In most cases this makes things
     * easier.  Here we have to move it back one
     * amino acid, so that in the edge case it will
     * be included in the block that's loaded.  Then
     * we move it back. */
    if (isEnd)
        pt -= 1;
    trans3Offset(t3List, pt, &offset, &frame);
    if (isEnd)
        offset += 1;
    return 3*offset + frame;
    }
else
   {
   return pt - seq->dna;
   }
}

int trans3Frame(char *pt, struct trans3 *t3List)
/* Figure out which frame pt is in or 0 if no frame. */
{
if (t3List == NULL)
    return 0;
else
    return 1 + trans3GenoPos(pt, NULL, t3List, FALSE)%3;
}

