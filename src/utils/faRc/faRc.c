/* faRc - Reverse complement a FA file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faRc - Reverse complement a FA file\n"
  "usage:\n"
  "   faRc XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void faRc(char *XXX)
/* faRc - Reverse complement a FA file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
faRc(argv[1]);
return 0;
}
