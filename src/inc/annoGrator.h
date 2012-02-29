/* annoGrator -- annoStreamer that integrates genomic annotations from two annoStreamers */

// The real work of intersecting by position and integrating is left
// to subclass implementations.  The purpose of this module is to
// provide an interface for communication with other components of the
// annoGratorQuery framework, and simple methods shared by all
// subclasses.

#ifndef ANNOGRATOR_H
#define ANNOGRATOR_H

#include "annoStreamer.h"

struct annoGrator
/* annoStreamer that can integrate an internal annoStreamer's data
 * with data from a primary source. */
    {
    struct annoStreamer streamer;
    // Public method that makes this a 'grator:
    // Integrate own source's data with single row of primary source's data
    struct annoRow *(*integrate)(struct annoGrator *self, struct annoStreamer *primary,
				 struct annoRow *primaryRow, boolean *retFilterFailed);
    };

#endif//ndef ANNOGRATOR_H
