/* dataSim - Simulate system where data is dynamically distributed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dataSim - Simulate system where data is dynamically distributed\n"
  "usage:\n"
  "   dataSim XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void dataSim(char *XXX)
/* dataSim - Simulate system where data is dynamically distributed. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
dataSim(argv[1]);
return 0;
}
