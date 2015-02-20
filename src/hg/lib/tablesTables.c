/* tablesTables - this module deals with two types of tables SQL tables in a database,
 * and fieldedTable objects in memory. It has routines to do sortable, filterable web
 * displays on tables. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "fieldedTable.h"
#include "web.h"
#include "cart.h"

struct fieldedTable *fieldedTableFromDbQuery(struct sqlConnection *conn, char *query)
/* Return fieldedTable from a database query */
{
struct sqlResult *sr = sqlGetResult(conn, query);
char **fields;
int fieldCount = sqlResultFieldArray(sr, &fields);
struct fieldedTable *table = fieldedTableNew(query, fields, fieldCount);
char **row;
int i = 0;
while ((row = sqlNextRow(sr)) != NULL)
    fieldedTableAdd(table, row, fieldCount, ++i);
sqlFreeResult(&sr);
return table;
}

