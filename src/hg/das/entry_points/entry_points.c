/* entry_points - DAS entry point server. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "entry_points - DAS entry point server\n"
  "usage:\n"
  "   entry_points XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void entry_points(char *XXX)
/* entry_points - DAS entry point server. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
entry_points(argv[1]);
return 0;
}
