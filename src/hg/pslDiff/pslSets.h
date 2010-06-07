/* pslSets - object use to stores sets of psls and match alignments to those
 * other sets */
#ifndef PSSETS_H
#define PSSETS_H

struct pslSets
/* Object use to stores sets of psls and match alignments to those
 * other sets.  Each set is stored in an array so sets to be indexed */
{
    unsigned numSets;                /* number of sets (tables) of psls */
    struct pslTbl **sets;            /* array of psl tables */
    struct pslRef **pending;         /* pending PSLs for current query for each set */
    struct pslMatches *matches;      /* list of matches linking psls together */
    struct pslMatches *matchesPool;  /* recycled pslMatches objects */
    struct pslRef *refPool;          /* recycled pslMatches objects */
    struct lm *lm;                   /* pool for allocating objects */
    int nameWidth;                   /* max witdh of any set name */
};

struct pslMatches
/* A set of matched PSLs  */
{
    struct pslMatches *next;    /* next matches */
    char *tName;                /* chrom for matched queries, memory not owned */
    int tStart;                 /* range covered by matched queries */
    int tEnd;
    char strand[3];
    int numSets;                /* number of sets */
    struct psl **psls;          /* psl for each set */
    
};

struct pslRef
/* object used to link psl for current query to a set */
{
    struct pslRef *next;       /* link to next in set (or pool list) */
    struct psl *psl;           /* psl for this set, set to NULL when added
                                * to pslMatches */
};

struct pslSets *pslSetsNew(int numSets);
/* construct a new pslSets object */

void pslSetsFree(struct pslSets **psPtr);
/* free a pslSets object */

void pslSetsLoadSet(struct pslSets *ps, unsigned iSet, char *pslFile, char *setName);
/* load a set of PSLs */

struct slName *pslSetsQueryNames(struct pslSets *ps);
/* get list of all query names in all tables. slFree when done */


void pslSetsMatchQuery(struct pslSets *ps, char *qName);
/* Get the pslMatches records for a query from all psl sets */


#endif
