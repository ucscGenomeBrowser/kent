/* faFilterN - Get rid of sequences with too many N's. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faFilterN - Get rid of sequences with too many N's\n"
  "usage:\n"
  "   faFilterN XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void faFilterN(char *XXX)
/* faFilterN - Get rid of sequences with too many N's. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
faFilterN(argv[1]);
return 0;
}
