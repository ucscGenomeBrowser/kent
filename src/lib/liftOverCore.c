/* the basic routine of liftover mapping separated from the other liftOver routines */
/* in src/hg that are more geared for the browser and need MySQL */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "chain.h"
#include "basicBed.h"
#include "portable.h"
#include "obscure.h"
#include "liftOverCore.h"

static int chainAliSize(struct chain *chain)
/* Return size of all blocks in chain. */
{
struct cBlock *b;
int total = 0;
for (b = chain->blockList; b != NULL; b = b->next)
    total += b->qEnd - b->qStart;
return total;
}

static int aliIntersectSize(struct chain *chain, int tStart, int tEnd)
/* How many bases in chain intersect region from tStart to tEnd */
{
int total = 0, one;
struct cBlock *b;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    one = rangeIntersection(tStart, tEnd, b->tStart, b->tEnd);
    if (one > 0)
	total += one;
    }
return total;
}

static boolean mapThroughChain(struct chain *chain, double minRatio, 
	int *pStart, int *pEnd, struct chain **retSubChain,
	struct chain **retChainToFree)
/* Map interval from start to end from target to query side of chain.
 * Return FALSE if not possible, otherwise update *pStart, *pEnd. */
{
struct chain *subChain = NULL;
struct chain *freeChain = NULL;
int s = *pStart, e = *pEnd;
int oldSize = e - s;
int newCover = 0;
int ok = TRUE;

chainSubsetOnT(chain, s, e, &subChain, &freeChain);
if (subChain == NULL)
    {
    *retSubChain = NULL;
    *retChainToFree = NULL;
    return FALSE;
    }
newCover = chainAliSize(subChain);
if (newCover < oldSize * minRatio)
    ok = FALSE;
else if (chain->qStrand == '+')
    {
    *pStart = subChain->qStart;
    *pEnd = subChain->qEnd;
    }
else
    {
    *pStart = subChain->qSize - subChain->qEnd;
    *pEnd = subChain->qSize - subChain->qStart;
    }
*retSubChain = subChain;
*retChainToFree = freeChain;
return ok;
}

char *remapRangeCore(struct hash *chainHash, double minRatio, 
		     int minSizeT, int minSizeQ, 
		     int minChainSizeT, int minChainSizeQ, 
		     char *chrom, int s, int e, char qStrand,
		     int thickStart, int thickEnd, bool useThick,
		     double minMatch,
		     char *regionName, char *db, char *chainTableName,
		     struct chain *(*chainTableCallBack)(char *db, char *chainTableName, char *chrom, int start, int end, int id), 
		     struct bed **bedRet, struct bed **unmappedBedRet)
