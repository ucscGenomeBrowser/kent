/* trackInfo - Centralized repository for track information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trackInfo - Centralized repository for track information\n"
  "usage:\n"
  "   trackInfo XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void trackInfo(char *XXX)
/* trackInfo - Centralized repository for track information. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
trackInfo(argv[1]);
return 0;
}
