/* rangeQuery - Generate and execute range queries. */
#include "common.h"
#include "jksql.h"
#include "hdb.h"

static char const rcsid[] = "";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rangeQuery - Generate and execute range queries\n"
  "usage:\n"
  "   rangeQuery database table chrom start end\n"
  );
}

void doQuery(char *database, char *table, char *chrom, int start, int end)
/* doQuery */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
char *empty = NULL;
int rowOffset = 0;
char **row;
int count = 0;

conn = sqlConnect(database);
// hSetDb(database);
sr = hRangeQuery(conn, table, chrom, start, end, empty, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
{
    count++;
}
printf("got %d rows\n", count);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int start = 0;
int end = 0;
char *e = NULL;
if (argc < 5)
    usage();
start = strtol(argv[4], &e, 0);
end = strtol(argv[5], &e, 0);
doQuery(argv[1], argv[2], argv[3], start, end);
return 0;
}
