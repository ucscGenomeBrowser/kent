/* addMhcClones - Add info on missing MHC files to Greg's stuff.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "addMhcClones - Add info on missing MHC files to Greg's stuff.\n"
  "usage:\n"
  "   addMhcClones XXX\n"
  "options:\n"
  "   -xxx=XXX\n");
}

void addMhcClones(char *XXX)
/* addMhcClones - Add info on missing MHC files to Greg's stuff.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
addMhcClones(argv[1]);
return 0;
}
