/* makeOrf2gene - Convert from Lincoln Stein's orf2gene dump to our simple format. */
#include "common.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "makeOrf2gene - Convert from Lincoln Stein's orf2gene dump to our simple format\n"
  "usage:\n"
  "   makeOrf2gene XXX\n");
}

void makeOrf2gene(char *XXX)
/* makeOrf2gene - Convert from Lincoln Stein's orf2gene dump to our simple format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
    usage();
makeOrf2gene(argv[1]);
return 0;
}
