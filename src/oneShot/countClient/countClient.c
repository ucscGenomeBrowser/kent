/* countClient - Connect repeatedly to the count server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "countClient - Connect repeatedly to the count server\n"
  "usage:\n"
  "   countClient XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void countClient(char *XXX)
/* countClient - Connect repeatedly to the count server. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
countClient(argv[1]);
return 0;
}