/* Remap a range through chain hash.  If all is well return NULL
 * and results in a BED (or a list of BED's, if regionName is set (-multiple).
 * Otherwise return a string describing the problem. 
 * note: minSizeT is currently NOT implemented. feel free to add it.
 *       (its not e-s, its the boundaries of what maps of chrom:s-e to Q)
 */
{
struct liftOverChromMap *map = hashFindVal(chainHash, chrom);
struct binElement *list = NULL;
struct binElement *el;
struct chain *chainsHit = NULL, 
                *chainsPartial = NULL, 
                *chainsMissed = NULL, *chain;
struct bed *bedList = NULL, *unmappedBedList = NULL;
struct bed *bed = NULL;
char strand = qStrand;
/* initialize for single region case */
int start = s, end = e;
double minMatchSize = minMatch * (end - start);
int intersectSize;
int tStart;
bool multiple = (regionName != NULL);
if (map)
    list = binKeeperFind(map->bk, start, end);
verbose(2, "%s:%d-%d", chrom, s, e);
verbose(2, multiple ? "\t%s\n": "\n", regionName);
for (el = list; el != NULL; el = el->next)
    {
    chain = el->val;
    if (multiple)
        {
        if (chain->qEnd - chain->qStart < minChainSizeQ ||
            chain->tEnd - chain->tStart < minChainSizeT)
                continue;
        /* limit required match to chain range on target */
        end = min(e, chain->tEnd);
        start = max(s, chain->tStart);
        minMatchSize = minMatch *  (end - start);
        }
    intersectSize = aliIntersectSize(chain, start, end);
    if (intersectSize >= minMatchSize)
	slAddHead(&chainsHit, chain);
    else if (intersectSize > 0)
	{
	slAddHead(&chainsPartial, chain);
	}
    else
	{
        /* shouldn't happen ? */
	slAddHead(&chainsMissed, chain);
	}
    }
slFreeList(&list);

if (chainsHit == NULL)
    {
    if (chainsPartial == NULL)
	return "Deleted in new";
    else if (chainsPartial->next == NULL)
	return "Partially deleted in new";
    else
	return "Split in new";
    }
else if (chainsHit->next != NULL && !multiple)
    {
    return "Duplicated in new";
    }
/* sort chains by position in target to order subregions by orthology */
slSort(&chainsHit, chainCmpTarget);

tStart = s;
struct chain *next = chainsHit->next;
for (chain = chainsHit; chain != NULL; chain = next)
    {
    struct chain *subChain = NULL;
    struct chain *toFree = NULL;
    int start=s, end=e;
    next = chain->next;
    verbose(3,"hit chain %s:%d %s:%d-%d %c (%d)\n",
        chain->tName, chain->tStart,  chain->qName, chain->qStart, chain->qEnd,
        chain->qStrand, chain->id);
    if (multiple)
        {
        /* no real need to verify ratio again (it would require
         * adjusting coords again). */
        minRatio = 0;
        if (db)
            {
            /* use full chain, not the possibly truncated chain
             * from the net */
            struct chain *next = chain->next;
            chain = chainTableCallBack(db, chainTableName,
                                        chrom, s, e, chain->id);
            chain->next = next;
            verbose(3,"chain from db %s:%d %s:%d-%d %c (%d)\n",
                chain->tName, chain->tStart,  chain->qName, 
                chain->qStart, chain->qEnd, chain->qStrand, chain->id);
            }
        }
    if (!mapThroughChain(chain, minRatio, &start, &end, &subChain, &toFree))
        errAbort("Chain mapping error: %s:%d-%d\n", chain->qName, start, end);
    if (chain->qStrand == '-')
	strand = '+';
    if (useThick)
	{
	struct chain *subChain2 = NULL;
	struct chain *toFree2 = NULL;
	if (!mapThroughChain(chain, minRatio, &thickStart, &thickEnd,
		&subChain2, &toFree2))
	    thickStart = thickEnd = start;
	chainFree(&toFree2);
	}
    verbose(3, "mapped %s:%d-%d\n", chain->qName, start, end);
    verbose(4, "Mapped! Target:\t%s\t%d\t%d\t%c\tQuery:\t%s\t%d\t%d\t%c\n",
	    chain->tName, subChain->tStart, subChain->tEnd, strand, 
	    chain->qName, subChain->qStart, subChain->qEnd, chain->qStrand);
    if (multiple && end - start < minSizeQ)
        {
        verbose(2,"dropping %s:%d-%d size %d (too small)\n", 
                       chain->qName, start, end, end - start);
        continue;
        }
    AllocVar(bed);
    bed->chrom = cloneString(chain->qName);
    bed->chromStart = start;
    bed->chromEnd = end;
    if (regionName)
        bed->name = cloneString(regionName);
    bed->strand[0] = strand;
    bed->strand[1] = 0;
    if (useThick)
	{
	bed->thickStart = thickStart;
	bed->thickEnd = thickEnd;
	}
    slAddHead(&bedList, bed);

    if (tStart < subChain->tStart)
        {
        /* unmapped portion of target */
        AllocVar(bed);
        bed->chrom = cloneString(chain->tName);
        bed->chromStart = tStart;
        bed->chromEnd = subChain->tStart;
        if (regionName)
            bed->name = cloneString(regionName);
        slAddHead(&unmappedBedList, bed);
        }
    tStart = subChain->tEnd;
    chainFree(&toFree);
    }
slReverse(&bedList);
*bedRet = bedList;
slReverse(&unmappedBedList);
if (unmappedBedRet)
    *unmappedBedRet = unmappedBedList;
if (bedList==NULL)
    return "Deleted in new";
return NULL;
}
