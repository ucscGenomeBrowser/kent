/* rnautil.h - functions for dealing with RNA and RNA secondary structure.  */
#ifndef RNAUTIL_H
#define RNAUTIL_H

#ifndef COMMON_H
#include "common.h"
#endif

extern const char *RNA_PAIRS[];
/* Null terminated array of rna pairs */

void dna2rna(char *s);
/* Replace 't' with 'u' and 'T' with 'U'. */

bool rnaPair(char a, char b);
/* Returns TRUE if a and b can pair, and false otherwise */

void reverseFold(char *s);
/* Reverse the order of the parenthesis defining an RNA secondary structure annotation. */

void fold2pairingList(char *fold, int len, int **p2pairList);
/* take a parenthesis string, allocate and return an array of pairing
   positions: pairList[i] = j <=> i pair with j and pairList[i] = -1
   <=> i does not pair.*/

void mkPairPartnerSymbols(int * pairList, char * pairSymbols, int size);
/* Make a symbol string indicating pairing partner */

char * projectString(char * s, char * ref, char refChar, char insertChar);
/* Insert 'insertChar' in 's' at every positin 'ref' has 'refChar'. */

char * gapAdjustFold(char * s, char * ref);
/* Insert space in s when there is a gap ('-') in ref. */

int * projectIntArray(int * in, char * ref, char refChar, int insertInt);
/* Insert 'insertChar' in 's' at every positin 'ref' has 'refChar'. */

int * gapIntArrayAdjust(int * in, char * ref);
/* Insert space in s when there is a gap ('-') in ref. */

void markCompensatoryMutations(char *s, char *ref, int * pairList, int *markList);
/* Compares s to ref and pairList and sets values in markList
 * according to pairing properties. The value of markList[i] specifies
 * the pairing property of the i'th position. The following values are
 * used: -1: default; 0: ref[i] = s[i] and both compatible with
 * pairList, 1: ref[i] != s[i] and both compatible with pairList; 2:
 * s[i] not compatible with pairList;
 */

int assignBin(double val, double minVal, double maxVal, int binCount);
/* Divide range given by minVal and maxVal into maxBin+1 intervals
   (bins), and return index of the bin val falls into. */

#endif /* RNAUTIL_H */

