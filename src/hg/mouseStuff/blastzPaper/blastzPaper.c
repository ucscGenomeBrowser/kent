/* blastzPaper - blastz paper manuscript. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blastzPaper - blastz paper manuscript\n"
  "usage:\n"
  "   blastzPaper XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void blastzPaper(char *XXX)
/* blastzPaper - blastz paper manuscript. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
blastzPaper(argv[1]);
return 0;
}
