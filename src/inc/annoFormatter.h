/* annoFormatter -- aggregates, formats and writes output from multiple sources */

#ifndef ANNOFORMATTER_H
#define ANNOFORMATTER_H

#include "annoOption.h"
#include "annoStreamer.h"

// The real work of aggregating and formatting data is left to
// subclass implementations.  The purpose of this module is to provide
// an interface for communication with other components of the
// annoGratorQuery framework, and simple methods shared by all
// subclasses.

struct annoFormatter
/* Generic interface to aggregate data fields from multiple sources and write
 * output. */
    {
    struct annoFormatter *next;

    // Public methods
    struct annoOption *(*getOptions)(struct annoFormatter *self);
    void (*setOptions)(struct annoFormatter *self, struct annoOption *options);
    /* Get and set output options */

    void (*initialize)(struct annoFormatter *self, struct annoStreamer *primarySource,
		       struct annoStreamer *integrators);
    /* Initialize output (print header if applicable, etc). */

    void (*comment)(struct annoFormatter *self, char *content);
    /* Print a comment in whatever form is appropriate.  Content must not contain '\n'. */

    void (*formatOne)(struct annoFormatter *self, struct annoStreamRows *primaryData,
		      struct annoStreamRows gratorData[], int gratorCount);
    /* Aggregate all sources' data for a single primary-source item into output. */

    void (*close)(struct annoFormatter **pSelf);
    /* End of input; finish output, close connection/handle and free self. */

    // Private members -- callers are on the honor system to access these using only methods above.
    struct annoOption *options;
    struct annoGratorQuery *query;
    };

// ---------------------- annoFormatter default methods -----------------------

struct annoOption *annoFormatterGetOptions(struct annoFormatter *self);
/* Return supported options and current settings.  Callers can modify and free when done. */

void annoFormatterSetOptions(struct annoFormatter *self, struct annoOption *newOptions);
/* Free old options and use clone of newOptions. */

void annoFormatterFree(struct annoFormatter **pSelf);
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */

#endif//ndef ANNOFORMATTER_H
