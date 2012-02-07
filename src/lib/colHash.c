/* colHash - stuff for fast lookup of index given an
 * rgb value. */

#include "common.h"
#include "memgfx.h"
#include "colHash.h"


struct colHash *colHashNew()
/* Get a new color hash. */
{
struct colHash *cHash;
AllocVar(cHash);
cHash->freeEl = cHash->elBuf;
return cHash;
}

void colHashFree(struct colHash **pEl)
/* Free up color hash. */
{
freez(pEl);
}

struct colHashEl *colHashAdd(struct colHash *cHash, 
	unsigned r, unsigned g, unsigned b, int ix)
/* Add new element to color hash. */
{
struct colHashEl *che = cHash->freeEl++, **pCel;
che->col.r = r;
che->col.g = g;
che->col.b = b;
che->ix = ix;
pCel = &cHash->lists[colHashFunc(r,g,b)];
slAddHead(pCel, che);
return che;
}

struct colHashEl *colHashLookup(struct colHash *cHash, 
	unsigned r, unsigned g, unsigned b)
/* Lookup value in hash. */
{
struct colHashEl *che;
for (che = cHash->lists[colHashFunc(r,g,b)]; che != NULL; che = che->next)
    if (che->col.r == r && che->col.g == g && che->col.b == b)
	return che;
return NULL;
}

