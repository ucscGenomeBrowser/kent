/* hgGateway - Human Genome Browser Gateway. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGateway - Human Genome Browser Gateway\n"
  "usage:\n"
  "   hgGateway XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void hgGateway(char *XXX)
/* hgGateway - Human Genome Browser Gateway. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
hgGateway(argv[1]);
return 0;
}
