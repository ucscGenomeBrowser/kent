/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* ggPrivate - Some stuff that's shared between the various
 * geneGraph making modules, but that generally should
 * not be used by other routines. */

#ifndef GENEGRAPH_H
#include "geneGraph.h"
#endif

struct ggMrnaBlock
/* A single block of an mRNA alignment. */
    {
    int qStart, qEnd;           /* Start and end position in cDNA. */
    int tStart, tEnd;           /* Start and end position in genome. */
    };

struct ggMrnaAli
/* An mRNA alignment */
    {
    struct ggMrnaAli *next;
    char *name;
    HGID id;		 /* ID in database. */
    int baseCount;
    short milliScore;    /* Score 0-1000 */
    bits16 seqIx;        /* Which target sequence. */ 
    short strand;        /* Strand of chromosome cDNA aligns with +1 or -1 */
    short direction;     /* Direction of cDNA relative to transcription +1 or -1 */
    bool hasIntrons;
    short orientation;    /* +1 or -1 depending on whether *clone* is + or - WRT chrom . */
                          /* New and perhaps not always respected. */
    int contigStart, contigEnd;	     /* Target start and end. */
    short blockCount;                /* Number of blocks. */
    struct ggMrnaBlock *blocks;        /* Dynamically allocated array. */
    };

struct ggMrnaInput
/* This holds the input for the core clustering algorithm. */
    {
    int seqCount;		/* Count of target sequences. */
    struct dnaSeq *seqList;	/* Linked list of target sequences. */
    struct dnaSeq **seqArray;	/* Array of same target sequences. */
    HGID *seqIds;               /* IDs of target sequences. */
    struct ggMrnaAli *maList;     /* List of alignments. */
    };

#ifdef LATER
struct clonePair
/* A pair of ESTs from the same clone. */
    {
    struct clonePair *next;	/* Next in list. */
    struct ggMrnaAli *p5, *p3;	/* Start/end of pair. */
    int seqIx;                /* Chromosome (or contig) index. */
    int tStart,tEnd;		/* Positions within target sequence. */
    short strand;                /* Strand this is on. */
    };
#endif

struct maRef 
/* Holds a reference to a ma */
    {
    struct maRef *next;  /* Next in list. */
    struct ggMrnaAli *ma;   /* Cdna alignment info. */
    };

struct ggMrnaCluster
/* Input structure for gene grapher */
    {
    struct ggMrnaCluster *next;             /* Next in list. */
    struct maRef *refList;                  /* Full cDNA alignments. */
    struct ggAliInfo *mrnaList;             /* List of compact cDNA alignments. */
    int orientation;			    /* +1 or -1 strand of genomic DNA. */
    int tStart,tEnd;                        /* Position in genomic DNA. */
    int seqIx;                              /* Which target sequence. */
    };

void asciiArtDumpDa(struct ggAliInfo *da, int start, int end);
/* Display list of dense Alis in artistic ascii format. */

void dumpMc(struct ggMrnaCluster *mc);
/* Display mc on the screen. */

void dumpGg(struct geneGraph *gg);
/* Display a gene graph. */


