/* missingTable - test what happens when database is missing a table. */
#include "common.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "missingTable - test what happens when database is missing a table\n"
  "usage:\n"
  "   missingTable database table\n");
}

void missingTable(char *database, char *table)
/* missingTable - test what happens when database is missing a table. */
{
struct sqlConnection *conn = sqlConnect(database);
boolean exists = sqlTableExists(conn, table);

printf("database %s, table %s, exists %s\n",
	database, table, (exists? "yes" : "no"));

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
missingTable(argv[1], argv[2]);
return 0;
}
