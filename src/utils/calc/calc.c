/* calc - Little command line calculator. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "calc - Little command line calculator\n"
  "usage:\n"
  "   calc XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void calc(char *XXX)
/* calc - Little command line calculator. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
calc(argv[1]);
return 0;
}
