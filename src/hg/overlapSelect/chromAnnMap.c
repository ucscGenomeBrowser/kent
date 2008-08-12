/* Object the maps ranges to chromAnn objects */
#include "common.h"
#include "chromAnnMap.h"
#include "chromAnn.h"
#include "genomeRangeTree.h"

struct chromAnnMap
/* Object the maps ranges to chromAnn objects */
{
    struct genomeRangeTree* ranges; // values are lists of chromAnn object
};

struct chromAnnMap *chromAnnMapNew()
/* construct a new object */
{
struct chromAnnMap *cam;
AllocVar(cam);
cam->ranges = genomeRangeTreeNew();
return cam;
}

void chromAnnMapAdd(struct chromAnnMap *cam,
                    struct chromAnn *ca)
/* add a record to the table. */
{
/* don't add if zero-length, they can't select */
if (ca->start < ca->end)
    genomeRangeTreeAddValList(cam->ranges, ca->chrom, ca->start, ca->end, ca);
else
    chromAnnFree(&ca);
}

struct chromAnnRef *chromAnnMapFindOverlap(struct chromAnnMap *cam,
                                           struct chromAnn *ca)
/* get list of overlaps to ca */
{
struct range *or, *overs
    = genomeRangeTreeAllOverlapping(cam->ranges, ca->chrom, ca->start, ca->end);
struct chromAnnRef *overlaps = NULL;
for (or = overs; or != NULL; or = or->next)
    {
    struct chromAnn *ca;
    for (ca = or->val; ca != NULL; ca = ca->next)
        chromAnnRefAdd(&overlaps, ca);
    }
return overlaps;
}

struct chromAnnMapIter chromAnnMapFirst(struct chromAnnMap *cam)
/* get iterator over a chromAnnMap */
{
struct chromAnnMapIter iter;
ZeroVar(&iter);
iter.cam = cam;
iter.chromCookie = hashFirst(cam->ranges->hash);
return iter;
}

struct chromAnn *chromAnnMapIterNext(struct chromAnnMapIter *iter)
/* next element in select table */
{
if (iter->currentCa == NULL)
    {
    // no more chromAnns in this range
    if (iter->currentRange == NULL)
        {
        // no more ranges on this chrom
        struct hashEl *chromEl = hashNext(&iter->chromCookie);
        if (chromEl == NULL)
            return NULL;  // no more chroms
        iter->currentRange = genomeRangeTreeList(iter->cam->ranges, chromEl->name);
        }
    iter->currentCa = iter->currentRange->val;
    iter->currentRange = iter->currentRange->next;
    }
struct chromAnn *ca = iter->currentCa;
iter->currentCa = iter->currentCa->next;
return ca;
}

void chromAnnMapFree(struct chromAnnMap **camPtr)
/* free chromAnnMap structures. */
{
struct chromAnnMap *cam = *camPtr;
if (cam != NULL)
    {
    struct hashCookie chromCookie = hashFirst(cam->ranges->hash);
    struct hashEl *chromEl;
    for (chromEl = hashNext(&chromCookie); chromEl != NULL; chromEl = chromEl->next)
        {
        struct range *r, *ranges = genomeRangeTreeList(cam->ranges, chromEl->name);
        for (r = ranges; r != NULL; r = r->next)
            {
            struct chromAnn *ca, *cas = r->val;
            while ((ca = slPopHead(&cas)) != NULL)
                chromAnnFree(&ca);
            }
        }
    freez(camPtr);
    }
}


