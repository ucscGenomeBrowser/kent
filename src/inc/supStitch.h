/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* supStitch stitches together a bundle of ffAli alignments that share
 * a common query and target sequence into larger alignments if possible.
 * This is commonly used when the query sequence was broken up into
 * overlapping blocks in the initial alignment, and also to look for
 * introns larger than fuzzyFinder can handle. */

#ifndef SUPSTITCH_H
#define SUPSTITCH_H

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif

#ifndef FUZZYFIND_H
#include "fuzzyFind.h"
#endif

#ifndef PATSPACE_H
#include "patSpace.h"
#endif

struct ssFfItem
/* A list of fuzzy finder alignments. */
    {
    struct ssFfItem *next;      /* Next in list. */
    struct ffAli *ff;		/* Alignment (owned by ssFfItem) */
    };

void ssFfItemFree(struct ssFfItem **pEl);
/* Free a single ssFfItem. */

void ssFfItemFreeList(struct ssFfItem **pList);
/* Free a list of ssFfItems. */

struct ssBundle
/* A bunch of alignments all with the same query sequence.  This
 * is the input to the stitcher.*/
    {
    struct ssBundle *next;	/* Next in list. */
    struct ssFfItem *ffList;    /* Item list - memory owned by bundle. */
    bioSeq *qSeq;        /* Query sequence (not owned by bundle.) */
    bioSeq *genoSeq;     /* Genomic sequence (not owned by bundle.) */
    int genoIx;                 /* Index of bac in associated PatSpace. */
    int genoContigIx;           /* Index of contig inside of seq. */
    void *data;			/* User defined data pointer. */
    boolean isProt;		/* True if it's a protein based bundle. */
    struct trans3 *t3List;	/* Sometimes set to three translated frames. */
    boolean avoidFuzzyFindKludge;	/* Temporary flag to avoid call to fuzzyFind. */
    };

void ssBundleFree(struct ssBundle **pEl);
/* Free up one ssBundle */

void ssBundleFreeList(struct ssBundle **pList);
/* Free up list of ssBundles */


void ssStitch(struct ssBundle *bundle, enum ffStringency stringency, 
	int minScore, int maxToReturn);
/* Glue together mrnas in bundle as much as possible. 
 * Updates bundle->ffList with stitched together version. */

struct ssBundle *ssFindBundles(struct patSpace *ps, struct dnaSeq *cSeq, 
	char *cName, enum ffStringency stringency, boolean avoidSelfSelf);
/* Find patSpace alignments on cSeq and bundle them together. */

#endif /* SUPSTITCH_H */

