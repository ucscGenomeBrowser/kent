/* chromKeeper - Wrapper for binKeepers around the whole genome. */
#ifndef CHROMKEEPER_H
#define CHROMKEEPER_H

#ifndef BINRANGE_H
#include "binRange.h"
#endif

void chromKeeperInit(char *db);
/* Initialize the chromKeeper to a given database (hg15,mm2, etc). */

void chromKeeperAdd(char *chrom, int chromStart, int chromEnd, void *val);
/* Add an item to the chromKeeper. */

struct binElement *chromKeeperFind(char *chrom, int chromStart, int chromEnd);
/* Return a list of all items in chromKeeper that intersect range.
   Free this list with slFreeList. */

#endif /* CHROMKEEPER_H */
