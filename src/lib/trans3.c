/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/

/* trans3 - a sequence and three translated reading frames. */

#include "common.h"
#include "dnaseq.h"
#include "trans3.h"

struct trans3 *trans3New(struct dnaSeq *seq)
/* Create a new set of translated sequences. */
{
struct trans3 *t3;
int frame;

AllocVar(t3);
t3->name = seq->name;
t3->seq = seq;
t3->end = seq->size;
for (frame=0; frame<3; ++frame)
    t3->trans[frame] = translateSeq(seq, frame, FALSE);
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

