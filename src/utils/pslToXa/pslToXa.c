/* pslToXa - Convert from psl to xa alignment format. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslToXa - Convert from psl to xa alignment format\n"
  "usage:\n"
  "   pslToXa XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void pslToXa(char *XXX)
/* pslToXa - Convert from psl to xa alignment format. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
pslToXa(argv[1]);
return 0;
}
