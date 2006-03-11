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

unsigned gbSqlUnsignedNull(char *s)
/* parse an unsigned, allowing empty string to return zero */
{
if (s[0] == '\0')
    return 0;
else
    return sqlUnsigned(s);
}

int gbSqlSignedNull(char *s)
/* parse an int, allowing empty string to return zero */
{
if (s[0] == '\0')
    return 0;
else
    return sqlSigned(s);
}

struct slName* gbSqlListTablesLike(struct sqlConnection *conn, char *like)
/* get list of tables matching a pattern */
{
char query[256];
safef(query, sizeof(query), "show tables like '%s'", like);
return sqlQuickList(conn, query);
}

/* mysql lock suffix, database if prefixed, just in case we want multiple
 * gbLoadRnas on different databases*/
static char *GB_LOCK_NAME = "genbank";

void gbLockDb(struct sqlConnection *conn, char *db)
/* get an advisory lock to keep two genbank process from updating
 * the database at the same time.  If db is null, use the database
 * associated with the connection. */
{
char query[128];
int got;
if (db == NULL)
    db = sqlGetDatabase(conn);
safef(query, sizeof(query), "SELECT GET_LOCK(\"%s.%s\", 0)", db, GB_LOCK_NAME);
got = sqlNeedQuickNum(conn, query);
if (!got)
    errAbort("failed to get lock %s.%s", db, GB_LOCK_NAME);
}

void gbUnlockDb(struct sqlConnection *conn, char *db)
/* free genbank advisory lock on database */
{
char query[128];
if (db == NULL)
    db = sqlGetDatabase(conn);
safef(query, sizeof(query), "SELECT RELEASE_LOCK(\"%s.%s\")", db, GB_LOCK_NAME);
sqlUpdate(conn, query);
}



/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
