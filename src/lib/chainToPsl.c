/* chainToPsl - convert between chains and psl.  Both of these
 * are alignment formats that can handle gaps in both strands
 * and do not include the sequence itself. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "psl.h"
#include "chain.h"


struct psl *chainToPsl(struct chain *chain)
/* chainToPsl - convert chain to psl.  This does not fill in
 * all the fields perfectly,  but it does get the starts
 * and sizes right. */
{
struct psl *psl;
int blockCount, i;
struct boxIn *b;

blockCount = slCount(chain->blockList);
AllocVar(psl);
AllocArray(psl->blockSizes, blockCount);
AllocArray(psl->qStarts, blockCount);
AllocArray(psl->tStarts, blockCount);
psl->strand[0] = chain->qStrand;
psl->qName = cloneString(chain->qName);
psl->qSize = chain->qSize;
if (chain->qStrand == '-')
    {
    psl->qStart = chain->qSize - chain->qEnd;
    psl->qEnd = chain->qSize - chain->qStart;
    }
else
    {
    psl->qStart = chain->qStart;
    psl->qEnd = chain->qEnd;
    }
psl->tName = cloneString(chain->tName);
psl->tSize = chain->tSize;
psl->tStart = chain->tStart;
psl->tEnd = chain->tEnd;
psl->blockCount = blockCount;
for (i=0, b=chain->blockList; i < blockCount; ++i, b = b->next)
    {
    psl->tStarts[i] = b->tStart;
    psl->qStarts[i] = b->qStart;
    psl->blockSizes[i] = b->qEnd - b->qStart;
    }
return psl;
}


