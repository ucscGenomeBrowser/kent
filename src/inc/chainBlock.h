/* chainBlock - Chain together scored blocks from an alignment
 * into scored chains.  Internally this uses a kd-tree and a
 * varient of an algorithm suggested by Webb Miller. */

#ifndef CHAINBLOCK_H
#define CHAINBLOCK_H

struct chainBlock
/* A block of an alignment.  Input to chain blocks. */
    {
    struct chainBlock *next;	/* Next in list. */
    int qStart,qEnd;		/* Block position in query. */
    int tStart,tEnd;		/* Block position in target. */
    int score;			/* Score for this block. */
    void *data;			/* User associated data. */
    };
/* Use freez/slFreeList to free chainBlocks. */

struct chain
/* A chain of blocks.  Used for output of chainBlocks. */
    {
    struct chain *next;	  	  /* Next in list. */
    struct chainBlock *blockList; /* List of blocks. */
    int score;			  /* Total score for chain. */
    };

typedef int (*ConnectCost)(struct chainBlock *a, struct chainBlock *b);
/* A function that returns cost of connecting a to b.  This includes
 * the gap penalty as well as any loss of score caused by a/b overlap. 
 * This function is supplied by the user. */

struct chain *chainBlocks(struct chainBlock **pBlockList, ConnectCost connectCost);
/* Create list of chains from list of blocks.  The blockList will get
 * eaten up as the blocks are moved from the list to the chain. */

void chainFree(struct chain **pChain);
/* Free up a chain. */

void chainFreeList(struct chain **pList);
/* Free a list of dynamically allocated chain's */

#endif /* CHAINBLOCK_H */
