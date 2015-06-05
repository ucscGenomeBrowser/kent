/* hAnno -- helpers for creating anno{Streamers,Grators,Formatters,Queries} */

#ifndef HANNO_H
#define HANNO_H

#include "annoGrator.h"
#include "jksql.h"
#include "trackDb.h"

// Represent "unlimited" as limit==0.
#define ANNO_NO_LIMIT 0


struct annoAssembly *hAnnoGetAssembly(char *db);
/* Make annoAssembly for db. */

struct annoStreamer *hAnnoStreamerFromTrackDb(struct annoAssembly *assembly, char *selTable,
                                              struct trackDb *tdb, char *chrom, int maxOutRows);
/* Figure out the source and type of data and make an annoStreamer. */

struct annoGrator *hAnnoGratorFromBigFileUrl(char *fileOrUrl, struct annoAssembly *assembly,
                                             int maxOutRows, enum annoGratorOverlap overlapRule);
/* Determine what kind of big data file/url we have, make an annoStreamer & in annoGrator. */

struct annoGrator *hAnnoGratorFromTrackDb(struct annoAssembly *assembly, char *selTable,
                                          struct trackDb *tdb, char *chrom, int maxOutRows,
                                          struct asObject *primaryAsObj,
                                          enum annoGratorOverlap overlapRule);
/* Figure out the source and type of data, make an annoStreamer & wrap in annoGrator.
 * If not NULL, primaryAsObj is used to determine whether we can make an annoGratorGpVar. */

struct asObject *hAnnoGetAutoSqlForTdb(char *db, char *chrom, struct trackDb *tdb);
/* If possible, return the asObj that a streamer for this track would use, otherwise NULL. */

#endif // HANNO_H
