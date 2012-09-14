/* annoGrator -- annoStreamer that integrates genomic annotations from two annoStreamers */

// Subclasses of annoGrator can add new columns of output such as predicted function given
// a variant and a gene; the base class simply intersects by position, returning
// all rows from its internal data source that overlap the position of primaryRow.
// The interface to an annoGrator is almost the same as the interface to an annoStreamer,
// *except* you call integrate() instead of nextRow().

#ifndef ANNOGRATOR_H
#define ANNOGRATOR_H

#include "annoStreamer.h"

struct annoGrator
/* annoStreamer that can integrate an internal annoStreamer's data
 * with data from a primary source. */
    {
    struct annoStreamer streamer;	// external annoStreamer interface

    // Public method that makes this a 'grator:
    struct annoRow *(*integrate)(struct annoGrator *self, struct annoRow *primaryRow,
				 boolean *retRJFilterFailed);
    /* Integrate internal source's data with single row of primary source's data */

    // Private members -- callers are on the honor system to access these using only methods above.
    struct annoStreamer *mySource;	// internal source
    struct annoRow *qHead;		// head of FIFO queue of rows from internal source
    struct annoRow *qTail;		// head of FIFO queue of rows from internal source
    char *prevPChrom;			// for detection of unsorted input from primary
    uint prevPStart;			// for detection of unsorted input from primary
    boolean eof;			// stop asking internal source for rows when it's done
    boolean haveRJIncludeFilter;	// TRUE if some filter has !isExclude && rightJoin;
					// if TRUE and there are no overlapping rows, then RJ fail
    };

#endif//ndef ANNOGRATOR_H

// ---------------------- annoGrator default methods -----------------------

struct annoRow *annoGratorIntegrate(struct annoGrator *self, struct annoRow *primaryRow,
				    boolean *retRJFilterFailed);
/* Given a single row from the primary source, get all overlapping rows from internal
 * source, and produce joined output rows.  If retRJFilterFailed is non-NULL and any
 * overlapping row has a rightJoin filter failure (see annoFilter.h),
 * set retRJFilterFailed and stop. */

struct annoGrator *annoGratorNew(struct annoStreamer *mySource);
/* Make a new integrator of columns from mySource with (positions of) rows passed to integrate().
 * mySource becomes property of the new annoGrator. */

void annoGratorSetQuery(struct annoStreamer *vSelf, struct annoGratorQuery *query);
/* Set query (to be called only by annoGratorQuery which is created after streamers). */

void annoGratorClose(struct annoStreamer **pSelf);
/* Free self (including mySource). */
