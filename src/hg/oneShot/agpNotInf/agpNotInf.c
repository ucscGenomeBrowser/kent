/* agpNotInf - List clones in .agp file not in .inf file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpNotInf - List clones in .agp file not in .inf file\n"
  "usage:\n"
  "   agpNotInf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void agpNotInf(char *XXX)
/* agpNotInf - List clones in .agp file not in .inf file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
agpNotInf(argv[1]);
return 0;
}
