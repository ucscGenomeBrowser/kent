/* selectTrainedHits - Select only hits from sites that we've trained on. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "selectTrainedHits - Select only hits from sites that we've trained on\n"
  "usage:\n"
  "   selectTrainedHits XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void selectTrainedHits(char *XXX)
/* selectTrainedHits - Select only hits from sites that we've trained on. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
selectTrainedHits(argv[1]);
return 0;
}
