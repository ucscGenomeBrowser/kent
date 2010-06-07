/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* crudeAli.h - Interface to the fast, crude blast-like aligner. */

#ifndef CRUDEALI_H
#define CRUDEALI_H

struct crudeAli
/* Stored info on a crude alignment. */
    {
    struct crudeAli *next;
    int chromIx;
    int start, end;
    int score;
    char strand;
    int qStart, qEnd;
    };

struct crudeAli *crudeAliFind(DNA *probe, int probeSize, struct nt4Seq **chrome, int chromeCount, int tileSize, int minScore);
/* Returns a list of crude alignments.  (You can free this with slFreeList() */

#endif /* CRUDEALI_H */

