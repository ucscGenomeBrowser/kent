/* minIndexLength - check chrom names to find the best size for chrom index. */
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
  "minIndexLength - check chrom names to find the best size for chrom index\n"
  "usage:\n"
  "   minIndexLength <database>\n"
  "options:\n"
  "   <database> - examine chromInfo chrom names in this database\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void minIndexLength(char *database)
/* minIndexLength - check chrom names to find the best size for chrom index. */
{
    int minLength = hGetMinIndexLength(database);
    printf("# database: %s, minIndexLength: %d\n", database, minLength);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
minIndexLength(argv[1]);
return 0;
}
