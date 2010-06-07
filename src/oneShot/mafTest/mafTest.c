/* mafTest - Testing out maf stuff. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "maf.h"
#include "mafXml.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mafTest - Testing out maf stuff\n"
  "usage:\n"
  "   mafTest in out\n"
  );
}

void mafTest(char *inFile, char *outFile)
/* mafTest - Testing out maf stuff. */
{
struct mafFile *mm = NULL;
FILE *f = mustOpen(outFile, "w");

if (endsWith(inFile, ".maf"))
   mm = mafReadAll(inFile);
else if (endsWith(inFile, ".axt"))
   {
   struct lineFile *lf = lineFileOpen(inFile, TRUE);
   while (axtRead(lf) != NULL)
       ;
   }
else
   mm = mafMafLoad(inFile);
mafMafSave(mm, 0, f);
mafFileFreeList(&mm);
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
