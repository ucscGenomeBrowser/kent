/* expToRna - Make a little two column table that associates rnaClusters with expression info. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "expToRna - Make a little two column table that associates rnaClusters with expression info\n"
  "usage:\n"
  "   expToRna XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void expToRna(char *XXX)
/* expToRna - Make a little two column table that associates rnaClusters with expression info. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
expToRna(argv[1]);
return 0;
}
