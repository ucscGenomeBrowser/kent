/* parasol - Parasol program - for launching programs in parallel on a computer cluster. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "parasol - Parasol program - for launching programs in parallel on a computer cluster\n"
  "usage:\n"
  "   parasol XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void parasol(char *XXX)
/* parasol - Parasol program - for launching programs in parallel on a computer cluster. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
parasol(argv[1]);
return 0;
}
