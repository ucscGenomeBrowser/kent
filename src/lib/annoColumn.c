/* annoColumn -- def. of column plus flag for inclusion in output of annoGratorQuery framework */

#include "annoColumn.h"

struct annoColumn *annoColumnsFromAsObject(struct asObject *asObj)
/* Create a list of columns from asObj; by default, all are set to be included in output.
 * Callers: do not modify any column's def! */
{
struct annoColumn *colList = NULL;
struct asColumn *asCol;
for (asCol = asObj->columnList;  asCol != NULL;  asCol = asCol->next)
    {
    struct annoColumn *col;
    AllocVar(col);
    col->def = asCol;
    col->included = TRUE;
    slAddHead(&colList, col);
    }
slReverse(&colList);
return colList;
}

struct annoColumn *annoColumnCloneList(struct annoColumn *list)
/* Shallow-copy a list of annoColumns.  Callers: do not modify any column's def! */
{
struct annoColumn *newList = NULL, *oldC;
for (oldC = list;  oldC != NULL;  oldC = oldC->next)
    {
    struct annoColumn *newC = CloneVar(oldC);
    slAddHead(&newList, newC);
    }
slReverse(&newList);
return newList;
}

void annoColumnFreeList(struct annoColumn **pList)
/* Shallow-free a list of annoColumns.  Does not free any column's def. */
{
slFreeList(pList);
}

