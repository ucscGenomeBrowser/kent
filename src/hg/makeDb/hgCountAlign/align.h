/* Copyright (C) 2002 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef ALIGN_H
#define ALIGN_H

#include "bits.h"

struct align
/* object used to hold an alignment (as a list of sorted axt objects) and
 * associated information. */
{
    char* tName;
    unsigned tStart;
    unsigned tEnd;
    unsigned tSize;   /* zero if unknown */
    struct axt *head;
    Bits* selectMap;  /* Map of positions to count if not NULL */
};

struct align* alignNew(struct axt* axtList);
/* Construct a new align object. */

void alignFree(struct align** aln);
/* Free a align object. */

struct align* alignLoadAxtFile(char* fname);
/* Construct an align object from an AXT file */

void alignSelectWithBedFile(struct align* aln,
                            char *fname);
/* select positions to count based on a BED file */

#define alignIsSelected(aln, pos) ((aln->selectMap == NULL) \
    || bitReadOne(aln->selectMap, (pos - align->tStart)))
/* Check if a position is selected */

#endif
