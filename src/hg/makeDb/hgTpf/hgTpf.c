/* hgTpf - Make TPF table. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgTpf - Make TPF table\n"
  "usage:\n"
  "   hgTpf XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgTpf(char *XXX)
/* hgTpf - Make TPF table. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgTpf(argv[1]);
return 0;
}
