/* rmFromFinf - Remove clones in list from finf file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "rmFromFinf - Remove clones in list from finf file\n"
  "usage:\n"
  "   rmFromFinf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void rmFromFinf(char *XXX)
/* rmFromFinf - Remove clones in list from finf file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
rmFromFinf(argv[1]);
return 0;
}
