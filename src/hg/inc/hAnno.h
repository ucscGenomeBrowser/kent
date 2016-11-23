/* hAnno -- helpers for creating anno{Streamers,Grators,Formatters,Queries} */

#ifndef HANNO_H
#define HANNO_H

#include "annoGrator.h"
#include "jksql.h"
#include "jsonParse.h"
#include "trackDb.h"

// Represent "unlimited" as limit==0.
#define ANNO_NO_LIMIT 0


struct annoAssembly *hAnnoGetAssembly(char *db);
/* Make annoAssembly for db. */

struct annoStreamer *hAnnoStreamerFromTrackDb(struct annoAssembly *assembly, char *selTable,
                                              struct trackDb *tdb, char *chrom, int maxOutRows,
                                              struct jsonElement *config);
/* Figure out the source and type of data and make an annoStreamer. */

struct annoStreamer *hAnnoStreamerFromBigFileUrl(char *fileOrUrl, char *indexUrl, struct annoAssembly *assembly,
                                                 int maxOutRows, char *type);
/* Determine what kind of big data file/url we have and make streamer for it.
 * If type is NULL, this will determine type using custom track type or file suffix. 
 * indexUrl can be NULL, unless the type is VCF and the .tbi file is not alongside the .VCF */

struct annoGrator *hAnnoGratorFromBigFileUrl(char *fileOrUrl, char *indexUrl, struct annoAssembly *assembly,
                                             int maxOutRows, enum annoGratorOverlap overlapRule);
/* Determine what kind of big data file/url we have, make an annoStreamer & in annoGrator. */
/* indexUrl can be NULL, unless the type is VCF and the .tbi file is not alongside the .VCF */

struct annoGrator *hAnnoGratorFromTrackDb(struct annoAssembly *assembly, char *selTable,
                                          struct trackDb *tdb, char *chrom, int maxOutRows,
                                          struct asObject *primaryAsObj,
                                          enum annoGratorOverlap overlapRule,
                                          struct jsonElement *config);
/* Figure out the source and type of data, make an annoStreamer & wrap in annoGrator.
 * If not NULL, primaryAsObj is used to determine whether we can make an annoGratorGpVar. */

struct asObject *hAnnoGetAutoSqlForTdb(char *db, char *chrom, struct trackDb *tdb);
/* If possible, return the asObj that a streamer for this track would use, otherwise NULL. */

struct asObject *hAnnoGetAutoSqlForDbTable(char *db, char *table, struct trackDb *tdb,
                                           boolean skipBin);
/* Get autoSql for db.dbTable from tdb and/or db.tableDescriptions;
 * if it doesn't match columns, make one up from db.table sql fields.
 * Some subtleties are lost in translation from .as to .sql, that's why
 * we try tdb & db.tableDescriptions first.  But ultimately we need to return
 * an asObj whose columns match all fields of the table. */

#endif // HANNO_H
