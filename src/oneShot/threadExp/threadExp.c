/* threadExp - Some pthread experiments. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "threadExp - Some pthread experiments\n"
  "usage:\n"
  "   threadExp XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void threadExp(char *XXX)
/* threadExp - Some pthread experiments. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
threadExp(argv[1]);
return 0;
}
