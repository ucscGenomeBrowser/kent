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
    struct annoStreamer streamer;	// external annoStreamer interface
    // Public method that makes this a 'grator:
    // Integrate own source's data with single row of primary source's data
    struct annoRow *(*integrate)(struct annoGrator *self, struct annoRow *primaryRow,
				 boolean *retFilterFailed);
    // Private members -- callers are on the honor system to access these using only methods above.
    struct asObject *primaryAsObj;	// autoSql description of primary source -- read only!
    struct annoStreamer *mySource;	// internal source
    char *prevPChrom;			// for detection of unsorted input from primary
    uint prevPStart;			// for detection of unsorted input from primary
    boolean eof;			// stop asking internal source for rows when it's done
    struct annoRow *qHead;		// head of FIFO queue of rows from internal source
    struct annoRow *qTail;		// head of FIFO queue of rows from internal source
    };

#endif//ndef ANNOGRATOR_H

// ---------------------- annoGrator default methods -----------------------

struct annoRow *annoGratorIntegrate(struct annoGrator *self, struct annoRow *primaryRow,
				    boolean *retRJFilterFailed);
/* Given a single row from the primary source, get all overlapping rows from internal
 * source, and produce joined output rows.  If retRJFilterFailed is non-NULL and any 
 * overlapping row has a rightJoin filter failure (see annoFilter.h),
 * set retRJFilterFailed and stop. */

struct annoGrator *annoGratorGenericNew(struct annoStreamer *primarySource,
					struct annoStreamer *mySource);
/* Make a new integrator of columns from two annoStreamer sources.
 * mySource becomes property of the new annoGrator. */

void annoGratorSetQuery(struct annoStreamer *vSelf, struct annoGratorQuery *query);
/* Set query (to be called only by annoGratorQuery which is created after streamers). */

void annoGratorGenericClose(struct annoStreamer **pSelf);
/* Free self (including mySource). */
