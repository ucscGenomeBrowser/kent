/* clusterRna - Make clusters of mRNA and ESTs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "clusterRna - Make clusters of mRNA and ESTs\n"
  "usage:\n"
  "   clusterRna XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void clusterRna(char *XXX)
/* clusterRna - Make clusters of mRNA and ESTs. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
clusterRna(argv[1]);
return 0;
}
