/* Parsing and output of mrna metadata in ra file into seq, mrna and
 * associated unique string tables.  This module functions as a singlton
 * object, with global state that can be reset.  Refer to the
 * doc/database-update-step.html before modifying. */
#ifndef GBMETADATA_H
#define GBMETADATA_H

#include "common.h"
#include "hgRelate.h"
struct sqlConnection;
struct lineFile;
struct gbProcessed;
struct gbStatusTbl;
struct gbSelect;
struct sqlDeleter;
struct dbLoadOptions;

void gbMetaDataInit(struct sqlConnection *conn, unsigned srcDb,
                    struct dbLoadOptions* options, char *gbdbGenBank, 
                    char *tmpDir);
/* initialize for parsing metadata */

void gbMetaDataProcess(struct sqlConnection *conn,
                       struct gbStatusTbl* statusTbl,
                       struct gbSelect* select);
/* process metadata for an update */

void gbMetaDataUpdateChgGenes(struct sqlConnection *conn,
                              struct gbSelect *select,
                              struct gbStatusTbl* statusTbl,
                              char *tmpDir);
/* update gene tables where annotations have changed but sequence
 * has not changes and is not being reloaded */

void gbMetaDataDbLoad(struct sqlConnection *conn);
/* load the metadata changes into the database */

void gbMetaDataDeleteFromIdTables(struct sqlConnection *conn, struct dbLoadOptions* options,
                                  struct sqlDeleter* deleter);
/* delete sequence from metadata tables with ids.  These are always
 * deleted and rebuilt even for modification */

void gbMetaDataDeleteFromTables(struct sqlConnection *conn, struct dbLoadOptions* options,
                                unsigned srcDb, struct sqlDeleter* deleter);
/* delete sequence from metadata tables */

void gbMetaDataRemove(struct sqlConnection *conn,
                      struct dbLoadOptions* options,
                      struct gbSelect* select,
                      struct sqlDeleter* deleter);
/* remove metaData from all entries in the select categories.
 * Used when reloading. */

void gbMetaDataDeleteOutdated(struct sqlConnection *conn,
                              struct gbSelect* select,
                              struct gbStatusTbl* statusTbl,
                              struct dbLoadOptions* options,
                              char *tmpDir);
/* delete outdated metadata */

void gbMetaDataFree();
/* Free data structures */

struct slName* gbMetaDataListTables(struct sqlConnection *conn);
/* Get a list of metadata tables that exist in the database */

#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
