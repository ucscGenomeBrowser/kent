/* sampleToWig - Convert sample format to denser wig(gle) format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sampleToWig - Convert sample format to denser wig(gle) format\n"
  "usage:\n"
  "   sampleToWig XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void sampleToWig(char *XXX)
/* sampleToWig - Convert sample format to denser wig(gle) format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
sampleToWig(argv[1]);
return 0;
}
