/* annoGrator -- integrates genomic annotations from two annoStreamers */

// The real work of intersecting by position and integrating is left
// to subclass implementations.  The purpose of this module is to
// provide an interface for communication with other components of the
// annoGratorQuery framework, and simple methods shared by all
// subclasses.

#ifndef ANNOGRATOR_H
#define ANNOGRATOR_H

#include "annoStreamer.h"

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

#endif//ndef ANNOGRATOR_H
