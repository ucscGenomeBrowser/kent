/* pslSets - object use to stores sets of psls and match alignments to those
 * other sets */
#include "common.h"
#include "pslSets.h"
#include "psl.h"
#include "pslTbl.h"
#include "localmem.h"
#include "hash.h"


struct pslSets *pslSetsNew(int numSets)
/* construct a new pslSets object */
{
struct pslSets *ps;
AllocVar(ps);
ps->lm = lmInit(1024*1024);
ps->numSets = numSets;
lmAllocArray(ps->lm, ps->sets, numSets);
lmAllocArray(ps->lm, ps->pending, numSets);
return ps;
}

void pslSetsFree(struct pslSets **psPtr)
/* free a pslSets object */
{
struct pslSets *ps = *psPtr;
if (ps != NULL)
    {
    int i;
    for (i = 0; i < ps->numSets; i++)
        pslTblFree(&ps->sets[i]);
    lmCleanup(&ps->lm);
    freeMem(ps);
    *psPtr = NULL;
    }
}

void pslSetsLoadSet(struct pslSets *ps, unsigned iSet, char *pslFile, char *setName)
/* load a set of PSLs */
{
struct pslTbl *pt = pslTblNew(pslFile, setName);
assert(iSet < ps->numSets); 
assert(ps->sets[iSet] == NULL);
ps->sets[iSet] = pt;
ps->nameWidth = max(ps->nameWidth, strlen(setName));
}

static struct pslMatches *pslMatchesAlloc(struct pslSets *ps)
/* allocate a matches object, either new or from the recycled list */
{
struct pslMatches *pm = slPopHead(&ps->matchesPool);
if (pm == NULL)
    {
    lmAllocVar(ps->lm, pm);
    lmAllocArray(ps->lm, pm->psls, ps->numSets);
    }
pm->numSets = ps->numSets;
return pm;
}

static void pslMatchesRecycle(struct pslSets *ps, struct pslMatches *pm)
/* recycle a pslMatches object */
{
struct psl **hold = pm->psls;
memset(pm, 0, sizeof(struct pslMatches));
pm->psls = hold;
memset(pm->psls, 0, ps->numSets*sizeof(struct psl*));
slAddHead(&ps->matchesPool, pm);
}

static struct pslRef *pslRefAlloc(struct pslSets *ps)
/* allocate a matches object, either new or from the recycled list */
{
struct pslRef *pr = slPopHead(&ps->refPool);
if (pr == NULL)
    lmAllocVar(ps->lm, pr);
return pr;
}

static void pslRefRecycle(struct pslSets *ps, struct pslRef *pr)
/* recycle a pslRef object */
{
memset(pr, 0, sizeof(struct pslRef));
slAddHead(&ps->refPool, pr);
}

static void recycleAll(struct pslSets *ps)
/* recycle all pslMatches */
{
struct pslMatches *pm;

while ((pm = slPopHead(&ps->matches)) != NULL)
    pslMatchesRecycle(ps, pm);
}

static struct pslRef *getSetQueryPsls(struct pslSets *ps, int iSet, char *qName)
/* get the pslRefs objects for a query from the given set */
{
struct pslRef *psls = NULL;
struct pslQuery *pq = hashFindVal(ps->sets[iSet]->queryHash, qName);
if (pq != NULL)
    {
    struct psl *psl;
    for (psl = pq->psls; psl != NULL; psl = psl->next)
        {
        struct pslRef *pr = pslRefAlloc(ps);
        pr->psl = psl;
        slAddHead(&psls, pr);
        }
    }
return psls;
}

static boolean isOverlapping(struct psl *psl1, struct psl *psl2)
/* fast check to determine if two psls have any genomic overlap */
{
return (psl1->tStart < psl2->tEnd) &&  (psl1->tEnd > psl2->tStart)
    && (psl1->strand[0] == psl2->strand[0])
    && (psl1->strand[1] == psl2->strand[1])
    && sameString(psl1->tName, psl2->tName);
}

static boolean sameBlockStruct(struct psl *psl1, struct psl *psl2)
/* determine if two psls have the same exon structure; must have checked
 * isOverlapping() first.*/
{
int iBlk;
if (psl1->blockCount != psl2->blockCount)
    return FALSE;
for (iBlk = 0; iBlk < psl1->blockCount; iBlk++)
    {
    if ((psl1->blockSizes[iBlk] != psl2->blockSizes[iBlk])
        || (psl1->tStarts[iBlk] != psl2->tStarts[iBlk])
        || (psl1->qStarts[iBlk] != psl2->qStarts[iBlk]))
        return FALSE;
    }
return TRUE;
}

static int countBlockOverlap(struct psl *psl1, int iBlk1, struct psl *psl2, int iBlk2)
/* count bases of overlap in two blocks */
{
int nOver = 0;

/* find common subblock */
int maxTStart = max(psl1->tStarts[iBlk1], psl2->tStarts[iBlk2]);
int minTEnd = min(psl1->tStarts[iBlk1]+psl1->blockSizes[iBlk1],
                  psl2->tStarts[iBlk2]+psl2->blockSizes[iBlk2]);
if (maxTStart < minTEnd)
    {
    /* find corresponding query subblocks */
    int qStart1 = psl1->tStarts[iBlk1] + (psl1->tStarts[iBlk1]-maxTStart);
    int qStart2 = psl2->tStarts[iBlk2] + (psl2->tStarts[iBlk2]-maxTStart);
    /* if query matches, entire subblock is in common, otherwise none of it
     * matches */
    if (qStart1 == qStart2)
        nOver = minTEnd - maxTStart;
    }
return nOver;
}

