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

#ifndef BITS_H
#include "bits.h"
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

int chainCmpQuery(const void *va, const void *vb);
/* Compare to sort based on query chrom and start osition. */

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

void chainSwap(struct chain *chain);
/* Swap target and query side of chain. */

struct hash *chainReadUsedSwap(char *fileName, boolean swapQ, Bits *bits);
/* Read chains that are marked as used in the 
 * bits array (which may be NULL) into a hash keyed by id. */

struct hash *chainReadAllSwap(char *fileName, boolean swapQ);
/* Read chains into a hash keyed by id. 
 * Set swapQ to True to read chain by query. */
    
struct hash *chainReadAll(char *fileName);
/* Read chains into a hash keyed by id. */
    
struct chain *chainLookup(struct hash *hash, int id);
/* Find chain in hash. */

void chainSubsetOnT(struct chain *chain, int subStart, int subEnd, 
    struct chain **retSubChain,  struct chain **retChainToFree);
/* Get subchain of chain bounded by subStart-subEnd on 
 * target side.  Return result in *retSubChain.  In some
 * cases this may be the original chain, in which case
 * *retChainToFree is NULL.  When done call chainFree on
 * *retChainToFree.  The score and id fields are not really
 * properly filled in. */

void chainSubsetOnQ(struct chain *chain, int subStart, int subEnd, 
    struct chain **retSubChain,  struct chain **retChainToFree);
/* Get subchain of chain bounded by subStart-subEnd on 
 * query side.  Return result in *retSubChain.  In some
 * cases this may be the original chain, in which case
 * *retChainToFree is NULL.  When done call chainFree on
 * *retChainToFree.  The score and id fields are not really
 * properly filled in. */

void chainRangeQPlusStrand(struct chain *chain, int *retQs, int *retQe);
/* Return range of bases covered by chain on q side on the plus
 * strand. */

#endif /* CHAIN_H */
