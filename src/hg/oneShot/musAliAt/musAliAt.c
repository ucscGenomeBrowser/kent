/* musAliAt - Produce .fa files where mouse alignments hit on chr22. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "musAliAt - Produce .fa files where mouse alignments hit on chr22\n"
  "usage:\n"
  "   musAliAt XXX\n"
  "options:\n"
  "   -xxx=XXX\n");
}

void musAliAt(char *XXX)
/* musAliAt - Produce .fa files where mouse alignments hit on chr22. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
musAliAt(argv[1]);
return 0;
}
