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


void asciiArtDumpDa(struct ggAliInfo *da, int start, int end);
/* Display list of dense Alis in artistic ascii format. */

void dumpMc(struct ggMrnaCluster *mc);
/* Display mc on the screen. */

void dumpGg(struct geneGraph *gg);
/* Display a gene graph. */


