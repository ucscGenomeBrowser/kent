#ifndef CHAININFO_H
#define CHAININFO_H

struct intronChain {
    /* scoring of aligned introns in a chain */ 
    struct intronChain *next;	/* Next in list. */
    struct chain *chain;        /* scored chain */
    int qIntron ;               /* count of introns on query side */ 
    int tIntron ;               /* count of introns on target side */ 
    int tGap ;                  /* count of bases in gaps on target */
    int qGap ;                  /* count of bases in gaps on query */
    int tReps;              /* total number of repeats in target */
    int qReps;              /* total number of repeats in target */
    int total;                  /* total size of chain */
    float score1;               /* score2 * ratio of bases in target/query gaps */
    float score2;               /* ratio of introns target/query , negative if query has more */
};

#endif /* CHAININFO_H */
