/* axtSplitByTarget - Split a single axt file into one file per target. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "axt.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtSplitByTarget - Split a single axt file into one file per target\n"
  "usage:\n"
  "   axtSplitByTarget in.axt outDir\n"
  );
}

void axtSplitByTarget(char *inName, char *outDir)
/* axtSplitByTarget - Split a single axt file into one file per target. */
{
char outName[512];
struct hash *outHash = newHash(8);  /* FILE valued hash */
struct lineFile *lf = lineFileOpen(inName, TRUE);
struct axt *axt;

makeDir(outDir);
while ((axt = axtRead(lf)) != NULL)
    {
    FILE *f = hashFindVal(outHash, axt->tName);
    if (f == NULL)
        {
	sprintf(outName, "%s/%s.axt", outDir, axt->tName);
	f = mustOpen(outName, "w");
	hashAdd(outHash, axt->tName, f);
	}
    axtWrite(axt, f);
    axtFree(&axt);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
axtSplitByTarget(argv[1], argv[2]);
return 0;
}
