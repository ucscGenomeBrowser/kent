/* blatFilter - filter blat alignments somewhat. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blatFilter - filter blat alignments somewhat\n"
  "usage:\n"
  "   blatFilter XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void blatFilter(char *XXX)
/* blatFilter - filter blat alignments somewhat. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
blatFilter(argv[1]);
return 0;
}
