/* hgDropSplitTable - Drop a table, or drop all tables in a split table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgDropSplitTable - Drop a table, or drop all tables in a split table\n"
  "usage:\n"
  "   hgDropSplitTable db tableRoo \n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void hgDropSplitTable(char *db, char *oldRoot)
/* hgDropSplitTable - Drop a table, or drop all tables in a split table. */
{
struct slName *tables, *table;
char query[128];
struct sqlConnection *conn;

if (!hDbExists(db))
    errAbort("Non-existent database: %s", db);
tables = hSplitTableNames(db, oldRoot);
if (!tables)
    errAbort("Can't find table: %s\n", oldRoot);
conn = sqlConnect(db);
for (table = tables; table != NULL; table = table->next)
    {
    safef(query, sizeof query, "DROP TABLE %s", table->name);
    sqlUpdate(conn, query);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hgDropSplitTable(argv[1], argv[2]);
return 0;
}
