/* flagMhcClones - Look for clones Stephan wants in MHC.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "flagMhcClones - Look for clones Stephan wants in MHC.\n"
  "usage:\n"
  "   flagMhcClones XXX\n"
  "options:\n"
  "   -xxx=XXX\n");
}

void flagMhcClones(char *XXX)
/* flagMhcClones - Look for clones Stephan wants in MHC.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
flagMhcClones(argv[1]);
return 0;
}
