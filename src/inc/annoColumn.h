/* annoColumn -- def. of column plus flag for inclusion in output of annoGratorQuery framework */
#ifndef ANNOCOLUMN_H
#define ANNOCOLUMN_H

#include "common.h"
#include "asParse.h"

struct annoColumn
/* A column as defined by autoSql, and whether it is currently set to be included in output. */
    {
    struct annoColumn *next;
    struct asColumn *def;
    boolean included;
    };

struct annoColumn *annoColumnsFromAsObject(struct asObject *asObj);
/* Create a list of columns from asObj; by default, all are set to be included in output.
 * Callers: do not modify any column's def! */

struct annoColumn *annoColumnCloneList(struct annoColumn *list);
/* Shallow-copy a list of annoColumns.  Callers: do not modify any column's def! */

void annoColumnFreeList(struct annoColumn **pList);
/* Shallow-free a list of annoColumns. */

#endif//ndef ANNOCOLUMN_H
