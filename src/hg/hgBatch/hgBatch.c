/* hgBatch - Do batch queries of genome database over web. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgBatch - Do batch queries of genome database over web\n"
  "usage:\n"
  "   hgBatch XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgBatch(char *XXX)
/* hgBatch - Do batch queries of genome database over web. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
hgBatch(argv[1]);
return 0;
}
