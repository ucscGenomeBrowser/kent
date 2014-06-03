/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "repeatGroup.h"

void repeatGroupFree(struct repeatGroup **pRG)
/* Free up a repeatGroup item. */
{
if ((pRG != NULL) && (*pRG != NULL))
    freeMem((*pRG)->name);
freez(pRG);
}

void repeatGroupFreeList(struct repeatGroup **pList)
/* Free up a list of repeatGroups */
{
struct repeatGroup *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    repeatGroupFree(&el);
    }
*pList = NULL;
}

int repeatGroupCopyCmp(const void *va, const void *vb)
/* Compare by number of copies. */
{
const struct repeatGroup *a = *((struct repeatGroup **)va);
const struct repeatGroup *b = *((struct repeatGroup **)vb);
return b->copies - a->copies;
}

int repeatGroupBasesCmp(const void *va, const void *vb)
/* Compare by number of bases covered for all copies of repeats. */
{
const struct repeatGroup *a = *((struct repeatGroup **)va);
const struct repeatGroup *b = *((struct repeatGroup **)vb);
return b->bases - a->bases;
}

int repeatGroupLenCmp(const void *va, const void *vb)
/* Compare by mean length of a repeat. */
{
const struct repeatGroup *a = *((struct repeatGroup **)va);
const struct repeatGroup *b = *((struct repeatGroup **)vb);
if (b->meanLength > a->meanLength)
    return 1;
else if (b->meanLength < a->meanLength)
    return -1;
return 0;
}

int repeatGroupScoreCmp(const void *va, const void *vb)
/* Compare by mean score of a repeat. */
{
const struct repeatGroup *a = *((struct repeatGroup **)va);
const struct repeatGroup *b = *((struct repeatGroup **)vb);
if (b->meanScore > a->meanScore)
    return 1;
else if (b->meanScore < a->meanScore)
    return -1;
return 0;
}
