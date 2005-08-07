/* chainConnect - Modules to figure out cost of connecting two blocks
 * in a chain, and to help clean up overlapping blocks in a chain after
 * main chainBlock call is done. */

#ifndef CHAINCONNECT_H
#define CHAINCONNECT_H

struct chainConnect
/* Structure to help figure out chain connection costs. */
    {
    struct dnaSeq *query;
    struct dnaSeq *target;
    struct axtScoreScheme *ss;
    struct gapCalc *gapCalc;
    };


int chainConnectCost(struct cBlock *a, struct cBlock *b, 
	struct chainConnect *cc);
/* Calculate connection cost - including gap score
 * and overlap adjustments if any. */

int chainConnectGapCost(int dq, int dt, struct chainConnect *cc);
/* Calculate cost of non-overlapping gap. */

void chainRemovePartialOverlaps(struct chain *chain, 
	struct dnaSeq *qSeq, struct dnaSeq *tSeq, int matrix[256][256]);
/* If adjacent blocks overlap then find crossover points between them. */

void chainMergeAbutting(struct chain *chain);
/* Merge together blocks in a chain that abut each
 * other exactly. */

double chainScoreBlock(char *q, char *t, int size, int matrix[256][256]);
/* Score block through matrix. */

double chainCalcScore(struct chain *chain, struct axtScoreScheme *ss, 
	struct gapCalc *gapCalc, struct dnaSeq *query, struct dnaSeq *target);
/* Calculate chain score freshly. */

double chainCalcScoreSubChain(struct chain *chain, struct axtScoreScheme *ss, 
	struct gapCalc *gapCalc, struct dnaSeq *query, struct dnaSeq *target);
/* Calculate chain score assuming query and target 
   span the chained region rather than entire chrom. */

void chainCalcBounds(struct chain *chain);
/* Recalculate chain boundaries - setting qStart/qEnd/tStart/tEnd from
 * a scan of blockList. */

void cBlockFindCrossover(struct cBlock *left, struct cBlock *right,
	struct dnaSeq *qSeq, struct dnaSeq *tSeq,  
	int overlap, int matrix[256][256], int *retPos, int *retScoreAdjustment);
/* Find ideal crossover point of overlapping blocks.  That is
 * the point where we should start using the right block rather
 * than the left block.  This point is an offset from the start
 * of the overlapping region (which is the same as the start of the
 * right block). */

#endif /* CHAINCONNECT_H */

