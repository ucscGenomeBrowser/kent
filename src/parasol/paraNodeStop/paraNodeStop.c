/* paraNodeStop - Shut down parasol node daemons on a list of machines. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paraNodeStop - Shut down parasol node daemons on a list of machines\n"
  "usage:\n"
  "   paraNodeStop XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void paraNodeStop(char *XXX)
/* paraNodeStop - Shut down parasol node daemons on a list of machines. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
paraNodeStop(argv[1]);
return 0;
}
