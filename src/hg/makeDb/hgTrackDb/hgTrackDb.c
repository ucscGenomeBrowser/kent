/* hgTrackDb - Create trackDb table from text files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTrackDb - Create trackDb table from text files\n"
  "usage:\n"
  "   hgTrackDb XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgTrackDb(char *XXX)
/* hgTrackDb - Create trackDb table from text files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgTrackDb(argv[1]);
return 0;
}
