/* netSplit - Split a genome net file into chromosome net files. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
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
char fileName[512], tpath[512], cmd[512];
struct lineFile *lf = lineFileOpen(inNet, TRUE);
FILE *f, *meta ;
struct chainNet *net;
bool metaOpen = TRUE;
safef(tpath, sizeof(tpath), "%s/meta.tmp", outDir);

makeDir(outDir);
meta = mustOpen(tpath,"w");
lineFileSetMetaDataOutput(lf, meta);
while ((net = chainNetRead(lf)) != NULL)
    {
    safef(fileName, sizeof(fileName), "%s/%s.net", outDir, net->name);
    if (metaOpen)
        fclose(meta);
    metaOpen = FALSE;
    safef(cmd,sizeof(cmd), "cat %s | sort -u > %s", tpath, fileName);
    mustSystem(cmd);
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
