/* polyInfo - Collect info on polyAdenylation signals etc. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "polyInfo - Collect info on polyAdenylation signals etc\n"
  "usage:\n"
  "   polyInfo XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void polyInfo(char *XXX)
/* polyInfo - Collect info on polyAdenylation signals etc. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
polyInfo(argv[1]);
return 0;
}
