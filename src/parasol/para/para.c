/* para - para - manage a batch of jobs in parallel on a compute cluster.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "para - para - manage a batch of jobs in parallel on a compute cluster.\n"
  "usage:\n"
  "   para XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void para(char *XXX)
/* para - para - manage a batch of jobs in parallel on a compute cluster.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
para(argv[1]);
return 0;
}
