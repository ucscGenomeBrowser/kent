/* Sorting and loading of PSL align records into the database.  This module
 * functions as a singlton object, with global state that can be reset.
 * Refer to the doc/database-update-step.html before modifying. */
#ifndef GBALIGNDATA_H
#define GBALIGNDATA_H

#include "common.h"

struct sqlConnection;
struct gbSelect;
struct gbStatusTbl;
struct gbGenome;
struct slName;
struct sqlDeleter;
struct dbLoadOptions;
struct gbGeneTblSet;

void gbAlignDataInit(char *tmpDir, struct dbLoadOptions* options,
                     struct sqlConnection *conn);
/* initialize for outputing PSL files, called once per genbank type.
 * tmpDirPath can be null to setup just for deletion.*/

void gbAlignDataProcess(struct sqlConnection *conn, struct gbSelect* select,
                        struct gbStatusTbl* statusTbl);
/* Parse a psl file looking for accessions to add to the database.  If the
 * version number of an entry matches the accessions status->entry->selectVer
 * field, it will be saved for loading and the count of aligned entries will
 * be incremented. */

void gbAlignDataSetStatus(struct gbStatusTbl* statusTbl);
/* Set the status entries to have the version of the alignments.
 * This is used to set the version on entries that did not align
 * and is called after scanning all of the PSLs.*/

void gbAlignDataDbLoad(struct sqlConnection *conn);
/* load the alignments into the database */

void gbAlignDataDeleteFromTables(char *db, struct sqlConnection *conn,
                                 unsigned srcDb, unsigned type,
                                 struct sqlDeleter* deleter,
                                 struct dbLoadOptions* options);
/* delete alignment data from tables */

void gbAlignDataDeleteOutdated(char *db, struct sqlConnection *conn,
                               struct gbSelect* select, 
                               struct gbStatusTbl* statusTbl,
                               struct dbLoadOptions* options,
                               char *tmpDir);
/* delete outdated alignment data */

void gbAlignRemove(char *db, struct sqlConnection *conn, struct dbLoadOptions* options,
                   struct gbSelect* select, struct sqlDeleter* deleter);
/* Delete all alignments for the selected categories.  Used when reloading
 * alignments.*/

struct slName* gbAlignDataListTables(struct sqlConnection *conn);
/* Get list of alignment tables from database. */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
