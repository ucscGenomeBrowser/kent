/* cda.h - cDNA Alignment structure.  This stores all the info except
 * the bases themselves on an cDNA alignment. 
 * 
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef CDA_H
#define CDA_H

#ifndef MEMGFX_H
#include "memgfx.h"
#endif 

#ifndef FUZZYFIND_H
#include "fuzzyFind.h"
#endif

struct cdaBlock
    {
    int nStart, nEnd;           /* Start and end position in cDNA. */
    int hStart, hEnd;           /* Start and end position on chromosome. */
    UBYTE startGood, endGood;   /* Number of bases matching perfectly on end */
    UBYTE midScore;             /* 0-255  255 is perfect. */
    };

struct cdaAli
    {
    struct cdaAli *next;
    char *name;
    int baseCount;
    short milliScore;               /* Score 0-1000 */
    bits16 chromIx;                 /* On disk as just a UBYTE */
    char strand;                    /* Strand of chromosome cDNA aligns with + or - */
    char direction;                 /* Direction of cDNA relative to transcription + or - */
    UBYTE isEmbryonic;
    UBYTE hasIntrons;
    int orientation;                /* +1 or -1 depending on whether *clone* is + or - WRT chrom . */
                                    /* New and perhaps not always respected. */
    int chromStart, chromEnd;
    short blockCount;               /* Number of blocks. */
    struct cdaBlock *blocks;        /* Dynamically allocated array. */
    };

boolean cdaCloneIsReverse(struct cdaAli *cda);
/* Returns TRUE if clone (.3/.5 pair) aligns on reverse strand. */

char cdaCloneStrand(struct cdaAli *cda);
/* Return '+' or '-' depending on the strand that clone (.3/.5 pair) aligns on. */

char cdaDirChar(struct cdaAli *cda, char chromStrand);
/* Return '>' or '<' or ' ' depending whether cDNA is going same, opposite, or
 * unknown alignment as the chromosome strand. */

char *cdaLoadString(FILE *f);
/* Load in a string from CDA file. */

void cdaReadBlock(FILE *f, struct cdaBlock *block);
/* Read one block from cda file. */

FILE *cdaOpenVerify(char *fileName);
/* Call this to open file and verify signature, then call cdaLoadOne
 * which returns NULL at EOF. This file type is created by binGood.exe. */

struct cdaAli *cdaLoadOne(FILE *f);
/* Load one cdaAli from file.  Assumes file pointer is correctly positioned.
 * either by cdaOpenVerify or a previous cdaLoadOne.  Returns NULL at EOF. */

void cdaFixChromStartEnd(struct cdaAli *cda);
/* Loop through blocks and figure out and fill in chromStart
 * and chromEnd. */

void cdaCoalesceBlocks(struct cdaAli *ca);
/* Coalesce blocks separated by small amounts of noise. */

void cdaCoalesceFast(struct cdaAli *ca);
/* Coalesce blocks as above, but don't update the score. */

void cdaShowAlignmentTrack(struct memGfx *mg, 
    int xOff, int yOff, int width, int height,  Color goodColor, Color badColor,
    int dnaSize, int dnaOffset, struct cdaAli *cda, char repeatChar);
/* Draw alignment on a horizontal track of picture. */

void cdaRcOne(struct cdaAli *cda, int dnaStart, int baseCount);
/* Reverse complement one cda. DnaStart is typically display window start. */

void cdaRcList(struct cdaAli *cdaList, int dnaStart, int baseCount);
/* Reverse complement cda list. */

void cdaFreeAli(struct cdaAli *ca);
/* Free a single cdaAli. */

void cdaFreeAliList(struct cdaAli **pList);
/* Free list of cdaAli. */

struct cdaAli *cdaAliFromFfAli(struct ffAli *aliList, 
    DNA *needle, int needleSize, DNA *hay, int haySize, boolean isRc);
/* Convert from ffAli to cdaAli format. */

void cdaWrite(char *fileName, struct cdaAli *cdaList);
/* Write out a cdaList to a cda file. */

#endif /* CDA_H */
