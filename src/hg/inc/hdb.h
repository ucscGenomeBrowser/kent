/* hdb - human genome browser database. */

#ifndef HDB_H
#define HDB_H

#ifndef DNAUTIL_H
#include "dnautil.h"
#endif

void hDefaultConnect();
/* read the default settings from the config file */

void hSetDbConnect(char* host, char *db, char *user, char *password);
/* set the connection information for the database */

void hSetDb(char *dbName);
/* Set the database name. */

char *hGetDb();
/* Return the current database name. */

char *hGetDbHost();
/* Return the current database host. */

char *hGetDbName();
/* Return the current database name. */

char *hGetDbUser();
/* Return the current database user. */

char *hGetDbPassword();
/* Return the current database password. */

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

void hNibForChrom(char *chromName, char retNibName[512]);
/* Get .nib file associated with chromosome. */

struct slName *hAllChromNames();
/* Get list of all chromosomes. */

struct dnaSeq *hExtSeq(char *acc);
/* Return sequence for external seq. */

struct dnaSeq *hRnaSeq(char *acc);
/* Return sequence for RNA. */

char *hFreezeFromDb(char *database);
/* return the freeze for the database version. 
   For example: "db6" returns "Dec 12, 2000". If database
   not recognized returns NULL */

#endif /* HDB_H */
