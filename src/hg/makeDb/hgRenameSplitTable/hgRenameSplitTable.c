/* hgRenameSplitTable - Rename a table, or rename all tables in a split table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgRenameSplitTable - Rename a table, or rename all tables in a split table\n"
  "usage:\n"
  "   hgRenameSplitTable db old new\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void hgRenameSplitTable(char *db, char *oldRoot, char *newRoot)
/* hgRenameSplitTable - Rename a table, or rename all tables in a split table. */
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
    char *new = replaceChars(table->name, oldRoot, newRoot);
    safef(query, sizeof query, "ALTER TABLE %s RENAME TO %s", table->name, new);
    sqlUpdate(conn, query);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgRenameSplitTable(argv[1], argv[2], argv[3]);
return 0;
}
