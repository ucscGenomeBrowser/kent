/* chainBlock - Chain together scored blocks from an alignment
 * into scored chains.  Internally this uses a kd-tree and a
 * varient of an algorithm suggested by Webb Miller and further
 * developed by Jim Kent. */

#ifndef CHAINBLOCK_H
#define CHAINBLOCK_H

#ifndef BOXCLUMP_H
#include "boxClump.h"	/* Include so can share boxIn structure */
#endif

#ifndef LINEFILE_H
#include "linefile.h"
#endif

struct chain
/* A chain of blocks.  Used for output of chainBlocks. */
    {
    struct chain *next;	  	  /* Next in list. */
    struct boxIn *blockList;      /* List of blocks. */
    double score;	  	  /* Total score for chain. */
    char *tName;		  /* target name, allocated here. */
    int tSize;			  /* Overall size of target. */
    /* tStrand always + */
    int tStart,tEnd;		  /* Range covered in query. */
    char *qName;		  /* query name, allocated here. */
    int qSize;			  /* Overall size of query. */
    char qStrand;		  /* Query strand. */
    int qStart,qEnd;		  /* Range covered in query. */
    int id;			  /* ID of chain in file. */
    };

typedef int (*GapCost)(int dq, int dt);
/* A function that returns gap cost (gaps can be in both dimensions
 * at once!) */

typedef int (*ConnectCost)(struct boxIn *a, struct boxIn *b);
/* A function that returns gap cost as well as any penalty
 * from a and b overlapping. */

struct chain *chainBlocks(char *qName, int qSize, char qStrand,
	char *tName, int tSize, struct boxIn **pBlockList, 
	ConnectCost connectCost, GapCost gapCost);
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

void chainWrite(struct chain *chain, FILE *f);
/* Write out chain to file in dense format. */

void chainWriteLong(struct chain *chain, FILE *f);
/* Write out chain to file in more verbose format. */
struct chain *chainRead(struct lineFile *lf);
/* Read next chain from file.  Return NULL at EOF. 
 * Note that chain block scores are not filled in by
 * this. */

void chainIdReset();
/* Reset chain id. */

void chainIdNext(struct chain *chain);
/* Add id to chain. */

#endif /* CHAINBLOCK_H */
