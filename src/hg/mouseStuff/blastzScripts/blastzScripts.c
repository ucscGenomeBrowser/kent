/* blastzScripts - Scripts for processing blastz axt files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "blastzScripts - Scripts for processing blastz axt files\n"
  "usage:\n"
  "   blastzScripts XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void blastzScripts(char *XXX)
/* blastzScripts - Scripts for processing blastz axt files. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
blastzScripts(argv[1]);
return 0;
}
