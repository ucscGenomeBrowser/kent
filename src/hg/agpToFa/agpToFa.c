/* agpToFa - Convert a .agp file to a .fa file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpToFa - Convert a .agp file to a .fa file\n"
  "usage:\n"
  "   agpToFa XXX\n"
  "options:\n"
  "   -xxx=XXX\n");
}

void agpToFa(char *XXX)
/* agpToFa - Convert a .agp file to a .fa file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
agpToFa(argv[1]);
return 0;
}
