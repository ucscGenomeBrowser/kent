/* xenalign.h - Do cross-species DNA alignments from scratch. 
 *
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef XENALIGN_H
#define XENALIGN_H

#ifndef NT4_H
#include "nt4.h"
#endif

int xenAlignSmall(DNA *query, int querySize, DNA *target, int targetSize, 
    FILE *f, boolean printExtraAtEnds);
/* Use dynamic programming to do small scale 
 * (querySize * targetSize < 10,000,000)
 * alignment of DNA. Write results into f.*/

void xenStitch(char *inName, FILE *out, int stitchMinScore, 
    boolean compactOutput);
/* Do the big old stitching run putting together contents of inName
 * into contigs in out.  Create output in newer format. */

void xenStitcher(char *inName, FILE *out, int stitchMinScore, boolean 
    compactOutput);
/* Do the big old stitching run putting together contents of inName
 * into contigs in out. Create output in older format. */

void xenAlignBig(DNA *query, int qSize, DNA *target, int tSize, FILE *f, boolean forHtml);
/* Do a big alignment - one that has to be stitched together. 
 * Write results into f. */

void xenShowAli(char *qSym, char *tSym, char *hSym, int symCount, FILE *f,
   int qOffset, int tOffset, char qStrand, char tStrand, int maxLineSize);
/* Print alignment and HMM symbols maxLineSize bases at a time to file. */

void xenAlignWorm(DNA *query, int qSize, FILE *f, boolean forHtml);
/* Do alignment against worm genome. */

#endif /* XENALIGN_H */
