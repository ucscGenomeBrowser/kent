/* intronEnds - Gather stats on intron ends.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "intronEnds - Gather stats on intron ends.\n"
  "usage:\n"
  "   intronEnds XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void intronEnds(char *XXX)
/* intronEnds - Gather stats on intron ends.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
intronEnds(argv[1]);
return 0;
}
