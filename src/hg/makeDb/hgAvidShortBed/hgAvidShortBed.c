/* hgAvidShortBed - Convert short form of AVID alignments to BED. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgAvidShortBed - Convert short form of AVID alignments to BED\n"
  "usage:\n"
  "   hgAvidShortBed XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgAvidShortBed(char *XXX)
/* hgAvidShortBed - Convert short form of AVID alignments to BED. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgAvidShortBed(argv[1]);
return 0;
}
