/* Various genbank sql-related functions. */
#ifndef GBSQL_H
#define GBSQL_H

struct sqlConnection *conn;

void gbSqlDupTableDef(struct sqlConnection *conn, char* table, char* newTable);
/* create a duplicate of a table with the right indices. */

void gbAddTableIfExists(struct sqlConnection *conn, char* table,
                        struct slName** tableList);
/* Add a table to a list, if it exists */

unsigned gbSqlUnsignedNull(char *s);
/* parse an unsigned, allowing empty string to return zero */

int gbSqlSignedNull(char *s);
/* parse an int, allowing empty string to return zero */

struct slName* gbSqlListTablesLike(struct sqlConnection *conn, char *like);
/* get list of tables matching a pattern */

void gbLockDb(struct sqlConnection *conn, char *db);
/* get an advisory lock to keep two genbank process from updating
 * the database at the same time.  If db is null, use the database
 * associated with the connection. */

void gbUnlockDb(struct sqlConnection *conn, char *db);
/* free genbank advisory lock on database */

char *gbSqlStrOrNullTabVar(char *str);
/* If str is null, return \N for loading tab file, otherwise str */

/***** the following are table building methods *****/
/* Select flags indicate sets of tables: */
#define TBLBLD_REAL_TABLE 0x1  /* base table */
#define TBLBLD_TMP_TABLE  0x2  /* _tmp table */
#define TBLBLD_OLD_TABLE  0x4  /* _old table */

void tblBldGetTmpName(char *tmpTable, int tmpBufSize, char *table);
/* generate the temporary table name */

void tblBldDrop(struct sqlConnection *conn, char *table, unsigned selFlags);
/* Drop a tables based on the set of select flags: TBLBLD_REAL_TABLE,
 * TBLBLD_TMP_TABLE, TBLBLD_OLD_TABLE */

void tblBldDropTables(struct sqlConnection *conn, char **tables, unsigned selFlags);
/* Drop a list of tables based on the set of select flags: TBLBLD_REAL_TABLE,
 * TBLBLD_TMP_TABLE, TBLBLD_OLD_TABLE */

void tblBldRemakePslTable(struct sqlConnection *conn, char *table, char *insertTable);
/* remake a PSL table based on another PSL that is going to be inserted into
 * it. */

void tblBldAtomicInstall(struct sqlConnection *conn, char **tables);
/* Install the tables in the NULL terminated in an atomic manner.  Drop
 * under tbl_old first, then renametbl to tbl_old, and tbl_tmp to tbl. */

void tblBldGenePredFromPsl(struct sqlConnection *conn, char *tmpDir, char *pslTbl,
                           char *genePredTbl, FILE *warnFh);
/* build a genePred table from a PSL table, output warnings about missing or
 * invalid CDS if warnFh is not NULL */

struct slName *getChromNames(char *db);
/* get a list of chrom names; do not modify results, as it is cached */

#endif
