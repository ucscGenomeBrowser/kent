/* mhcToNt - Convert Roger's MHC file to NT file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mhcToNt - Convert Roger's MHC file to NT file\n"
  "usage:\n"
  "   mhcToNt XXX\n"
  "options:\n"
  "   -xxx=XXX\n");
}

void mhcToNt(char *XXX)
/* mhcToNt - Convert Roger's MHC file to NT file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
mhcToNt(argv[1]);
return 0;
}
