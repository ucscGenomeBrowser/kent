/* twinOrf3 - Paired ORF finder take three. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twinOrf3 - Paired ORF finder take three\n"
  "usage:\n"
  "   twinOrf3 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void twinOrf3(char *XXX)
/* twinOrf3 - Paired ORF finder take three. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
twinOrf3(argv[1]);
return 0;
}
