/* annoFormatter -- aggregates, formats and writes output from multiple sources */

#ifndef ANNOFORMATTER_H
#define ANNOFORMATTER_H

#include "annoRow.h"
#include "annoStreamer.h"
#include "options.h"

// The real work of aggregating and formatting data is left to
// subclass implementations.  The purpose of this module is to provide
// an interface for communication with other components of the
// annoGratorQuery framework, and simple methods shared by all
// subclasses.

struct annoFormatterOption
/* A named and typed option and its value. */
    {
    struct annoFormatterOption *next;
    struct optionSpec spec;
    void *value;
    };

struct annoFormatter
/* Generic interface to aggregate data fields from multiple sources and write
 * output. */
    {
    struct annoFormatter *next;
    struct annoGratorQuery *query;	// The query object that owns this formatter.
    // Public methods
    // Get and set output options
    struct annoFormatterOption *(*getOptions)(struct annoFormatter *self);
    void (*setOptions)(struct annoFormatter *self, struct annoFormatterOption *spec);
    // Collect data from one source
    void (*collect)(struct annoFormatter *self, struct annoStreamer *source, struct annoRow *rows,
		    boolean filterFailed);
    // Aggregate all sources' data for a single primarySource item into output:
    void (*formatOne)(struct annoFormatter *self);
    // End of input; finish output, close connection/handle and free self.
    void (*close)(struct annoFormatter **pSelf);
    };

// ---------------------- annoFormatter default methods -----------------------

#endif//ndef ANNOFORMATTER_H
