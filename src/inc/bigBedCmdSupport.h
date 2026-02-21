/* bigBedCmdSupport - functions to support writing bigBed related commands. */

/* Copyright (C) 2022 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef bigBedCmdSupport_h
#define bigBedCmdSupport_h
#include "bigBed.h"

void bigBedCmdOutputHeader(struct bbiFile *bbi, FILE *f);
/* output a autoSql-style header from the autoSql in the file */

void bigBedCmdOutputTsvHeader(struct bbiFile *bbi, FILE *f);
/* output a TSV-style header from the autoSql in the file */

struct hash *makeChromHash(struct bbiChromInfo *chromList);
/* make a fast searchable hash from chromList */

struct bed *bed3FromPositions(char *fileName);
/* Read positions file and retrun bed 3 list. */

void genericBigToNonBigFromBed(struct bbiFile *bbi, struct hash *chromHash, char *bedFileName, FILE *outFile,  
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, int start, int end, char *bedName, FILE *f)
);
/* Read list of ranges from bed file chrom start end.
 * Automatically sort them by chrom, start */

void genericBigToNonBigFromRange(struct bbiFile *bbi, struct hash *chromHash, FILE *outFile, struct slName *ranges, 
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, int start, int end, char *bedName, FILE *f)
);
/* Read list of ranges from commandline option as chrom start end or chrom:start-end.
 * Supports multiple -range options.
 * Automatically sort them by chrom, start. */


void genericBigToNonBigFromPos(struct bbiFile *bbi, struct hash *chromHash, char *posFileName, FILE *outFile, 
 void (*processChromChunk)(struct bbiFile *bbi, char *chrom, int start, int end, char *bedName, FILE *f)
);
/* Read  positions from file (chrom:start-end). starts are 1-based,
 * but after conversion to bed3 list, they are 0-based. 
 * Automatically sort them by chrom, start */

#endif
