/* annoFilter -- autoSql-driven data filtering for annoGratorQuery framework */

#include "annoFilter.h"
#include "sqlNum.h"

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

static boolean annoFilterDouble(struct annoFilter *filter, double val)
/* Compare val to double(s) in filter->values according to filter->op,
 * return TRUE if comparison fails. */
{
double *filterVals = filter->values;
if (filter->op == afInRange)
    {
    double min = filterVals[0], max = filterVals[1];
    return (val < min || val > max);
    }
else
    {
    double threshold = filterVals[0];
    switch (filter->op)
	{
	case afLT:
	    return !(val < threshold);
	case afLTE:
	    return !(val <= threshold);
	case afEqual:
	    return !(val == threshold);
	case afNotEqual:
	    return !(val != threshold);
	case afGTE:
	    return !(val >= threshold);
	case afGT:
	    return !(val > threshold);
	default:
	    errAbort("annoFilterDouble: unexpected filter->op %d for column %s",
		     filter->op, filter->columnDef->name);
	}
    }
return FALSE;
}

static boolean annoFilterLongLong(struct annoFilter *filter, long long val)
/* Compare val to long long(s) in filter->values according to filter->op,
 * return TRUE if comparison fails. */
{
long long *filterVals = filter->values;
if (filter->op == afInRange)
    {
    long long min = filterVals[0], max = filterVals[1];
    return (val < min || val > max);
    }
else
    {
    long long threshold = filterVals[0];
    switch (filter->op)
	{
	case afLT:
	    return !(val < threshold);
	case afLTE:
	    return !(val <= threshold);
	case afEqual:
	    return !(val == threshold);
	case afNotEqual:
	    return !(val != threshold);
	case afGTE:
	    return !(val >= threshold);
	case afGT:
	    return !(val > threshold);
	default:
	    errAbort("annoFilterLongLong: unexpected filter->op %d for column %s",
		     filter->op, filter->columnDef->name);
	}
    }
return FALSE;
}

boolean annoFilterTestRow(struct annoFilter *filterList, char **row, int rowSize)
/* Apply filters to row, using autoSql column definitions to interpret
 * each word of row.  Return TRUE if any filter fails. */
{
if (filterList != NULL && slCount(filterList) != rowSize)
    errAbort("annoFilterTestRow: filterList length %d doesn't match rowSize %d",
	     slCount(filterList), rowSize);
struct annoFilter *filter;
int i;
for (i = 0, filter = filterList;  i < rowSize && filter != NULL;  i++, filter = filter->next)
    {
    if (filter->op == afNoFilter)
	continue;
    if (filter->op == afMatch)
	{
	if (!wildMatch((char *)(filter->values), row[i]))
	    return TRUE;
	}
    else if (filter->op == afNotMatch)
	{
	if (wildMatch((char *)(filter->values), row[i]))
	    return TRUE;
	}
    else
	{
	// row[i] is a number -- integer or floating point?
	enum asTypes type = filter->columnDef->lowType->type;
	if (type == t_double || t_float)
	    {
	    if (annoFilterDouble(filter, sqlDouble(row[i])))
		return TRUE;
	    }
	else if (type == t_char || type == t_int || type == t_uint || type == t_short ||
		 type == t_ushort || type == t_byte || type == t_ubyte ||
		 type == t_off)
	    {
	    if (annoFilterLongLong(filter, sqlLongLong(row[i])))
		return TRUE;
	    }
	else
	    errAbort("annoFilterTestRow: unexpected enum asTypes %d for numeric filter op %d",
		     type, filter->op);
	}
    }
return FALSE;
}
