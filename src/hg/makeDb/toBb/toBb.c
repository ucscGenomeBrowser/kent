/* toBb - Convert tables to bb format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "toBb - Convert tables to bb format\n"
  "usage:\n"
  "   toBb XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void toBb(char *XXX)
/* toBb - Convert tables to bb format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
toBb(argv[1]);
return 0;
}
