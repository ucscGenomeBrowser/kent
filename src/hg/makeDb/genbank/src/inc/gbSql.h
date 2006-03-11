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

#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
