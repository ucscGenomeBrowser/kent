/* t - Test loader. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "t - Test loader\n"
  "usage:\n"
  "   t XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void t(char *XXX)
/* t - Test loader. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
t(argv[1]);
return 0;
}
