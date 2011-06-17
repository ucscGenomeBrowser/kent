/* chain - pairwise alignments that can include gaps in both
 * sequences at once.  This is similar in many ways to psl,
 * but more suitable to cross species genomic comparisons. */

#ifndef CHAIN_H
#define CHAIN_H

#ifndef LINEFILE_H
#include "linefile.h"
#endif


#ifndef BITS_H
#include "bits.h"
#endif

struct cBlock
/* A gapless part of a chain. */
    {
    struct cBlock *next;	/* Next in list. */
    int tStart,tEnd;		/* Range covered in target. */
    int qStart,qEnd;		/* Range covered in query. */
    int score;	 	 	/* Score of block. */
    void *data;			/* Some associated data pointer. */
    };

int cBlockCmpQuery(const void *va, const void *vb);
/* Compare to sort based on query start. */

int cBlockCmpTarget(const void *va, const void *vb);
/* Compare to sort based on target start. */

int cBlockCmpBoth(const void *va, const void *vb);
/* Compare to sort based on query, then target. */

int cBlockCmpDiagQuery(const void *va, const void *vb);
/* Compare to sort based on diagonal, then query. */

void cBlocksAddOffset(struct cBlock *blockList, int qOff, int tOff);
/* Add offsets to block list. */

struct cBlock *cBlocksFromAliSym(int symCount, char *qSym, char *tSym, 
        int qPos, int tPos);
/* Convert alignment from alignment symbol (bases and dashes) format 
 * to a list of chain blocks.  The qPos and tPos correspond to the start
 * in the query and target sequences of the first letter in  qSym and tSym. */

struct chain
/* A chain of blocks.  Used for output of chainBlocks. */
    {
    struct chain *next;	  	  /* Next in list. */
    struct cBlock *blockList;      /* List of blocks. */
    double score;	  	  /* Total score for chain. */
    char *tName;		  /* target name, allocated here. */
    int tSize;			  /* Overall size of target. */
    /* tStrand always + */
    int tStart,tEnd;		  /* Range covered in target. */
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

int chainCmpScoreDesc(const void *va, const void *vb);
/* Compare to sort based on score descending. */

int chainCmpTarget(const void *va, const void *vb);
/* Compare to sort based on target position. */

int chainCmpQuery(const void *va, const void *vb);
/* Compare to sort based on query chrom and start osition. */

void chainWrite(struct chain *chain, FILE *f);
/* Write out chain to file in dense format. */

void chainWriteAll(struct chain *chainList, FILE *f);
/* Write all chains to file. */

void chainWriteLong(struct chain *chain, FILE *f);
/* Write out chain to file in more verbose format. */

void chainWriteHead(struct chain *chain, FILE *f);
/* Write chain before block/insert list. */

struct chain *chainRead(struct lineFile *lf);
/* Read next chain from file.  Return NULL at EOF. 
 * Note that chain block scores are not filled in by
 * this. */

struct chain *chainReadChainLine(struct lineFile *lf);
/* Read line that starts with chain.  Allocate memory
 * and fill in values.  However don't read link lines. */

void chainReadBlocks(struct lineFile *lf, struct chain *chain);
/* Read in chain blocks from file. */

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
    
struct hash *chainReadAllWithMeta(char *fileName, FILE *f);
/* Read chains into a hash keyed by id and outputs meta data */

struct chain *chainFind(struct hash *hash, int id);
/* Find chain in hash, return NULL if not found */

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

void chainFastSubsetOnT(struct chain *chain, struct cBlock *firstBlock,
	int subStart, int subEnd, struct chain **retSubChain,  struct chain **retChainToFree);
/* Get subchain as in chainSubsetOnT. Pass in initial block that may
 * be known from some index to speed things up. */

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
