/* chainBlock - Chain together scored blocks from an alignment
 * into scored chains.  Internally this uses a kd-tree and a
 * varient of an algorithm suggested by Webb Miller and further
 * developed by Jim Kent. */

#ifndef CHAINBLOCK_H
#define CHAINBLOCK_H

#ifndef CHAIN_H
#include "chain.h"
#endif

typedef int (*GapCost)(int dq, int dt);
/* A function that returns gap cost (gaps can be in both dimensions
 * at once!) */

typedef int (*ConnectCost)(struct boxIn *a, struct boxIn *b);
/* A function that returns gap cost as well as any penalty
 * from a and b overlapping. */

struct chain *chainBlocks(char *qName, int qSize, char qStrand,
	char *tName, int tSize, struct boxIn **pBlockList, 
	ConnectCost connectCost, GapCost gapCost, FILE *details);
/* Create list of chains from list of blocks.  The blockList will get
 * eaten up as the blocks are moved from the list to the chain. 
 * The chain returned is sorted by score. 
 *
 * The details FILE may be NULL, and is where additional information
 * about the chaining is put.
 *
 * Note that the connectCost needs to adjust for possibly partially 
 * overlapping blocks, and that these need to be taken out of the
 * resulting chains in general.  See src/hg/axtChain for an example
 * of coping with this. */

#endif /* CHAINBLOCK_H */
