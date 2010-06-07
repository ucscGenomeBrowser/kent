/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* unfin - things to help handle unfinished (fragmented) DNA sequences). */
#ifndef UNFIN_H
#define UNFIN_H

enum {contigPad = 800,};
/* How may N's between contigs. */

struct contigTree
/* A hierarchical structure of contigs.  A forest of these is
 * maintained by the system.  (No need to free these.) */
    {
    struct contigTree  *next;    /* Sibling. */
    struct contigTree *children; /* Sub-contigs. */
    struct contigTree *parent;   /* Parent of contig. */
    char *id;	       /* Ensemble ID. (Not allocated here.) */
    int submitLength;  /* Number of bases in submission. */
    int submitOffset;  /* Offset relative to parent in genBank submission. */
    int orientation;   /* +1 or -1. Strand relative to parent.*/
    int corder;        /* Order of contig in genBank submission. */
    int browserOffset; /* Offset relative to parent for browser, ordered by ensemble, with
                        * ensContigPad N's between each contig. */ 
    int browserLength; /* Size including padding. */
    };

#endif /* UNFIN_H */

