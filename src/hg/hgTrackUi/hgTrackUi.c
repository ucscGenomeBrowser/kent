/* hgTrackUi - Display track-specific user interface.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTrackUi - Display track-specific user interface.\n"
  "usage:\n"
  "   hgTrackUi XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgTrackUi(char *XXX)
/* hgTrackUi - Display track-specific user interface.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgTrackUi(argv[1]);
return 0;
}
