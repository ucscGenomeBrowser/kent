/* textHist2 - Make two dimensional histogram. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "textHist2 - Make two dimensional histogram\n"
  "usage:\n"
  "   textHist2 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void textHist2(char *XXX)
/* textHist2 - Make two dimensional histogram. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
textHist2(argv[1]);
return 0;
}
