/* blatz - Align two DNA sequences of roughly 60% sequence identity
 * semi-reliably and quickly I hope. */

#ifndef BLATZ_H
#define BLATZ_H

#ifndef BZP_H
#include "bzp.h"
#endif

#ifndef CHAIN_H
#include "chain.h"
#endif

struct blatzIndexPos
/* An array of places where indexed word occurs. */
    {
    bits32 *pos;	/* Array of positions. */
    bits32 count;	/* Number of positions. */
    };

struct blatzIndex
/* An index based on a neigborhood. */
    {
    struct blatzIndex *next;	/* Next in list. */
    struct blatzIndexPos *slots;/* One slot for each index value. */
    int seedWeight;		/* Number of bases where seed is 1. */
    int seedSpan;		/* Total bases spanned by seed. */
    int *seedOffsets;		/* Offsets to bases seed cares about. */
    struct dnaSeq *target;	/* Target sequence.  Not allocated here. */
    int targetOffset;		/* Offset of target in parent sequence. */
    int targetParentSize;	/* Size of parent sequence. */
    bits32 *posBuf;		/* This holds memory for positions in all slots. */
    };

struct blatzIndex *blatzIndexOne(struct dnaSeq *seq, int offset, int parentSize, 
	int weight);
/* Create a new index of given seed weight populated by seq. */

struct blatzIndex *blatzIndexAll(struct dnaSeq *seqList, int seedWeight);
/* Return a list of indexes, one for each seq on seqList */

void blatzIndexFree(struct blatzIndex **pIndex);
/* Free up memory associated with index. */

int blatzIndexKey(DNA *dna, int *seedOffsets, int seedWeight);
/* Calculate which slot in index to look into.  Returns -1 if
 * dna contains lower case or N or other junk. */

void blatzGaplessScan(struct bzp *bzp, struct blatzIndex *index, 
	struct dnaSeq *target, struct cBlock **msps);
/* Scan index for hits, do gapless extensions into maximally
 * scoring segment pairs (MSPs), and put MSPs over threshold
 * onto pBlockList. */

void blatzWriteChains(struct bzp *bzp, struct chain *chainList,
	struct dnaSeq *query, struct blatzIndex *targetIndexList, 
	FILE *f);
/* Output chainList to file in format specified by bzp->outType. */

struct chain *blatzAlign(struct bzp *bzp, struct blatzIndex *indexList,
	struct dnaSeq *query);
/* Align both strands of query against index using parameters in bzp.
 * Return chains sorted by score (highest scoring first) */

int blatzVersion();
/* Return version number. */

#endif /* BLATZ_H */
