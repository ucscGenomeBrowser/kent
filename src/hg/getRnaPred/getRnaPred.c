/* getRnaPred - Get RNA for gene predictions. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getRnaPred - Get RNA for gene predictions\n"
  "usage:\n"
  "   getRnaPred XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void getRnaPred(char *XXX)
/* getRnaPred - Get RNA for gene predictions. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
getRnaPred(argv[1]);
return 0;
}
