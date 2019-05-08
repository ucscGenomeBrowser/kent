/* binFromRange - Translate a 0-based half open start and end into a bin range sql expression.. */
#include "common.h"
#include "options.h"
#include "hdb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "binFromRange - Translate a 0-based half open start and end into a bin range sql expression.\n"
  "usage:\n"
  "   binFromRange start end\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {NULL, 0},
};

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
int start = atoi(argv[1]);
int end = atoi(argv[2]);
struct dyString *query = dyStringNew(0);
hAddBinToQuery(start, end, query);
puts(query->string);
return 0;
}
