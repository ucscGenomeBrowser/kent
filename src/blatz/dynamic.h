// LX BEG Sep 06 2005
/* dynamic - Keep track of hits at individual query and target positions
   and mask them dynamically as needed when they reach dynaLimit */

#ifndef DYNAMIC_H
#define DYNAMIC_H

#define VERY_LARGE_NUMBER 999999

// Set the following to to the desired COUNTER_TYPE
//#define COUNTER_TYPE "char"
#define COUNTER_TYPE "short"
unsigned short *dynaCountT;
unsigned short *dynaCountQ;
//unsigned short *dynaCountTtemp; // LX Oxt 12 2005
unsigned short *dynaCountQtemp; // LX Oxt 06 2005
//unsigned char *dynaCountT;
//unsigned char *dynaCountQ;

unsigned short* globalCounter;

int dynaHits;
char dynaBreak;
int dynaDrops;
int dynaDropsPerc;
int dynaCountTarget;
int dynaCountQuery;
//char dynaCountT[MAX_TLENGTH];
//char dynaCountQ[MAX_QLENGTH];
int targetHitDLimit; // Sep 06 2005
int queryHitDLimit; // Sep 06 2005

#endif /* DYNAMIC_H */
// LX END
