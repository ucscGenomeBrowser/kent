/* oneShotrmFromFinf - Remove clones in list from finf file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "oneShotrmFromFinf - Remove clones in list from finf file\n"
  "usage:\n"
  "   oneShotrmFromFinf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void oneShotrmFromFinf(char *XXX)
/* oneShotrmFromFinf - Remove clones in list from finf file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
oneShotrmFromFinf(argv[1]);
return 0;
}
