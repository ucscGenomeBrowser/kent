#include "common.h"
#include "gbSql.h"
#include "jksql.h"

void gbSqlDupTableDef(struct sqlConnection *conn, char* table,
                      char* newTable)
/* create a duplicate of a table with the right indices. */
{
char query[2048], *createArgs;
struct sqlResult* sr;
char** row;

/* Returns two columns of table name and command */
safef(query, sizeof(query), "SHOW CREATE TABLE %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (sameString(row[0], table))
        break;
    }
if (row == NULL)
    errAbort("can't get create table command for: %s", table);
createArgs = strchr(row[1], '(');
if (createArgs == NULL)
    errAbort("don't understand returned create table command: %s", row[1]);
safef(query, sizeof(query), "CREATE TABLE %s %s", newTable, createArgs);
sqlFreeResult(&sr);
sqlUpdate(conn, query);
}

void gbAddTableIfExists(struct sqlConnection *conn, char* table,
                        struct slName** tableList)
{
/* Add a table to a list, if it exists */
if (sqlTableExists(conn, table))
    slSafeAddHead(tableList, slNameNew(table));
}


/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
