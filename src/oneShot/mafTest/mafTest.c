/* mafTest - Testing out maf stuff. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafTest - Testing out maf stuff\n"
  "usage:\n"
  "   mafTest in out\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void mafTest(char *inFile, char *outFile)
/* mafTest - Testing out maf stuff. */
{
struct mafFile *mm = mafReadAll(inFile);
mafWriteAll(mm, outFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
mafTest(argv[1], argv[2]);
return 0;
}
