/* wikiPlot - Quick plots of maps vs. each other. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "wikiPlot - Quick plots of maps vs. each other\n"
  "usage:\n"
  "   wikiPlot XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void wikiPlot(char *XXX)
/* wikiPlot - Quick plots of maps vs. each other. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
wikiPlot(argv[1]);
return 0;
}
