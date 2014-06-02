/* minChromSize - routines to calculate the minimum size of
 * all chromosomes needed to cover available annotatins. 
 * Generally this is helpful when constructing binKeepers
 * for random access to annotations. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "bed.h"
#include "minChromSize.h"


struct hash *minChromSizeFromBeds(struct bed *bedList)
/* Go through bed list, creating a hash full of minChromSizes. 
 * This is so we can use binKeeper without user having to pass
 * us list of chromSizes. */
{
struct hash *sizeHash = hashNew(16);
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    struct minChromSize *chrom = hashFindVal(sizeHash, bed->chrom);
    if (chrom == NULL)
        {
	lmAllocVar(sizeHash->lm, chrom);
	chrom->chrom = bed->chrom;
	chrom->minSize = bed->chromEnd;
	hashAdd(sizeHash, bed->chrom, chrom);
	}
    else
        {
	chrom->minSize = max(chrom->minSize, bed->chromEnd);
	}
    }
return sizeHash;
}

struct hash *minChromSizeKeeperHash(struct hash *sizeHash)
/* Return a hash full of binKeepers that match the input sizeHash,
 * (which generally is the output of minChromSizeFromBeds). */
{
struct hashEl *el, *list = hashElListHash(sizeHash);
struct hash *keeperHash = hashNew(16);
for (el = list; el != NULL; el = el->next)
    {
    struct minChromSize *chrom = el->val;
    struct binKeeper *bk = binKeeperNew(0, chrom->minSize);
    hashAdd(keeperHash, chrom->chrom, bk);
    }
hashElFreeList(&list);
return keeperHash;
}

