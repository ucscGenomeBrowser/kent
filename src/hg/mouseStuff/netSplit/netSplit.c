/* netSplit - Split a genome net file into chromosome net files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "chainNet.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "netSplit - Split a genome net file into chromosome net files\n"
  "usage:\n"
  "   netSplit in.net outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

void netSplit(char *inNet, char *outDir)
/* netSplit - Split a genome net file into chromosome net files. */
{
char fileName[512];
struct lineFile *lf = lineFileOpen(inNet, TRUE);
FILE *f;
struct chainNet *net;

makeDir(outDir);
while ((net = chainNetRead(lf)) != NULL)
    {
    sprintf(fileName, "%s/%s.net", outDir, net->name);
    f = mustOpen(fileName, "w");
    printf("Writing %s\n", fileName);
    chainNetWrite(net, f);
    carefulClose(&f);
    chainNetFree(&net);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 3)
    usage();
netSplit(argv[1], argv[2]);
return 0;
}
