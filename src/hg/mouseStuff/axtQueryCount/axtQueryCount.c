/* axtQueryCount - Count bases covered on each query sequence. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtQueryCount - Count bases covered on each query sequence\n"
  "usage:\n"
  "   axtQueryCount XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtQueryCount(char *XXX)
/* axtQueryCount - Count bases covered on each query sequence. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtQueryCount(argv[1]);
return 0;
}
