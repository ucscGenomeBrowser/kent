/* chainNet - read/write and free nets which are constructed
 * of chains of genomic alignments. */

/* Copyright (C) 2003 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef CHAINNET_H
#define CHAINNET_H

#ifndef BITS_H
#include "bits.h"
#endif

struct chainNet
/* A net on one chromosome. */
    {
    struct chainNet *next;
    char *name;			/* Chromosome name. */
    int size;			/* Chromosome size. */
    struct cnFill *fillList;	/* Top level fills. */
    struct hash *nameHash; 	/* Hash of all strings in fillList. */
    };

struct cnFill
/* Filling sequence or a gap. */
    {
	/* Required fields */
    struct cnFill *next;	   /* Next in list. */
    int tStart, tSize;	   /* Range in target chromosome. */
    char *qName;	   /* Other chromosome (not allocated here) */
    char qStrand;	   /* Orientation + or - in other chrom. */
    int qStart,	qSize;	   /* Range in query chromosome. */
    struct cnFill *children; /* List of child gaps. */
	/* Optional fields. */
    int chainId;   /* Chain id.  0 for a gap. */
    double score;  /* Score of associated chain. 0 if undefined. */
    int ali;       /* Bases in non-gapped alignments. */
    int qOver;     /* Overlap with parent in query if syntenic or inverted. */
    int qFar;      /* How far away is parent in query if syntenic or inverted. */
    int qDup;	   /* Number of bases that are duplicated. */
    char *type;    /* top/syn/inv/nonSyn */
    int tN;	   /* Count of N's in target chromosome or -1 */
    int qN;	   /* Count of N's in query chromosome or -1 */
    int tR;	   /* Count of repeats in target chromosome or -1 */
    int qR;	   /* Count of repeats in query chromosome or -1 */
    int tNewR;	   /* Count of new (lineage specific) repeats in target */
    int qNewR;	   /* Count of new (lineage specific) repeats in query */
    int tOldR;	   /* Count of ancient repeats (pre-split) in target */
    int qOldR;	   /* Count of ancient repeats (pre-split) in query */
    int qTrf;	   /* Count of simple repeats, period 12 or less. */
    int tTrf;	   /* Count of simple repeats, period 12 or less. */
    };

void chainNetFree(struct chainNet **pNet);
/* Free up a chromosome net. */

void chainNetFreeList(struct chainNet **pList);
/* Free up a list of chainNet. */

void chainNetWrite(struct chainNet *net, FILE *f);
/* Write out chain net. */

struct chainNet *chainNetRead(struct lineFile *lf);
/* Read next net from file. Return NULL at end of file.*/


struct cnFill *cnFillNew();
/* Return fill structure with some basic stuff filled in */

void cnFillFree(struct cnFill **pFill);
/* Free up a fill structure and all of it's children. */

void cnFillFreeList(struct cnFill **pList);
/* Free up a list of fills. */

int cnFillCmpTarget(const void *va, const void *vb);
/* Compare to sort based on target. */

struct cnFill *cnFillRead(struct chainNet *net, struct lineFile *lf);
/* Recursively read in list and children from file. */

struct cnFill *cnFillFromLine(struct hash *nameHash, 
	struct lineFile *lf, char *line);
/* Create cnFill structure from line.  This will chop up
 * line as a side effect. */

void cnFillWrite(struct cnFill *fillList, FILE *f, int depth);
/* Recursively write out fill list. */

void chainNetMarkUsed(struct chainNet *net, Bits *bits, int bitCount);
/* Fill in a bit array with 1's corresponding to
 * chainId's used in net file. */

#endif /* CHAINNET_H */


