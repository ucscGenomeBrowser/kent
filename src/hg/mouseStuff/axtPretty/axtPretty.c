/* axtPretty - Convert axt to more human readable format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtPretty - Convert axt to more human readable format.\n"
  "usage:\n"
  "   axtPretty XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void axtPretty(char *XXX)
/* axtPretty - Convert axt to more human readable format.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
axtPretty(argv[1]);
return 0;
}
