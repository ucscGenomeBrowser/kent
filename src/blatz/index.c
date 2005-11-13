/* index - blatz module that builds indexes using spaced seeds. */
/* Copyright 2005 Jim Kent.  All rights reserved. */

#include "common.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "spacedSeed.h"
#include "blatz.h"
#include "dynamic.h"

int blatzIndexKey(DNA *dna, int *seedOffsets, int seedWeight)
/* Calculate which slot in index to look into.  Returns -1 if
 * dna contains lower case or N or other junk. */
{
int i;
int key=0;
for (i=0; i<seedWeight; ++i)
    {
    int nt = ntValUpper[dna[seedOffsets[i]]];
    if (nt < 0)
        return -1;
    key <<= 2;
    key += nt;
    }
return key;
}

void blatzIndexFree(struct blatzIndex **pIndex)
/* Free up memory associated with index. */
{
struct blatzIndex *index = *pIndex;
if (index != NULL)
    {
    freeMem(index->slots);
    freeMem(index->seedOffsets);
    freeMem(index->posBuf);
    freeMem(index->counter); // for position-based dynamic masking
    freez(pIndex);
    }
}

struct seqPos 
/* A reference to a position within a sequence.  Part of a non-compacted index. */
    {
    struct seqPos *next;        /* Next in list. */
    int pos;                        /* Position in sequence. */
    };


struct blatzIndex *blatzIndexOne(struct dnaSeq *seq, int parentStart, int parentEnd,
	int parentSize, int weight)
/* Create a new index of given seed weight populated by seq. */
{
int seedSpan = spacedSeedSpan(weight);
int *seedOffsets = spacedSeedOffsets(weight);
int slotCount = (1<<(2*weight));
struct seqPos **slots, *pos;
DNA *dna = seq->dna;
int lastBase = seq->size - seedSpan;
int i;
int key;
struct blatzIndex *index;
size_t posCount = 0;
bits32 *posBuf;
struct blatzIndexPos *compactSlots;

/* Allocate index structure and fill in basics. */
AllocVar(index);
index->seedWeight = weight;
index->seedSpan = seedSpan;
index->seedOffsets = seedOffsets;
index->target = seq;
index->targetStart = parentStart;
index->targetEnd = parentEnd;
index->targetParentSize = parentSize;
compactSlots = AllocArray(index->slots, slotCount);

/* Count up positions in each index slot. */
for (i=0; i<=lastBase; ++i)
    {
    int key = blatzIndexKey(dna + i, seedOffsets, weight);
    if (key >= 0)
	compactSlots[key].count += 1;
    }

/* Get rid of overused keys */
if (dynaWordLimit != 0)
    {
    for (key=0; key<slotCount; ++key)
	{
	if (compactSlots[key].count > dynaWordLimit)
	    compactSlots[key].count = 0;
	}
    }

/* Add up total number of positions needed. */
for (key=0; key<slotCount; ++key)
    posCount += compactSlots[key].count;

if (posCount > 0)
    {
    /* Alloc array for all positions, and point slots to appropriate parts. 
     * Leave dynamically masked slots NULL. */
    index->posBuf = posBuf = needHugeMem(posCount * sizeof(posBuf[0]));
    for (key=0; key < slotCount; ++key)
        {
	struct blatzIndexPos *slot = &compactSlots[key];
	if (slot->count > 0)
	    {
	    slot->pos = posBuf;
	    posBuf += slot->count;
	    slot->count = 0;
	    }
	}

    /* Scan through DNA again, this time saving key positions
     * instead of just counting them. */
    /* Count up positions in each index slot. */
    for (i=0; i<=lastBase; ++i)
	{
	int key = blatzIndexKey(dna + i, seedOffsets, weight);
	if (key >= 0)
	    {
	    struct blatzIndexPos *slot = &compactSlots[key];
	    if (slot->pos != NULL)
	        {
		slot->pos[slot->count] = i;
		slot->count += 1;
		}
	    }
	}
    }

return index;
}

struct blatzIndex *blatzIndexDl(struct dnaLoad *dl, int weight, boolean unmask)
/* Cycle through everything in dl, save it, and make an index for it. */
{
struct dnaSeq *seq;
struct blatzIndex *indexList = NULL, *index;
while ((seq = dnaLoadNext(dl)) != NULL)
    {
    int start = dnaLoadCurStart(dl);
    int end = dnaLoadCurEnd(dl);
    int size = dnaLoadCurSize(dl);
    if (unmask)
	toUpperN(seq->dna, seq->size);
    index = blatzIndexOne(seq, start, end, size, weight);
    slAddHead(&indexList, index);
    }
slReverse(&indexList);
return indexList;
}

