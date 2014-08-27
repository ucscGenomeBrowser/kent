/* pairDistance - help manage systems where you have a list of distances between pairs of
 * elements */

#ifndef PAIRDISTANCE_H
#define PAIRDISTANCE_H

struct pairDistance
/* A pair of items and the distance between them. */
    {
    struct pairDistance *next;
    char *a;	/* First in pair */
    char *b;	/* Second in pair */
    double distance;  /* Distance between two */
    };

struct pairDistance *pairDistanceReadAll(char *fileName);
/* Read in file of format <a> <b> <distance> into list of pairs */

struct hash *pairDistanceHashList(struct pairDistance *pairList);
/* Return hash of all pairs keyed by pairName function on pair with pair values */

double pairDistanceHashLookup(struct hash *pairHash, char *aName, char *bName);
/* Return distance between a and b. */

void pairDistanceInvert(struct pairDistance *list);
/* Go through and reverse distances, and make them positive. */

char *pairDistanceName(char *a, char *b, char *outBuf, int outBufSize);
/* Return name for pair */

#endif /* PAIRDISTANCE_H */
