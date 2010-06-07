/* Object the maps ranges to chromAnn objects */
#ifndef CHROMANNMAP_H
#define CHROMANNMAP_H
struct chromAnnMap;
struct chromAnn;

#include "hash.h"

struct chromAnnMapIter
/* iterator over a chromAnnMap */
{
    struct chromAnnMap *cam;
    struct hashCookie chromCookie;
    struct range *currentRange;
    struct chromAnn *currentCa;
};

struct chromAnnMap *chromAnnMapNew();
/* construct a new object */

void chromAnnMapAdd(struct chromAnnMap *cam,
                    struct chromAnn *ca);
/* add a record to the table. */

struct chromAnnRef *chromAnnMapFindOverlap(struct chromAnnMap *cam,
                                           struct chromAnn *ca);
/* get list of overlaps to ca */

struct chromAnnMapIter chromAnnMapFirst(struct chromAnnMap *cam);
/* get iterator over a chromAnnMap.  EXTREME DANGER: only one can be
 * active on the same map. */

struct chromAnn *chromAnnMapIterNext(struct chromAnnMapIter *iter);
/* next element in select table.  EXTREME DANGER: only one can be
 * active on the same map. */

void chromAnnMapFree(struct chromAnnMap **camPtr);
/* free chromAnnMap structures. */

#endif
