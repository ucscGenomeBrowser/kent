/* ooc.h - Stuff to handle overused N-mers (tiles) in genome
 * indexing schemes. */
/* Copyright 2001-2002 Jim Kent.  All rights reserved. */

#ifndef OOC_H
#define OOC_H

void oocMaskCounts(char *oocFile, bits32 *tileCounts, int tileSize, bits32 maxPat);
/* Set items of tileCounts to maxPat if they are in oocFile. 
 * Effectively masks this out of index.*/

void oocMaskSimpleRepeats(bits32 *tileCounts, int seedSize, bits32 maxPat);
/* Mask out simple repeats in index . */

#endif /* OOC_H */

