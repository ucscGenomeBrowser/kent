/* Delete ignored entries from the database.  Also handled deleting of
 * accessions to explictly reloadd */

/* Copyright (C) 2008 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#ifndef GBIGNOREDELETE_H
#define GBIGNOREDELETE_H
struct dbLoadOptions;

void gbIgnoredDelete(char *db, struct gbSelect* selectList, boolean force,
                     struct dbLoadOptions* options, char* workDir);
/* Deleted any sequences in the ignore table from the database that are in the
 * gbStatus table for all selected releases. force will delete even if not in
 * gbStatus. */

void gbReloadDelete(char *db, char *reloadList, struct dbLoadOptions* options, char* workDir);
/* delete sequences that have been explictly requested for reloading */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

