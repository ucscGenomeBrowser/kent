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

void gbAlignDataInit(char *tmpDirPath, boolean noPerChrom);
/* initialize for outputing PSL files, called once per genbank type */

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

void gbAlignDataDeleteFromTables(struct sqlConnection *conn,
                                 unsigned srcDb, unsigned type,
                                 struct sqlDeleter* deleter);
/* delete alignment data from tables */

void gbAlignDataDeleteOutdated(struct sqlConnection *conn,
                               struct gbSelect* select, 
                               struct gbStatusTbl* statusTbl,
                               char *tmpDirPath);
/* delete outdated alignment data */

void gbAlignDataDrop(struct sqlConnection *conn);
/* Drop alignment tables from database. */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
