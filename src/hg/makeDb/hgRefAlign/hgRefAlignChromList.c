#include "hgRefAlignChromList.h"
#include <stdio.h>
#include "refAlign.h"
#include "common.h"
#include "hash.h"
#include "hdb.h"

struct refAlignChrom* refAlignChromListBuild(struct refAlign* refAlignList)
/* split refAlign objects by chromosome, ownership of objects is passed
 * to this list. */
{
struct refAlignChrom* chromList = NULL;
struct refAlignChrom* refAlignChrom;
struct slName* chromName = hAllChromNames();
struct refAlign* refAlign = refAlignList;
struct hash* hash = hashNew(0);

/* set and entry per chromsome and add to hash table */
while (chromName != NULL)
    {
    AllocVar(refAlignChrom);
    refAlignChrom->next = NULL;
    refAlignChrom->slName = chromName;
    refAlignChrom->chrom = chromName->name;
    refAlignChrom->refAlignList = NULL;
    slAddHead(&chromList, refAlignChrom);
    hashAdd(hash, refAlignChrom->chrom, refAlignChrom);

    chromName = chromName->next;
    }
 
/* add refAlign objects to their chromosomes */
while (refAlign != NULL)
    {
    struct refAlign* refAlignNext = refAlign->next;
    refAlignChrom = (struct refAlignChrom*)hashMustFindVal(hash,
                                                           refAlign->chrom);
    slAddHead(&refAlignChrom->refAlignList, refAlign);
    refAlign = refAlignNext;
    }
hashFree(&hash);
return chromList;
}

void refAlignChromListFree(struct refAlignChrom* chromList)
/* free a list of refAlignChrom, frees the refAlign objects  */
{
while (chromList != NULL)
    {
    struct refAlignChrom* chromNext = chromList->next;
    refAlignFreeList(&chromList->refAlignList);
    freeMem(chromList->slName);
    freeMem(chromList);
    chromList = chromNext;
    }
}

