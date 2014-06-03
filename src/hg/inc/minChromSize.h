/* minChromSize - routines to calculate the minimum size of
 * all chromosomes needed to cover available annotations. 
 * Generally this is helpful when constructing binKeepers
 * for random access to annotations. */

/* Copyright (C) 2007 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef MINCHROMSIZE_H
#define MINCHROMSIZE_H

#ifndef BED_H
#include "bed.h"
#endif

#ifndef BINRANGE_H
#include "binRange.h"
#endif

struct minChromSize
/* Associate chromosome and size. */
    {
    char *chrom;	/* Chromosome name, Not alloced here */
    int minSize;	/* Minimum size of chromosome. */
    };

struct hash *minChromSizeFromBeds(struct bed *bedList);
/* Go through bed list, creating a hash full of minChromSizes. 
 * This is so we can use binKeeper without user having to pass
 * us list of chromSizes. */

struct hash *minChromSizeKeeperHash(struct hash *sizeHash);
/* Return a hash full of binKeepers that match the input sizeHash,
 * (which generally is the output of minChromSizeFromBeds). */

#endif /* MINCHROMSIZE_H */
