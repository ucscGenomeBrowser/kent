/* annoStreamDb -- subclass of annoStreamer for database tables */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef ANNOSTREAMDB_H
#define ANNOSTREAMDB_H

#include "annoStreamer.h"
#include "joiner.h"
#include "jsonParse.h"

struct annoStreamer *annoStreamDbNew(char *db, char *table, struct annoAssembly *aa,
				     int maxOutRows, struct jsonElement *config);
/* Create an annoStreamer (subclass) object from a database table.
 * If config is NULL, then the streamer produces output from all fields
 * (except bin, unless table's autoSql includes bin).
 * Otherwise, config is a json object with a member 'relatedTables' that specifies
 * related tables and fields to join with table, for example:
 * config = { "relatedTables": [ { "table": "hg19.kgXref",
 *                                 "fields": ["geneSymbol", "description"] },
 *                               { "table": "hg19.knownCanonical",
 *                                 "fields": ["clusterId"] }
 *                             ] }
 * -- the streamer's autoSql will be constructed by appending autoSql column
 * descriptions to the columns of table.
 * Caller may free db, table, and dbTableFieldList when done with them, but must keep the
 * annoAssembly aa alive for the lifetime of the returned annoStreamer. */

char *annoStreamDbColumnNameFromDtf(char *db, char *mainTable, struct joinerDtf *dtf);
/* Return a string with the autoSql column name that would be assigned according to dtf's
 * db, table and field. */

#endif//ndef ANNOSTREAMDB_H
