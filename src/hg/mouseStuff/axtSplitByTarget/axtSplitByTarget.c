/* axtSplitByTarget - Split a single axt file into one file per target. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "axt.h"

static char const rcsid[] = "$Id: axtSplitByTarget.c,v 1.5 2004/02/02 20:30:40 angie Exp $";

int tStartSize = 0;
double tSS = 0.0;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtSplitByTarget - Split a single axt file into one file per target\n"
  "usage:\n"
  "   axtSplitByTarget in.axt outDir\n"
  "options:\n"
  "   -tStartSize=N - Split into multiple files per target, with each file\n"
  "                   outDir/tName.M.axt containing records whose tStart is\n"
  "                   between M*N and ((M+1)*N)-1, i.e. M = floor(tStart/N).\n"
  );
}

static FILE *getSplitFile(struct hash *outHash, char *outDir, char *tName,
			  int tStart)
{
char outName[1024];
FILE *f;
if (tStartSize > 0)
    {
    int fNum = (int)floor((double)tStart / tSS);
    safef(outName, sizeof(outName), "%s/%s.%d.axt", outDir, tName, fNum);
    }
else
    {
    safef(outName, sizeof(outName), "%s/%s.axt", outDir, tName);
    }
f = hashFindVal(outHash, outName);
if (f == NULL)
    {
    f = mustOpen(outName, "w");
    hashAdd(outHash, outName, f);
    }
return f;
}

void axtSplitByTarget(char *inName, char *outDir)
/* axtSplitByTarget - Split a single axt file into one file per target. */
{
struct hash *outHash = newHash(8);  /* FILE valued hash */
struct lineFile *lf = lineFileOpen(inName, TRUE);
struct axt *axt;

makeDir(outDir);
while ((axt = axtRead(lf)) != NULL)
    {
    FILE *f = getSplitFile(outHash, outDir, axt->tName, axt->tStart);
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
tStartSize = optionInt("tStartSize", 0);
tSS = (double)tStartSize;
axtSplitByTarget(argv[1], argv[2]);
return 0;
}
