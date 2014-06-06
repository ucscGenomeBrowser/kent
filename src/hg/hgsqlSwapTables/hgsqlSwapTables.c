/* hgsqlSwapTables - swaps tables in database. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "hdb.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgsqlSwapTables - swaps tables in database\n"
  "usage:\n"
  "   hgsqlSwapTables database table1 table2 table3\n"
  "Renames table2 to table3, then renames table1 to table2.\n"
  "Checks for existance of table1 and table2 and lack of existance\n"
  "of table3 and fails without change if not verified\n"
  "options:\n"
  "   -okNoTable2  --  don't fail if table2 doesn't exist\n"
  "   -dropTable3  --  drop table3\n"
  );
}

boolean okNoTable2 = FALSE;
boolean dropTable3 = FALSE;

static struct optionSpec options[] = {
   {"okNoTable2", OPTION_BOOLEAN},
   {"dropTable3", OPTION_BOOLEAN},
   {NULL, 0},
};

void hgsqlSwapTables(char *database, char *table1, char *table2, char *table3)
/* hgsqlSwapTables - swaps tables in database. */
{
struct sqlConnection *conn = hAllocConn(database);
boolean noTable2 = FALSE;

if (!sqlTableExists(conn, table1))
    errAbort("%s does not exist",table1);

if (!sqlTableExists(conn, table2))
    {
    if (okNoTable2)
        noTable2 = TRUE;
    else
        errAbort("%s does not exist", table2);
    }

if (sqlTableExists(conn, table3))
    {
    if (dropTable3)
        sqlDropTable(conn, table3);
    else
        errAbort("%s exists", table3);
    }

if (!noTable2)
    sqlRenameTable(conn, table2, table3);

sqlRenameTable(conn, table1, table2);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();

okNoTable2 = optionExists("okNoTable2");
dropTable3 = optionExists("dropTable3");

hgsqlSwapTables(argv[1],argv[2],argv[3],argv[4]);
return 0;
}
