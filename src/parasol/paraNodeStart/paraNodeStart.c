/* paraNodeStart - Start up parasol node daemons on a list of machines. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paraNodeStart - Start up parasol node daemons on a list of machines\n"
  "usage:\n"
  "   paraNodeStart XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void paraNodeStart(char *XXX)
/* paraNodeStart - Start up parasol node daemons on a list of machines. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
paraNodeStart(argv[1]);
return 0;
}
