/* axtSort - Sort axt files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtSort - Sort axt files\n"
  "usage:\n"
  "   axtSort in.axt out.axt\n"
  "options:\n"
  "   -query - Sort by query position, not target\n"
  );
}

void axtSort(char *in, char *out, boolean byQuery)
/* axtSort - Sort axt files. */
{
struct axt *axtList = NULL, *axt;
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f = mustOpen(out, "w");

while ((axt = axtRead(lf)) != NULL)
    {
    slAddHead(&axtList, axt);
    }
if (byQuery)
    slSort(&axtList, axtCmpQuery);
else
    slSort(&axtList, axtCmpTarget);
for (axt = axtList; axt != NULL; axt = axt->next)
    axtWrite(axt, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
axtSort(argv[1], argv[2], optionExists("query"));
return 0;
}
