/* visiSearch - free form search of visiGene database. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef VISISEARCH_H
#define VISISEARCH_H

#ifndef BITS_H
#include "bits.h"
#endif

struct visiMatch
/* Info on a score of an image in free format search. */
    {
    struct visiMatch *next;
    int imageId;	/* Image ID associated with search. */
    double weight;	/* The higher the weight the better the match (ignored for now) */
    Bits *wordBits;	/* A bit set for each matching word in search. */
    };

struct visiMatch *visiMatchNew(int imageId, int wordCount);
/* Create a new visiMatch structure, as yet with no weight. */

void visiMatchFree(struct visiMatch **pMatch);
/* Free up memory associated with visiMatch */

void visiMatchFreeList(struct visiMatch **pList);
/* Free up memory associated with list of visiMatch */

int visiMatchCmpWeight(const void *va, const void *vb);
/* Compare to sort based on match. */

struct visiMatch *visiSearch(struct sqlConnection *conn, char *searchString);
/* visiSearch - return list of images that match searchString sorted
 * by how well they match. This will search most fields in the
 * database. */

extern char titleMessage[1024];

#endif /* VISISEARCH_H */
