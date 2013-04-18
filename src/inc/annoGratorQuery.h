/* annoGratorQuery -- framework for integrating genomic annotations from many sources */

#ifndef ANNOGRATORQUERY_H
#define ANNOGRATORQUERY_H

#include "annoFormatter.h"
#include "annoGrator.h"

struct annoGratorQuery;
/* Representation of a complex query: multiple sources, each with its own filters,
 * output data and means of integration, aggregated and output by a formatter. */

struct annoGratorQuery *annoGratorQueryNew(struct annoAssembly *assembly,
					   struct annoStreamer *primarySource,
					   struct annoGrator *integrators,
					   struct annoFormatter *formatters);
/* Create an annoGratorQuery from all of its components.
 * integrators may be NULL.  All other inputs must be non-NULL. */

void annoGratorQuerySetRegion(struct annoGratorQuery *query, char *chrom, uint rStart, uint rEnd);
/* Set genomic region for query; if chrom is NULL, position is whole genome. */

void annoGratorQueryExecute(struct annoGratorQuery *query);
/* For each annoRow from query->primarySource, invoke integrators and
 * pass their annoRows to formatters. */

void annoGratorQueryFree(struct annoGratorQuery **pQuery);
/* Close and free all inputs and outputs; free self. */

#endif//ndef ANNOGRATORQUERY_H
