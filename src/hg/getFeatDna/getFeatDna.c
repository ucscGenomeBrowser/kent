/* getFeatDna - Get dna for a type of feature. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "getFeatDna - Get dna for a type of feature\n"
  "usage:\n"
  "   getFeatDna XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void getFeatDna(char *XXX)
/* getFeatDna - Get dna for a type of feature. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 2)
    usage();
getFeatDna(argv[1]);
return 0;
}
