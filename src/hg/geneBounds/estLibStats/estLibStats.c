/* estLibStats - Calculate some stats on EST libraries given file from polyInfo. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "estLibStats - Calculate some stats on EST libraries given file from polyInfo\n"
  "usage:\n"
  "   estLibStats XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void estLibStats(char *XXX)
/* estLibStats - Calculate some stats on EST libraries given file from polyInfo. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
estLibStats(argv[1]);
return 0;
}
