/* hdb - human genome browser database. */

#ifndef HDB_H
#define HDB_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

void hSetDb(char *dbName);
/* Set the database name. */

char *hGetDb();
/* Return the current database name. */

struct sqlConnection *hAllocConn();
/* Get free connection if possible. If not allocate a new one. */

struct sqlConnection *hFreeConn(struct sqlConnection **pConn);
/* Put back connection for reuse. */

boolean hTableExists(char *table);
/* Return TRUE if a table exists in database. */

int hChromSize(char *chromName);
/* Return size of chromosome. */

struct dnaSeq *hDnaFromSeq(char *seqName, int start, int end, enum dnaCase dnaCase);
/* Fetch DNA */

struct dnaSeq *hLoadChrom(char *chromName);
/* Fetch entire chromosome into memory. */

struct dnaSeq *hExtSeq(char *acc);
/* Return sequence for external seq. */

struct dnaSeq *hRnaSeq(char *acc);
/* Return sequence for RNA. */

#endif /* HDB_H */
