/* rangeQuery - Generate and execute range queries. */
#include "common.h"
#include "hdb.h"
#include "jksql.h"
#include "options.h"
// for clock1000()
#include "portable.h"

static char const rcsid[] = "";

/* Command line switches. */
int iterations = 1;        /* Number of iterations. */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"iter", OPTION_INT},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rangeQuery - Generate and execute range queries\n"
  "usage:\n"
  "   rangeQuery database table chrom start end\n"
  "options:\n"
  "    -iter   number of iterations (incrementing end pos)\n"
  );
}

void doQuery(char *database, char *table, char *chrom, int start, int end)
/* doQuery */
{
struct sqlConnection *conn = NULL;
struct sqlResult *sr = NULL;
char *empty = NULL;
int rowOffset = 0;
// char **row;
// int count = 0;
long startTime = 0;
long deltaTime = 0;
int i;

conn = sqlConnect(database);


// hSetDb(database);

for (i = 0; i < iterations; i++)
{
  startTime = clock1000();
  sr = hRangeQuery(conn, table, chrom, start, end, empty, &rowOffset);
  deltaTime = clock1000() - startTime;
  printf("time for hRangeQuery = %0.3fs\n", ((double)deltaTime)/1000.0);
  end++;
  sqlFreeResult(&sr);
  sleep1000(500);
}

// startTime = clock1000();

// while ((row = sqlNextRow(sr)) != NULL)
// {
    // count++;
// }
// printf("got %d rows\n", count);

// sqlFreeResult(&sr);
// deltaTime = clock1000() - startTime;
// printf("time for sqlFreeResult = %0.3fs\n", ((double)deltaTime)/1000.0);

// sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int start = 0;
int end = 0;
char *e = NULL;
if (argc < 5)
    usage();
optionInit(&argc, argv, optionSpecs);
iterations = optionInt("iter", 1);
start = strtol(argv[4], &e, 0);
end = strtol(argv[5], &e, 0);
doQuery(argv[1], argv[2], argv[3], start, end);
return 0;
}
