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
    // Public methods
    // Get and set output options
    struct annoFormatterOption *(*getOptions)(struct annoFormatter *self);
    void (*setOptions)(struct annoFormatter *self, struct annoFormatterOption *options);
    // Collect data from one source
    void (*collect)(struct annoFormatter *self, struct annoStreamer *source, struct annoRow *rows,
		    boolean filterFailed);
    // Aggregate all sources' data for a single primarySource item into output:
    void (*formatOne)(struct annoFormatter *self);
    // End of input; finish output, close connection/handle and free self.
    void (*close)(struct annoFormatter **pSelf);
    // Private members -- callers are on the honor system to access these using only methods above.
    struct annoFormatterOption *options;
    struct annoGratorQuery *query;
    };

// ---------------------- annoFormatter default methods -----------------------

struct annoFormatterOption *annoFormatterGetOptions(struct annoFormatter *self);
/* Return supported options and current settings.  Callers can modify and free when done. */

void annoFormatterSetOptions(struct annoFormatter *self, struct annoFormatterOption *newOptions);
/* Free old options and use newOptions. */

void annoFormatterFree(struct annoFormatter **pSelf);
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */

#endif//ndef ANNOFORMATTER_H
