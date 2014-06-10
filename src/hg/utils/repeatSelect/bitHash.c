/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "bitHash.h"
#include "bits.h"

struct hash *newBitHash(struct hash *chromHash)
/* Make another hashtable of bitvectors based on the chroms. */
{
struct hash *ret = newHash(12);
struct hashEl *list = hashElListHash(chromHash), *el;
for (el = list; el != NULL; el = el->next)
    {
    int size = (int)el->val;
    char *chrom = el->name;
    Bits *chromBits = bitAlloc(size);
    hashAdd(ret, chrom, chromBits);
    }
hashElFreeList(&list);
return ret;
}

void freeBitEl(struct hashEl *el)
/* Free a value at an el. */
{
Bits **pEl = (Bits **)&el->val;
bitFree(pEl);
}

void freeBitHash(struct hash **pHash)
/* Free up bit hash table and all values associated with it. */
{
struct hash *hash;
if ((hash = *pHash) != NULL)
    {
    hashTraverseEls(hash, freeBitEl);
    freeHash(pHash);
    }
}

