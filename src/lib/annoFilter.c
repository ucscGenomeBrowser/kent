/* annoFilter -- autoSql-driven data filtering for annoGratorQuery framework */

#include "annoFilter.h"

struct annoFilter *annoFiltersFromAsObject(struct asObject *asObj)
/* Translate a table's autoSql representation into a list of annoFilters that make
 * sense for the table's set of fields.
 * Callers: do not modify any filter's columnDef! */
{
struct annoFilter *filterList = NULL;
struct asColumn *def;
for (def = asObj->columnList;  def != NULL;  def = def->next)
    {
    struct annoFilter *newF;
    AllocVar(newF);
    newF->columnDef = def;
    newF->op = afNoFilter;
    }
slReverse(&filterList);
return filterList;
}

struct annoFilter *annoFilterCloneList(struct annoFilter *list)
/* Shallow-copy a list of annoFilters.  Callers: do not modify any filter's columnDef! */
{
struct annoFilter *newList = NULL, *oldF;
for (oldF = list;  oldF != NULL;  oldF = oldF->next)
    {
    struct annoFilter *newF = CloneVar(oldF);
    slAddHead(&newList, newF);
    }
slReverse(&newList);
return newList;
}

void annoFilterFreeList(struct annoFilter **pList)
/* Shallow-free a list of annoFilters. */
{
slFreeList(pList);
}
