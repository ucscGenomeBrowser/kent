/* crudeAli.h - Interface to the fast, crude blast-like aligner. 
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

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

