/* annoStreamer -- returns items sorted by genomic position to successive nextRow calls */

#ifndef ANNOSTREAMER_H
#define ANNOSTREAMER_H

#include "annoColumn.h"
#include "annoFilter.h"
#include "annoRow.h"

// The real work of fetching and filtering data is left to subclass
// implementations.  The purpose of this module is to provide an
// interface for communication with other components of the
// annoGratorQuery framework, and simple methods shared by all
// subclasses.

// stub in order to avoid problems with circular .h references:
struct annoGratorQuery;

struct annoStreamer
/* Generic interface to configure a data source's filters and output data, and then
 * retrieve data, which must be sorted by genomic position.  Subclasses of this
 * will do all the actual work. */
    {
    struct annoStreamer *next;

    // Public methods
    struct asObject *(*getAutoSqlObject)(struct annoStreamer *self);
    /* Get autoSql representation (do not modify or free!) */

    void (*setRegion)(struct annoStreamer *self, char *chrom, uint rStart, uint rEnd);
    /* Set genomic region for query (should be called only by annoGratorQuerySetRegion) */

    char *(*getHeader)(struct annoStreamer *self);
    /* Get the file header as a string (possibly NULL, possibly multi-line). */

    struct annoFilter *(*getFilters)(struct annoStreamer *self);
    void (*setFilters)(struct annoStreamer *self, struct annoFilter *newFilters);
    /* Get and set filters */

    struct annoColumn *(*getColumns)(struct annoStreamer *self);
    void (*setColumns)(struct annoStreamer *self, struct annoColumn *newColumns);
    /* Get and set output fields */

    struct annoRow *(*nextRow)(struct annoStreamer *self);
    /* Get next item's output fields from this source */

    void (*close)(struct annoStreamer **pSelf);
    /* Close connection to source and free self. */

    void (*setQuery)(struct annoStreamer *self, struct annoGratorQuery *query);
    /* For use by annoGratorQuery only: hook up query object after creation */

    // Public members -- callers are on the honor system to access these read-only.
    struct annoGratorQuery *query;	// The query object that owns this streamer.
    enum annoRowType rowType;
    int numCols;
    struct annoFilter *filters;
    struct annoColumn *columns;

    // Private members -- callers are on the honor system to access these using only methods above.
    boolean positionIsGenome;
    char *chrom;
    uint regionStart;
    uint regionEnd;
    struct asObject *asObj;
    };

// ---------------------- annoStreamer default methods -----------------------

struct asObject *annoStreamerGetAutoSqlObject(struct annoStreamer *self);
/* Return parsed autoSql definition of this streamer's data type. */

void annoStreamerSetRegion(struct annoStreamer *self, char *chrom, uint rStart, uint rEnd);
/* Set genomic region for query; if chrom is NULL, position is genome.
 * Many subclasses should make their own setRegion method that calls this and
 * configures their data connection to change to the new position. */

struct annoFilter *annoStreamerGetFilters(struct annoStreamer *self);
/* Return supported filters with current settings.  Callers can modify and free when done. */

void annoStreamerSetFilters(struct annoStreamer *self, struct annoFilter *newFilters);
/* Free old filters and use clone of newFilters. */

struct annoColumn *annoStreamerGetColumns(struct annoStreamer *self);
/* Return supported columns with current settings.  Callers can modify and free when done. */

void annoStreamerSetColumns(struct annoStreamer *self, struct annoColumn *columns);
/* Free old columns and use clone of newColumns. */

void annoStreamerInit(struct annoStreamer *self, struct asObject *asObj);
/* Initialize a newly allocated annoStreamer with default annoStreamer methods and
 * default filters and columns based on asObj.
 * In general, subclasses' constructors will call this first; override nextRow, close,
 * and probably setRegion and setQuery; and then initialize their private data. */

void annoStreamerFree(struct annoStreamer **pSelf);
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */

void annoStreamerSetQuery(struct annoStreamer *self, struct annoGratorQuery *query);
/* Set query (to be called only by annoGratorQuery which is created after streamers). */

#endif//ndef ANNOSTREAMER_H
