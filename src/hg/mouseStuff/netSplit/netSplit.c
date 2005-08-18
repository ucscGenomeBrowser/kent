/* netSplit - Split a genome net file into chromosome net files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "chainNet.h"

static char const rcsid[] = "$Id: netSplit.c,v 1.4 2005/08/18 07:46:35 baertsch Exp $";

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
char fileName[512], tpath[512], cmd[512];
struct lineFile *lf = lineFileOpen(inNet, TRUE);
FILE *f, *meta ;
struct chainNet *net;
bool metaOpen = TRUE;
sprintf(tpath, "%s/meta.tmp", outDir);

makeDir(outDir);
meta = mustOpen(tpath,"w");
lineFileSetMetaDataOutput(lf, meta);
while ((net = chainNetRead(lf)) != NULL)
    {
    sprintf(fileName, "%s/%s.net", outDir, net->name);
    if (metaOpen)
        fclose(meta);
    metaOpen = FALSE;
    sprintf(cmd, "cat %s | sort -u > %s", tpath, fileName);
    system(cmd);
    f = mustOpen(fileName, "a");
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
