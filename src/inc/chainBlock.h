/* chainBlock - Chain together scored blocks from an alignment
 * into scored chains.  Internally this uses a kd-tree and a
 * varient of an algorithm suggested by Webb Miller. */

#ifndef CHAINBLOCK_H
#define CHAINBLOCK_H

#ifndef BOXCLUMP_H
#include "boxClump.h"	/* Include so can share boxIn structure */
#endif

struct chain
/* A chain of blocks.  Used for output of chainBlocks. */
    {
    struct chain *next;	  	  /* Next in list. */
    struct boxIn *blockList;      /* List of blocks. */
    int score;			  /* Total score for chain. */
    char *qName;		  /* query name, allocated here. */
    int qSize;			  /* Overall size of query. */
    char qStrand;		  /* Query strand. */
    int qStart,qEnd;		  /* Range covered in query. */
    char *tName;		  /* target name, allocated here. */
    int tSize;			  /* Overall size of target. */
    int tStart,tEnd;		  /* Range covered in query. */
    };

typedef int (*ConnectCost)(struct boxIn *a, struct boxIn *b);
/* A function that returns gap cost (gaps can be in both dimensions
 * at once! */

struct chain *chainBlocks(char *qName, int qSize, char qStrand,
	char *tName, int tSize,
	struct boxIn **pBlockList, ConnectCost connectCost);
/* Create list of chains from list of blocks.  The blockList will get
 * eaten up as the blocks are moved from the list to the chain. 
 * The chain returned is sorted by score. 
 *
 * Note that the connectCost needs to adjust for possibly partially 
 * overlapping blocks, and that these need to be taken out of the
 * resulting chains in general.  See src/hg/axtChain for an example
 * of coping with this. */

void chainFree(struct chain **pChain);
/* Free up a chain. */

void chainFreeList(struct chain **pList);
/* Free a list of dynamically allocated chain's */

int chainCmpScore(const void *va, const void *vb);
/* Compare to sort based on score. */

#endif /* CHAINBLOCK_H */
