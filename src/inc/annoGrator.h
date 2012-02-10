/* annoGrator -- object framework for integrating genomic annotations from disparate sources */

// The real work of fetching data, filtering, intersecting/integrating, and
// producing output in various formats is left to subclass implementations
// of annoStreamer, annoGrator and annoFormatter.  The purpose of this module
// is to provide an interface for communication among those components and
// the UI through which inputs, filters and outputs are configured, as well
// as a top-level routine for executing a query.

#ifndef ANNOGRATOR_H
#define ANNOGRATOR_H

#include "asParse.h"
#include "options.h"

struct annoRow
/* Representation of a row from a database table or file. Purposefully left vague so
 * that different data types can use the most suitable representation, e.g. a struct bed3
 * or a char *row[] or a struct that contains a genePred and a functionPred. */
    {
    struct annoRow *next;
    void *data;
    };

enum annoFilterOp
/* Types of filtering actions; most use a value defined elsewhere in annoFilterSpec. */
{
    afNop,		// Don't do anything (any column value passes filter)
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

struct annoFilterSpec
/* A filter on a column of data: operand and value(s). This info can tell the UI what
 * filtering options to offer and what their current values are; and it tells the
 * annoStreamer what filters to apply. */
{
    struct annoFilterSpec *next;
    struct asColumn *column;	// Lots of info about the column: name, type, type-hints, ...
    char *label;		// Option label for UI (if NULL, see column->comment)
    enum annoFilterOp op;	// Action to be performed
    void *values;		// Depending on op: name(s) to match, thresholds to compare, etc.
    boolean haveMinMax;		// True if we know the allowable range for numeric thresholds
				// so e.g. UI can enforce limits
    union aNumber min;		// Lowest valid threshold value
    union aNumber max;		// Highest valid threshold value
};

struct annoOutputSpec
/* A column and whether it is currently set to be included in output. */
    {
    struct annoOutputSpec *next;
    struct asColumn *column;
    boolean included;
    };

struct annoStreamer
/* Generic interface to configure a data source's filters and output data, and then
 * retrieve data, which must be sorted by genomic position.  Subclasses of this
 * will do all the actual work. */
    {
    struct annoStreamer *next;
    // Public methods
    // Get autoSql representation
    struct asObject *(*getAutoSqlObject)(struct annoStreamer *self);
    // Get and set filters
    struct annoFilterSpec *(*getFilterSpec)(struct annoStreamer *self);
    void (*setFilterSpec)(struct annoStreamer *self, struct annoFilterSpec *spec);
    // Get and set output fields
    struct annoOutputSpec *(*getOutputSpec)(struct annoStreamer *self);
    void (*setOutputSpec)(struct annoStreamer *self, struct annoOutputSpec *spec);
    // Get next item's output fields from this source, indicating
    // whether a filter failed (resulting in a previous item being omitted)
    struct annoRow *(*nextRow)(struct annoStreamer *self, boolean *retFilterFailed);
    // Close connection to source and free self.
    void (*close)(struct annoStreamer **pSelf);
    };

struct annoGrator
/* Generic interface to an annoStreamer and a method to integrate that annoStreamer's
 * data with data from a primary source. */
{
    struct annoGrator *next;
    struct annoStreamer *source;
    // Public methods
    // Get autoSql representation of integrated output
    struct asObject *(*getAutoSqlObject)(struct annoGrator *self, struct annoStreamer *primary);
    // Integrate own source's data with single row of primary source's data
    struct annoRow *(*integrate)(struct annoGrator *self, struct annoStreamer *primary,
				 struct annoRow *primaryRow, boolean *retFilterFailed);
    // Close connection to source and free self.
    void (*close)(struct annoGrator **pSelf);
};

struct annoOutputOption
/* A named and typed option and its value. */
{
    struct annoOutputOption *next;
    struct optionSpec spec;
    void *value;
};

struct annoFormatter
/* Generic interface to aggregate data fields from multiple sources and write
 * output. */
{
    struct annoFormatter *next;
    // Public methods
    // Get and set output options
    struct annoOutputOption *(*getOptions)(struct annoFormatter *self);
    void (*setOptions)(struct annoFormatter *self, struct annoOutputOption *spec);
    // Collect data from one source
    void (*collect)(struct annoFormatter *self, struct annoStreamer *source, struct annoRow *rows,
		    boolean filterFailed);
    // Aggregate all sources' data for a single primarySource item into output:
    void (*formatOne)(struct annoFormatter *self);
    // End of input; finish output, close connection/handle and free self.
    void (*close)(struct annoFormatter **pSelf);
};


struct annoGratorQuery
/* Representation of a complex query: multiple sources, each with its own filters,
 * output data and means of integration, aggregated and output by a formatter. */
    {
    struct annoStreamer *primarySource;
    struct annoGrator *integrators;
    struct annoFormatter *formatters;
    };

void annoGratorQueryExecute(struct annoGratorQuery *query);
/* For each annoRow from query->primarySource, invoke integrators and pass their annoRows
 * to formatters. */

void annoGratorQueryFree(struct annoGratorQuery **pQuery);
/* Close and free all inputs and outputs; free self. */

#endif//ndef ANNOGRATOR_H
