/* Various genbank sql-related functions. */
#ifndef GBSQL_H
#define GBSQL_H

struct sqlConnection *conn;

void gbSqlDupTableDef(struct sqlConnection *conn, char* table, char* newTable);
/* create a duplicate of a table with the right indices. */

void gbAddTableIfExists(struct sqlConnection *conn, char* table,
                        struct slName** tableList);
/* Add a table to a list, if it exists */


#endif

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
