/* sim4big - A wrapper for Sim4 that runs it repeatedly on a multi-sequence .fa file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sim4big - A wrapper for Sim4 that runs it repeatedly on a multi-sequence .fa file\n"
  "usage:\n"
  "   sim4big XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void sim4big(char *XXX)
/* sim4big - A wrapper for Sim4 that runs it repeatedly on a multi-sequence .fa file. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
sim4big(argv[1]);
return 0;
}
