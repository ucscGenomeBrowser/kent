/* Delete ignored entries from the database.  Also handled deleting of
 * accessions to explictly reloadd */
#ifndef GBIGNOREDELETE_H
#define GBIGNOREDELETE_H

void gbIgnoredDelete(struct gbSelect* selectList, boolean force, char* workDir);
/* Deleted any sequences in the ignore table from the database that are in the
 * gbStatus table for all selected releases. force will delete even if not in
 * gbStatus. */

void gbReloadDelete(char *reloadList, char* workDir);
/* delete sequences that have been explictly requested for reloading */

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

