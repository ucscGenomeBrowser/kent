/* eisenInput - Create input for Eisen-style cluster program. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eisenInput - Create input for Eisen-style cluster program\n"
  "usage:\n"
  "   eisenInput XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void eisenInput(char *XXX)
/* eisenInput - Create input for Eisen-style cluster program. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
eisenInput(argv[1]);
return 0;
}
