/* seqStats - some sequence statistics functions that need
 * math libraries. */

#ifndef SEQSTATS_H
#define SEQSTATS_H

double dnaMatchEntropy(DNA *query, DNA *target, int baseCount);
/* Return entropy of matching bases - a number between 0 and 1, with
 * higher numbers the more diverse the matching bases. */

#endif /* SEQSTATS_H */