static int overlappedBases(struct psl *psl1, struct psl *psl2)
/* count the number identially aligned bases in two psls */
{
int nOver = 0, iBlk1, iBlk2;

/* O(N^2), but should be used infrequently due to exact align check */
for (iBlk1 = 0; iBlk1 < psl1->blockCount; iBlk1++)
    {
    for (iBlk2 = 0; iBlk2 < psl2->blockCount; iBlk2++)
        nOver += countBlockOverlap(psl1, iBlk1, psl2, iBlk2);
    }
return nOver;
}

static struct psl *getBestMatch(struct pslSets *ps, int iSet, struct psl *psl)
/* Get the best match for a psl in a list of psls from another set. */
{
struct pslRef *pr, *prev;
struct pslRef *bestMatch = NULL, *bestPrev = NULL;
int bestOverlap = 0;
struct psl *bestPsl = NULL;

for (prev = NULL, pr = ps->pending[iSet]; pr != NULL; prev = pr, pr = pr->next)
    {
    if (isOverlapping(psl, pr->psl))
        {
        if (sameBlockStruct(psl, pr->psl))
            {
            bestMatch = pr;
            bestPrev = prev;
            break; /* perfect match */
            }
        else
            {
            int over = overlappedBases(psl, pr->psl);
            if ((bestMatch == NULL) || (over > bestOverlap))
                {
                bestMatch = pr;
                bestPrev = prev;
                bestOverlap = over;
                }
            }
        }
    }
if (bestMatch != NULL)
    {
    bestPsl = bestMatch->psl;
    /* unlink from list and recycle */
    if (bestPrev == NULL)
        ps->pending[iSet] = bestMatch->next;
    else
        bestPrev->next = bestMatch->next;
    pslRefRecycle(ps, bestMatch);
    }
return bestPsl;
}

static struct pslMatches *setupMatches(struct pslSets *ps, int iSet, struct psl *psl)
/* initializes a matches object for a psl in a set */
{
struct pslMatches *pm = pslMatchesAlloc(ps);
pm->psls[iSet] = psl;
pm->tName = psl->tName;
pm->tStart = psl->tStart;
pm->tEnd = psl->tEnd;
strcpy(pm->strand, psl->strand);
slAddHead(&ps->matches, pm);
return pm;
}

static void getBestMatches(struct pslSets *ps, int iSet, struct psl *psl)
/* find best matches to this psl in other sets and setup pslMatches object */
{
struct pslMatches *pm = setupMatches(ps, iSet, psl);
int iSet2;

for (iSet2 = iSet+1; iSet2 < ps->numSets; iSet2++)
    {
    struct psl *psl2 = getBestMatch(ps, iSet2, psl);
    if (psl2 != NULL)
        {
        pm->psls[iSet2] = psl2;
        pm->tStart = min(pm->tStart, psl2->tStart);
        pm->tEnd = min(pm->tEnd, psl2->tEnd);
        }
    }
}

static void getSetMatches(struct pslSets *ps, int iSet)
/* find matches for a set  */
{
struct pslRef *pr;
while ((pr = slPopHead(&ps->pending[iSet])) != NULL)
    {
    getBestMatches(ps, iSet, pr->psl);
    pslRefRecycle(ps, pr);
    }
}

static void addQueryNames(struct hash *qNameHash, struct pslTbl *pslTbl)
/* add query names to the hash table if they are not already there. */
{
struct hashCookie hc = hashFirst(pslTbl->queryHash);
struct pslQuery *q;
while ((q = hashNextVal(&hc)) != NULL)
    hashStore(qNameHash, q->qName);  /* add if not there */
}

struct slName *pslSetsQueryNames(struct pslSets *ps)
/* get list of all query names in all tables. slFree when done */
{
struct slName *qNames = NULL;
struct hash *qNameHash = hashNew(21);
int iSet;
struct hashCookie hc;
struct hashEl *hel;

for (iSet = 0; iSet < ps->numSets; iSet++)
    addQueryNames(qNameHash, ps->sets[iSet]);

/* copy to a list */
hc = hashFirst(qNameHash);
while ((hel = hashNext(&hc)) != NULL)
    slSafeAddHead(&qNames, slNameNew(hel->name));
hashFree(&qNameHash);
slNameSort(&qNames);
return qNames;
}

void pslSetsMatchQuery(struct pslSets *ps, char *qName)
/* Get the pslMatches records for a query from all psl sets */
{
int iSet;
recycleAll(ps);

/* gets psls for this query for each set */
for (iSet = 0; iSet < ps->numSets; iSet++)
    ps->pending[iSet] = getSetQueryPsls(ps, iSet, qName);

/* match each set's psl */
for (iSet = 0; iSet < ps->numSets; iSet++)
    getSetMatches(ps, iSet);
}

