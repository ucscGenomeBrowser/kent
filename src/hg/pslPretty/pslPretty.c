/* pslPretty - Convert PSL to human readable output. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslPretty - Convert PSL to human readable output\n"
  "usage:\n"
  "   pslPretty XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void pslPretty(char *XXX)
/* pslPretty - Convert PSL to human readable output. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
pslPretty(argv[1]);
return 0;
}
