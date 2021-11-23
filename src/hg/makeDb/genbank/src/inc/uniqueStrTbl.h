/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#ifndef UNIQUESTRTBL_H
#define UNIQUESTRTBL_H
#include "hgRelate.h"
struct sqlConnection;

struct uniqueStrTbl
/* Object to manage a SQL and memory table of unique strings with
 * corresponding numeric ids. */
{
    struct uniqueStrTbl *next;
    struct hash *hash;  /* hash of strings to ids.  Id is case into value
                         * pointer */
    HGID nextId;        /* next id to allocated */
    struct sqlUpdater *updater; /* object to manage updates */
};

struct uniqueStrTbl *uniqueStrTblNew(struct sqlConnection *conn,
                                     char *table, int hashPow2Size,
                                     boolean prefetch, char *tmpDir,
                                     boolean verbose);
/* Create an object to accessed the specified unique string table, creating
 * table if it doesn't exist.  Optionally prefetch the table into memory. */

HGID uniqueStrTblFind(struct uniqueStrTbl *ust, struct sqlConnection* conn,
                      char* str, char** strVal);
/* Lookup a string in the table, returning the id.  If strVal is not NULL,
 * a pointer to the string in the table is returned.  If not found, return
 * 0 and NULL in strVal */

HGID uniqueStrTblGet(struct uniqueStrTbl *ust, struct sqlConnection *conn,
                     char* str, char** strVal);
/* Lookup a string in the table.  If it doesn't exists, add it.  If strVal is
 * not NULL, a pointer to the string in the table is returned.  */

void uniqueStrTblCommit(struct uniqueStrTbl* ust, struct sqlConnection* conn);
/* commit pending table changes */

void uniqueStrTblFree(struct uniqueStrTbl **ust);
/* Free a uniqueStrTbl object */
#endif
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
