/* Copyright (C) 2003 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef ORTHOEVAL_H
#define ORTHOEVAL_H

#ifndef BORF_H
#include "borf.h"
#endif

#ifndef ALTGRAPHX_H
#include "altGraphX.h"
#endif

#ifndef BED_H
#include "bed.h"
#endif

struct orthoEval
/* Evaluation for a single mapped mRNA. */
{
    struct orthoEval *next; /* Next in list. */
    char *orthoBedName;     /* Name of orhtoBed. */
    struct bed *agxBed;     /* Summary of native mRNA's and intronEsts at loci. */
    struct altGraphX *agx;  /* Splicing graph at native loci. */
    struct bed *orthoBed;   /* Bed record created by mapping accession.*/
    struct borf *borf;      /* Results from Victors Solovyev's bestOrf program. */
    int basesOverlap;       /* Number of bases overlapping with transcripts at native loci. */
    char *agxBedName;       /* Name of agxBed that most overlaps, memory not owned here. */
    int numIntrons;         /* Number of introns in orthoBed. Should be orthoBed->exonCount -1. */
    int *inCodInt;         /* Are the introns surrounded in orf (as defined by borf record). */
    int *orientation;       /* Orientation of an intron, -1 = reverse, 0 = not consensus, 1 = forward. */
    int *support;           /* Number of native transcripts supporting each intron. */
    char **agxNames;        /* Names of native agx that support each intron. Memory for names not owned here. */
};

struct orthoEval *orthoEvalLoad(char *row[]);
/* Load a row of strings to an orthoEval. */

void orthoEvalFree(struct orthoEval **pEv);

void orthoEvalTabOut(struct orthoEval *ev, FILE *f);
/* Tab out an orthoEval record. Skipping the agxBed, and agx records. */

struct orthoEval *orthoEvalLoadAll(char *fileName);
/* Load all orthoEval from a whitespace-separated file.
 * Dispose of this with orthoEvalFreeList(). */

void orthoEvalFree(struct orthoEval **pEv);

#endif /* ORTHOEVAL_H */
