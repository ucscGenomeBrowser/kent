/* chainBlock - Chain together scored blocks from an alignment
 * into scored chains.  Internally this uses a kd-tree and a
 * varient of an algorithm suggested by Webb Miller and further
 * developed by Jim Kent. */

#ifndef CHAINBLOCK_H
#define CHAINBLOCK_H

#ifndef CHAIN_H
#include "chain.h"
#endif

typedef int (*GapCost)(int dq, int dt, void *gapData);
/* A function that returns gap cost (gaps can be in both dimensions
 * at once!) */

typedef int (*ConnectCost)(struct cBlock *a, struct cBlock *b, void *gapData);
/* A function that returns gap cost as well as any penalty
 * from a and b overlapping. */

struct chain *chainBlocks(
	char *qName, int qSize, char qStrand,	/* Info on query sequence */
	char *tName, int tSize, 		/* Info on target. */
	struct cBlock **pBlockList, 		/* Unordered ungapped alignments. */
	ConnectCost connectCost, 		/* Calculate cost to connect nodes. */
	GapCost gapCost, 			/* Cost for non-overlapping nodes. */
	void *gapData, 				/* Passed through to connect/gapCosts */
	FILE *details);				/* NULL except for debugging */
/* Create list of chains from list of blocks.  The blockList will get
 * eaten up as the blocks are moved from the list to the chain. 
 * The list of chains returned is sorted by score. 
 *
 * The details FILE may be NULL, and is where additional information
 * about the chaining is put.
 *
 * Note that the connectCost needs to adjust for possibly partially 
 * overlapping blocks, and that these need to be taken out of the
 * resulting chains in general.  This can get fairly complex.  Also
 * the chains will need some cleanup at the end.  Use the chainConnect
 * module to help with this.  See hg/mouseStuff/axtChain for example usage. */

#endif /* CHAINBLOCK_H */
