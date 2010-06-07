/* pairHmm - stuff to help implement pairwise hidden markov models,
 * which are useful ways of aligning two sequences. 
 *
 * This file is copyright 2000-2004 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef PAIRHMM_H
#define PAIRHMM_H

/* Mommy coding scheme - this is how one cell in the dynamic programming table
 * points to it's parent (mommy) cell.  Since these tables are really big,
 * rather than use a simple pointer costing four bytes, we use a encoding
 * scheme that requires only one byte. 
 *
 * Bits 0-4  the "hidden" state of the mommy.  Lets us have 32 hidden states.
 *           currently only using 7.
 * Bit  5    whether or not mommy is in previous cell in query
 * Bit  6    whether or not mommy is in previous cell in target
 * Bit  7    set during trace back for cells along optimal path
 *
 * Since the query and target advancing bits (5 and 6) are never both zero,
 * it is safe to use the value of all-bits-zero as an indicator of
 * no mommy. */

/* Compress state, query, and target offset into one byte. */
#define phmmPackMommy(stateIx, qOff, tOff) ((UBYTE)((stateIx) + ((-(qOff))<<5) + ((-(tOff))<<6)))

/* Traceback sets this, really just for debugging. */
#define phmmMommyTraceBit (1<<7)

struct phmmMommy
/* This contains the parent info for a single state of the matrix. */
    {
    UBYTE mommy; /* Unlike a parent, you can only have one mommy! */
    };

extern UBYTE phmmNullMommy; /* mommy value for orphans.... */

struct phmmState
/* This corresponds to a hidden Markov state.  Each one of
 * these has a two dimensional array[targetSize+1][querySize+1]
 * of cells. */
    {
    struct phmmMommy *cells;	/* The 2-D array containing traceback info. */
    int *scores;		/* Scores for the current row. */
    int *lastScores;		/* Scores for the previous row. */
    int stateIx;		/* Numerical handle on state. */
    char *name;			/* Name of state. */
    char emitLetter;		/* Single letter representing state. */
    };

struct phmmMatrix
/* The alignment matrix - has an array of states. */
    {
    char *query;	/* One sequence to align- all lower case. */
    char *target;    	/* Other sequence to align. */
    int querySize;	/* Size of query. */
    int targetSize;	/* Size of target. */
    int qDim;		/* One plus size of query - dimension of matrix. */
    int tDim;		/* One plus size of target - dimension of matrix. */
    int stateCount;	/* Number of hidden states in HMM. */
    int stateSize;	/* Number of cells in each state's matrix. */
    int stateByteSize;	/* Number of bytes used by each state's matrix. */
    struct phmmState *states;  /* Array of states. */
    struct phmmMommy *allCells; /* Memory for all matrices. */
    int *allScores;	      /* Memory for two rows of scores. */
    };

struct phmmMatrix *phmmMatrixNew(int stateCount,
    char *query, int querySize, char *target, int targetSize);
/* Allocate all memory required for an phmmMatrix. Set up dimensions. */

void phmmMatrixFree(struct phmmMatrix **pAm);
/* Free up memory required for an phmmMatrix and make sure
 * nobody reuses it. */

struct phmmState *phmmNameState(struct phmmMatrix *am, int stateIx, 
	char *name, char emitLetter);
/* Give a name to a state and return a pointer to it. */

struct phmmAliPair *phmmTraceBack(struct phmmMatrix *am, struct phmmMommy *end);
/* Create list of alignment pair by tracing back through matrix from end
 * state back to a start.*/

void phmmPrintTrace(struct phmmMatrix *am, struct phmmAliPair *pairList, 
	boolean showStates, FILE *f, boolean extraAtEnds);
/* Print out trace to file. */

struct axt *phhmTraceToAxt(struct phmmMatrix *am, struct phmmAliPair *pairList, 
	int score, char *qName, char *tName);
/* Convert alignment from traceback format to axt. */

#endif /* PAIRHMM_H */

