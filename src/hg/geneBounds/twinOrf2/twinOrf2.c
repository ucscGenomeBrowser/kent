/* twinOrf2 - Predict open reading frame in cDNA given a cross species alignment. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twinOrf2 - Predict open reading frame in cDNA given a cross species alignment\n"
  "usage:\n"
  "   twinOrf2 XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void twinOrf2(char *XXX)
/* twinOrf2 - Predict open reading frame in cDNA given a cross species alignment. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
twinOrf2(argv[1]);
return 0;
}
