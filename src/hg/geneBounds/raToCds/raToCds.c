/* raToCds - Extract CDS positions from ra file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "raToCds - Extract CDS positions from ra file\n"
  "usage:\n"
  "   raToCds XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void raToCds(char *XXX)
/* raToCds - Extract CDS positions from ra file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
raToCds(argv[1]);
return 0;
}
