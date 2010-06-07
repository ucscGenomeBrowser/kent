/* dynamic - Keep track of hits at individual query and target positions
   and mask them dynamically as needed when they reach dynaLimit 
   alternatively track word hits rather than positions and do the same */

#ifndef DYNAMIC_H
#define DYNAMIC_H

#define VERY_LARGE_NUMBER 999999

// *** Position-based dynamic masking
// Set the following to to the desired COUNTER_TYPE 
// In other words, what is the max No. of hits per position to deal with
//#define COUNTER_TYPE "char"
#define COUNTER_TYPE "short"
unsigned short *dynaCountT; // target hit table
unsigned short *dynaCountQ; // query hit table
unsigned short *dynaCountQtemp; // to postpone setting the dynamic mask
unsigned short* globalCounter; // always holds the right index->counter
int targetHitDLimit; // taken from command line
int queryHitDLimit;  // taken from command line

// *** Word-based dynamic masking
unsigned int *dynaWordCount; // word hit table
unsigned int dynaNumWords; // this many words exist at the given weight
unsigned int dynaWordLimit; // taken from command line

// Other variables, mostly statistics
int dynaHits;
char dynaBreak;
int dynaDrops;
int dynaDropsPerc;
int dynaCountTarget;
int dynaCountQuery;

#endif /* DYNAMIC_H */
