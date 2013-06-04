/* annoGrator -- annoStreamer that integrates genomic annotations from two annoStreamers */

// Subclasses of annoGrator can add new columns of output such as predicted function given
// a variant and a gene; the base class simply intersects by position, returning
// all rows from its internal data source that overlap the position of primaryRow.
// The interface to an annoGrator is almost the same as the interface to an annoStreamer,
// *except* you call integrate() instead of nextRow().

#ifndef ANNOGRATOR_H
#define ANNOGRATOR_H

#include "annoStreamer.h"

enum annoGratorOverlap
/* How integrate() method handles overlap (or non-overlap) of internal rows with primary row */
    {
    agoNoConstraint,	// Default: overlap with primary row doesn't matter
    agoMustOverlap,	// integrate() sets RJFilterFail if no internal rows overlap primary
    agoMustNotOverlap   // integrate() sets RJFilterFail if any internal rows overlap primary
    };

struct annoGrator
/* annoStreamer that can integrate an internal annoStreamer's data
 * with data from a primary source. */
    {
    struct annoStreamer streamer;	// external annoStreamer interface

    // Public method that makes this a 'grator:
    struct annoRow *(*integrate)(struct annoGrator *self, struct annoStreamRows *primaryData,
				 boolean *retRJFilterFailed, struct lm *callerLm);
    /* Integrate internal source's data with single row of primary source's data */

    void (*setOverlapRule)(struct annoGrator *self, enum annoGratorOverlap rule);
    /* Tell annoGrator how to handle overlap of its rows with primary row. */

    // Private members -- callers are on the honor system to access these using only methods above.
    struct annoStreamer *mySource;	// internal source
    struct annoRow *qHead;		// head of FIFO queue of rows from internal source
    struct annoRow *qTail;		// head of FIFO queue of rows from internal source
    struct lm *qLm;			// localmem for FIFO queue
    int qSkippedCount;			// Number of qLm-allocated rows skipped
    char *prevPChrom;			// for detection of unsorted input from primary
    uint prevPStart;			// for detection of unsorted input from primary
    boolean eof;			// stop asking internal source for rows when it's done
    boolean haveRJIncludeFilter;	// TRUE if some filter has !isExclude && rightJoin
    enum annoGratorOverlap overlapRule;	// constraint (if any) on overlap of internal & primary
    };

#endif//ndef ANNOGRATOR_H

// ---------------------- annoGrator default methods -----------------------

struct annoRow *annoGratorIntegrate(struct annoGrator *self, struct annoStreamRows *primaryData,
				    boolean *retRJFilterFailed, struct lm *callerLm);
/* Given a single row from the primary source, get all overlapping rows from internal
 * source, and produce joined output rows.  Use callerLm to allocate the output rows.
 * If retRJFilterFailed is non-NULL:
 * - any overlapping row has a rightJoin filter failure (see annoFilter.h), or
 * - overlap rule is agoMustOverlap and no rows overlap, or
 * - overlap rule is agoMustNotOverlap and any overlapping row is found,
 * then set retRJFilterFailed and stop. */

void annoGratorInit(struct annoGrator *self, struct annoStreamer *mySource);
/* Initialize an integrator of columns from mySource with (positions of)
 * rows passed to integrate().
 * mySource becomes property of the annoGrator. */

struct annoGrator *annoGratorNew(struct annoStreamer *mySource);
/* Make a new integrator of columns from mySource with (positions of) rows passed to integrate().
 * mySource becomes property of the new annoGrator. */

void annoGratorClose(struct annoStreamer **pSelf);
/* Free self (including mySource). */
