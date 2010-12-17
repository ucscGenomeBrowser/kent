/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* hgRelate - Especially relational parts of browser data base. */
#ifndef HGRELATE_H
#define HGRELATE_H

#ifndef DNASEQ_H
#include "dnaseq.h"
#endif 

#ifndef UNFIN_H
#include "unfin.h"
#endif

#ifndef JKSQL_H
#include "jksql.h"
#endif

typedef unsigned int HGID;	/* A database ID. */

void hgHistoryComment(struct sqlConnection *conn, char *comment, ...)
/* Add comment to history table. */
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
;

void hgHistoryCommentWithIds(struct sqlConnection *conn, int startId, int endId, char *comment, ...)
/* Add comment to history table, with id range. */
#if defined(__GNUC__)
__attribute__((format(printf, 4, 5)))
#endif
;

struct sqlConnection *hgStartUpdate(char *db);
/* Open and connection and lock the history table */

void hgEndUpdate(struct sqlConnection **pConn, int startId, int endId, char *comment, ...)
/* Finish up connection with a printf format comment and optional id range */
#if defined(__GNUC__)
__attribute__((format(printf, 4, 5)))
#endif
;

FILE *hgCreateTabFile(char *tmpDir, char *tableName);
/* Open a tab file with name corresponding to tableName in tmpDir.  If tmpDir is NULL,
 * use TMPDIR environment, or "/var/tmp" */

int hgUnlinkTabFile(char *tmpDir, char *tableName);
/* Unlink tab file.   If tmpDir is NULL, use TMPDIR environment, or "/var/tmp" */

void hgLoadTabFile(struct sqlConnection *conn, char *tmpDir, char *tableName,
                   FILE **tabFh);
/* Load tab delimited file corresponding to tableName. close fh if not NULL.
 * If tmpDir is NULL, use TMPDIR environment, or "/var/tmp"*/

void hgLoadNamedTabFile(struct sqlConnection *conn, char *tmpDir, char *tableName,
                        char *fileName, FILE **tabFh);
/* Load named tab delimited file corresponding to tableName. close fh if not
 * NULL If tmpDir is NULL, use TMPDIR environment, or "/var/tmp"*/

void hgLoadTabFileOpts(struct sqlConnection *conn, char *tmpDir, char *tableName,
                       unsigned options, FILE **tabFh);
/* Load tab delimited file corresponding to tableName. close tabFh if not NULL
 * If tmpDir is NULL, use TMPDIR environment, or "/var/tmp". Options are those
 * supported by sqlLoadTabFile */

void hgRemoveTabFile(char *tmpDir, char *tableName);
/* Remove file.* If tmpDir is NULL, use TMPDIR environment, or "/var/tmp" */

HGID hgGetMaxId(struct sqlConnection *conn, char *tableName);
/* get the maximum value of the id column in a table or zero if empry  */

int hgAddToExtFileTbl(char *path, struct sqlConnection *conn, char *extFileTbl);
/* Add entry to the specified extFile table.  Delete it if it already exists.
 * Returns extFile id. */

int hgAddToExtFile(char *path, struct sqlConnection *conn);
/* Add entry to ext file table.  Delete it if it already exists. 
 * Returns extFile id. */

void hgPurgeExtFileTbl(int id, struct sqlConnection *conn, char *extFileTbl);
/* remove an entry from the extFile table.  Called
 * when there is an error loading the referenced file
 */

void hgPurgeExtFile(int id, struct sqlConnection *conn);
/* remove an entry from the extFile table.  Called
 * when there is an error loading the referenced file
 */
#endif /* HGRELATE_H */

