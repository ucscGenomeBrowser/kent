/* chain - pairwise alignments that can include gaps in both
 * sequences at once.  This is similar in many ways to psl,
 * but more suitable to cross species genomic comparisons. */

#ifndef CHAIN_H
#define CHAIN_H

#ifndef LINEFILE_H
#include "linefile.h"
#endif

#ifndef BOXCLUMP_H
#include "boxClump.h"	/* Include so can share boxIn structure */
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

void chainFree(struct chain **pChain);
/* Free up a chain. */

void chainFreeList(struct chain **pList);
/* Free a list of dynamically allocated chain's */

int chainCmpScore(const void *va, const void *vb);
/* Compare to sort based on score. */

int chainCmpTarget(const void *va, const void *vb);
/* Compare to sort based on target position. */

void chainWrite(struct chain *chain, FILE *f);
/* Write out chain to file in dense format. */

void chainWriteLong(struct chain *chain, FILE *f);
/* Write out chain to file in more verbose format. */

void chainWriteHead(struct chain *chain, FILE *f);
/* Write chain before block/insert list. */

struct chain *chainRead(struct lineFile *lf);
/* Read next chain from file.  Return NULL at EOF. 
 * Note that chain block scores are not filled in by
 * this. */

void chainIdReset();
/* Reset chain id. */

void chainIdNext(struct chain *chain);
/* Add id to chain. */

#endif /* CHAIN_H */
