/* agpToGl - Convert AGP file to GL file.  Some fakery involved.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "agpToGl - Convert AGP file to GL file.  Some fakery involved.\n"
  "usage:\n"
  "   agpToGl XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void agpToGl(char *XXX)
/* agpToGl - Convert AGP file to GL file.  Some fakery involved.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
agpToGl(argv[1]);
return 0;
}
