/* annoGratorQuery -- framework for integrating genomic annotations from many sources */

#ifndef ANNOGRATORQUERY_H
#define ANNOGRATORQUERY_H

#include "annoFormatter.h"
#include "annoGrator.h"
#include "hash.h"
#include "twoBit.h"

struct annoGratorQuery
/* Representation of a complex query: multiple sources, each with its own filters,
 * output data and means of integration, aggregated and output by a formatter. */
    {
    // Private members -- callers are on the honor system to leave these alone. Use functions below.
    char *assemblyName;			// Usually a UCSC db name like "hg19".
    struct hash *chromSizes;		// Assembly seq sizes, must be valid for all inputs
    struct twoBitFile *tbf;		// Assembly sequence, must be valid for all inputs
    struct annoStreamer *primarySource;	// Annotations to be integrated with other annos.
    struct annoGrator *integrators;	// Annotations & methods for integrating w/primary
    struct annoFormatter *formatters;	// Writers of output collected from primary & intg's
    boolean csAllocdHere;		// internal use only.
    };

struct annoGratorQuery *annoGratorQueryNew(char *assemblyName, struct hash *chromSizes,
					   struct twoBitFile *tbf,
					   struct annoStreamer *primarySource,
					   struct annoGrator *integrators,
					   struct annoFormatter *formatters);
/* Create an annoGratorQuery from all of its components, and introduce components to each other.
 * Either chromSizes or tbf may be NULL.  integrators may be NULL.
 * All other inputs must be non-NULL. */

void annoGratorQuerySetRegion(struct annoGratorQuery *query, char *chrom, uint rStart, uint rEnd);
/* Set genomic region for query; if chrom is NULL, position is whole genome. */

void annoGratorQueryExecute(struct annoGratorQuery *query);
/* For each annoRow from query->primarySource, invoke integrators and
 * pass their annoRows to formatters. */

void annoGratorQueryFree(struct annoGratorQuery **pQuery);
/* Close and free all inputs and outputs; free self. */

#endif//ndef ANNOGRATORQUERY_H
