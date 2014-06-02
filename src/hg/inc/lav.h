/* lav.h -- common lav file reading routines */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef LAV_H
#define LAV_H

#include "common.h"
#include "linefile.h"
#include "axt.h"

struct block
/* A block of an alignment. */
    {
    struct block *next;
    int qStart, qEnd;   /* Query position. */
    int tStart, tEnd;   /* Target position. */
    int percentId;      /* Percentage identity. */
    int score;
    };

void parseS(struct lineFile *lf, int *tSize, int *qSize);
/* Parse s stanza and return tSize and qSize */

void parseH(struct lineFile *lf,  char **tName, char **qName, boolean *isRc);
/* Parse out H stanza */

void parseD(struct lineFile *lf, char **matrix, char **command, FILE *f);
/* Parse d stanza and return matrix and blastz command line */

struct block *removeFrayedEnds(struct block *blockList);
/* Remove zero length blocks at start and/or end to 
 * work around blastz bug. */

#endif /* LAV_H */
