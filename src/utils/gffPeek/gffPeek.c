/* gffPeek - Look at a gff file and report some basic stats. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "gffPeek - Look at a gff file and report some basic stats\n"
  "usage:\n"
  "   gffPeek XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void gffPeek(char *XXX)
/* gffPeek - Look at a gff file and report some basic stats. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
gffPeek(argv[1]);
return 0;
}
