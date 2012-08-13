/* annoFilter -- autoSql-driven data filtering for annoGratorQuery framework */

#ifndef ANNOFILTER_H
#define ANNOFILTER_H

#include "common.h"
#include "asParse.h"

enum annoFilterOp
/* Types of filtering actions; most use a value defined elsewhere in annoFilter. */
// NOTE: would be nice to have set ops, afInSet & afNotInSet
    {
    afNoFilter,		// Any column value passes filter
    afMatch,		// Stringish column value must match given value(s)
    afNotMatch,		// Stringish column value must not match given value(s)
    afLT,		// Numeric column value is less than a given value
    afLTE,		// Numeric column value is less than or equal to a given value
    afEqual,		// Numeric column value equals a given value
    afNotEqual,		// Numeric column value does not equal a given value
    afGTE,		// Numeric column value is greater than or equal to a given value
    afGT,		// Numeric column value is greater than a given value
    afInRange,		// Numeric column value falls within fully-closed [given min, given max]
    };

union aNumber
    {
    long long anInt;
    long double aDouble;
    };

struct annoFilter
/* A filter on a row or specific column of data: operand and value(s). This info can tell the
 * UI what filtering options to offer and what their current values are; and it tells the
 * annoStreamer what filters to apply. */
    {
    struct annoFilter *next;
    boolean (*filterFunc)(struct annoFilter *self, char **row, int rowSize);
				// If non-NULL, pass whole row to this function.
    char *label;		// Option label for UI
    int columnIx;		// If filterFunc is NULL, index of column in row
    enum asTypes type;		// Data type (determines which filter operations are applicable)
    enum annoFilterOp op;	// Action to be performed
    void *values;		// Depending on op: name(s) to match, thresholds to compare, etc.
    boolean isExclude;		// True if we are excluding items that pass, false if including.
    boolean rightJoin;		// A la SQL: if this filter fails, discard primary row.
    boolean hasMinMax;		// True if we know the allowable range for numeric thresholds
				// so e.g. UI can enforce limits
    union aNumber min;		// Lowest valid threshold value
    union aNumber max;		// Highest valid threshold value
    };

struct annoFilter *annoFiltersFromAsObject(struct asObject *asObj);
/* Translate a table's autoSql representation into a list of annoFilters that make
 * sense for the table's set of fields. */

boolean annoFilterRowFails(struct annoFilter *filterList, char **row, int rowSize,
			   boolean *retRightJoin);
/* Apply filters to row, using autoSql column definitions to interpret
 * each word of row.  Return TRUE if any filter fails (or passes, if isExclude).
 * Set retRightJoin to TRUE if a rightJoin filter has failed. */

boolean annoFilterWigValueFails(struct annoFilter *filterList, double value,
				boolean *retRightJoin);
/* Apply filters to value.  Return TRUE if any filter fails (or passes, if isExclude).
 * Set retRightJoin to TRUE if a rightJoin filter has failed. */

struct annoFilter *annoFilterCloneList(struct annoFilter *list);
/* Copy a list of annoFilters. */

void annoFilterFreeList(struct annoFilter **pList);
/* Free a list of annoFilters. */

enum annoFilterOp afOpFromString(char *string);
/* Translate string (e.g. "afNotEqual") into enum value (e.g. afNotEqual). */

char *stringFromAfOp(enum annoFilterOp op);
/* Translate op into a string.  Do not free result. */

#endif//ndef ANNOFILTER_H
