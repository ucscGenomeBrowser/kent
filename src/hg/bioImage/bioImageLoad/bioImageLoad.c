/* bioImageLoad - Load data into bioImage database. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bioImageLoad - Load data into bioImage database\n"
  "usage:\n"
  "   bioImageLoad setInfo.ra itemInfo.tab\n"
  "Please see bioImageLoad.doc for description of the .ra and .tab files\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void bioImageLoad(char *XXX)
/* bioImageLoad - Load data into bioImage database. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
bioImageLoad(argv[1]);
return 0;
}
