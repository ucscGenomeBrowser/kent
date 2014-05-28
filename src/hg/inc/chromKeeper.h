/* chromKeeper - Wrapper for binKeepers around the whole genome. */

/* Copyright (C) 2004 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#ifndef CHROMKEEPER_H
#define CHROMKEEPER_H

#ifndef BINRANGE_H
#include "binRange.h"
#endif

void chromKeeperInit(char *db);
/* Initialize the chromKeeper to a given database (hg15,mm2, etc). */

void chromKeeperInitChroms(struct slName *nameList, int maxChromSize);
/* Initialize a chrom keeper with a list of names and a size that
   will be used for each one. */

void chromKeeperAdd(char *chrom, int chromStart, int chromEnd, void *val);
/* Add an item to the chromKeeper. */

struct binElement *chromKeeperFind(char *chrom, int chromStart, int chromEnd);
/* Return a list of all items in chromKeeper that intersect range.
   Free this list with slFreeList. */

void chromKeeperRemove(char *chrom, int chromStart, int chromEnd, void *val);
/* Remove the item from the proper chromKeeper. */

#endif /* CHROMKEEPER_H */
