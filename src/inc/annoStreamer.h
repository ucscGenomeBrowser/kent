/* annoStreamer -- returns items sorted by genomic position to successive nextRow calls */

#ifndef ANNOSTREAMER_H
#define ANNOSTREAMER_H

#include "annoAssembly.h"
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

    // Public methods
    struct asObject *(*getAutoSqlObject)(struct annoStreamer *self);
    /* Get autoSql representation (do not modify or free asObj!)
     * There is no setter because changing the asObj should never be externally imposed.
     * Subclasses must use annoStreamerSetAutoSqlObject to change their internal asObj. */

    char *(*getName)(struct annoStreamer *self);
    void (*setName)(struct annoStreamer *self, char *name);
    /* Get and set name (short identifier, unique among streamers in a query). */

    void (*setRegion)(struct annoStreamer *self, char *chrom, uint rStart, uint rEnd);
    /* Set genomic region for query; if chrom is NULL, region is whole genome.
     * This must be called on all annoGrator components in query, not a subset. */

    char *(*getHeader)(struct annoStreamer *self);
    /* Get the file header as a string (possibly NULL, possibly multi-line). */

    void (*setFilters)(struct annoStreamer *self, struct annoFilter *newFilters);
    void (*addFilters)(struct annoStreamer *self, struct annoFilter *newFilters);
    /* Set/add filters. Memory management of filters is up to caller. */

    struct annoRow *(*nextRow)(struct annoStreamer *self, char *minChrom, uint minEnd,
			       struct lm *lm);
    /* Get the next item from this source.  If minChrom is non-NULL, optionally use
     * that as a hint to skip items that precede {minChrom, minEnd}.
     * Use localmem lm to store returned annoRow. */

    void (*close)(struct annoStreamer **pSelf);
    /* Close connection to source and free self. */

    // Public members -- callers are on the honor system to access these read-only.
    struct annoAssembly *assembly;	// Genome assembly that provides coords for annotations
    struct asObject *asObj;		// Annotation data definition
    char *name;				// Short identifier, e.g. name of file or database table
    struct annoFilter *filters;		// Filters to constrain output
    char *chrom;			// Non-NULL if querying a particular region
    uint regionStart;			// If chrom is non-NULL, region start coord
    uint regionEnd;			// If chrom is non-NULL, region end coord
    boolean positionIsGenome;		// True if doing a whole-genome query
    enum annoRowType rowType;		// Type of annotations (words or wiggle data)
    int numCols;			// For word-based annotations, number of words/columns
    };

struct annoStreamRows
/* An annoStreamer and (possibly NULL) list of rows it generated. */
    {
    struct annoStreamer *streamer;	// annoStreamer interface for metadata about row data
    struct annoRow *rowList;		// row data
    };

// ---------------------- annoStreamer default methods -----------------------

struct asObject *annoStreamerGetAutoSqlObject(struct annoStreamer *self);
/* Return parsed autoSql definition of this streamer's data type. */

void annoStreamerSetAutoSqlObject(struct annoStreamer *self, struct asObject *asObj);
/* Use new asObj and update internal state derived from asObj. */

char *annoStreamerGetName(struct annoStreamer *self);
/* Returns cloned name of streamer; free when done. */

void annoStreamerSetName(struct annoStreamer *self, char *name);
/* Sets streamer name to clone of name. */

void annoStreamerSetRegion(struct annoStreamer *self, char *chrom, uint rStart, uint rEnd);
/* Set genomic region for query; if chrom is NULL, position is genome.
 * Many subclasses should make their own setRegion method that calls this and
 * configures their data connection to change to the new position. */

void annoStreamerSetFilters(struct annoStreamer *self, struct annoFilter *newFilters);
/* Replace any existing filters with newFilters.  It is up to calling code to
 * free old filters and allocate newFilters. */

void annoStreamerAddFilters(struct annoStreamer *self, struct annoFilter *newFilters);
/* Add newFilter(s).  It is up to calling code to allocate newFilters. */

void annoStreamerInit(struct annoStreamer *self, struct annoAssembly *assembly,
		      struct asObject *asObj, char *name);
/* Initialize a newly allocated annoStreamer with default annoStreamer methods and
 * default filters and columns based on asObj.
 * In general, subclasses' constructors will call this first; override nextRow, close,
 * and probably setRegion; and then initialize their private data. */

void annoStreamerFree(struct annoStreamer **pSelf);
/* Free self. This should be called at the end of subclass close methods, after
 * subclass-specific connections are closed and resources are freed. */

boolean annoStreamerFindBed3Columns(struct annoStreamer *self,
			    int *retChromIx, int *retStartIx, int *retEndIx,
			    char **retChromField, char **retStartField, char **retEndField);
/* Scan autoSql for recognized column names corresponding to BED3 columns.
 * Set ret*Ix to list index of each column if found, or -1 if not found.
 * Set ret*Field to column name if found, or NULL if not found.
 * If all three are found, return TRUE; otherwise return FALSE. */

// -----------------------------------------------------------------------------

struct annoStreamRows *annoStreamRowsNew(struct annoStreamer *streamerList);
/* Returns an array of aSR, one for each streamer in streamerList.
 * Typically array is reused by overwriting elements' rowList pointers.
 * Free array when done. */

#endif//ndef ANNOSTREAMER_H
