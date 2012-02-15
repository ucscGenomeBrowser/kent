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

struct annoStreamer
/* Generic interface to configure a data source's filters and output data, and then
 * retrieve data, which must be sorted by genomic position.  Subclasses of this
 * will do all the actual work. */
    {
    struct annoStreamer *next;
    struct annoGratorQuery *query;	// The query object that owns this streamer.
    // Public methods
    // Get autoSql representation
    struct asObject *(*getAutoSqlObject)(struct annoStreamer *self);
    // Set genomic region for query (should be called only by annoGratorQuerySetRegion)
    void (*setRegion)(struct annoStreamer *self, char *chrom, uint rStart, uint rEnd);
    // Get and set filters
    struct annoFilter *(*getFilters)(struct annoStreamer *self);
    void (*setFilters)(struct annoStreamer *self, struct annoFilter *newFilters);
    // Get and set output fields
    struct annoColumn *(*getColumns)(struct annoStreamer *self);
    void (*setColumns)(struct annoStreamer *self, struct annoColumn *newColumns);
    // Get next item's output fields from this source, indicating
    // whether a filter failed (resulting in a previous item being omitted)
    struct annoRow *(*nextRow)(struct annoStreamer *self, boolean *retFilterFailed);
    // Close connection to source and free self.
    void (*close)(struct annoStreamer **pSelf);
    // Private members -- you're on the honor system to access these using only the methods above.
    boolean positionIsGenome;
    char *chrom;
    uint regionStart;
    uint regionEnd;
    struct asObject *asObj;
    struct annoFilter *filters;
    struct annoColumn *columns;
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
/* Free old filters and use newFilters. */

struct annoColumn *annoStreamerGetColumns(struct annoStreamer *self);
/* Return supported columns with current settings.  Callers can modify and free when done. */

void annoStreamerSetColumns(struct annoStreamer *self, struct annoColumn *columns);
/* Free old columns and use newColumns. */

void annoStreamerInit(struct annoStreamer *self, struct asObject *asObj);
/* Initialize a newly allocated annoStreamer with default annoStreamer methods and
 * default filters and columns based on asObj.
 * In general, subclasses' constructors will call this first; override nextRow, close,
 * and probably setRegion; and then initialize their private data. */

void annoStreamerFree(struct annoStreamer **pSelf);
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */

#endif//ndef ANNOSTREAMER_H
