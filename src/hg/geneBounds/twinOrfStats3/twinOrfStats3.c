/* twinOrfStats3 - Paired ORF trainer take three. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twinOrfStats3 - Paired ORF trainer take three\n"
  "usage:\n"
  "   twinOrfStats3 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void twinOrfStats3(char *XXX)
/* twinOrfStats3 - Paired ORF trainer take three. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
twinOrfStats3(argv[1]);
return 0;
}
