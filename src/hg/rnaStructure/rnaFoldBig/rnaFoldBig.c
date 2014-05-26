/* rnaFoldBig - Run RNAfold repeatedly. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "portable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "rnaFoldBig - Run RNAfold repeatedly\n"
  "usage:\n"
  "   rnaFoldBig in.fa foldDir \n"
  "The output goes into foldDir\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void rnaFoldBig(char *inFa, char *outDir)
/* rnaFoldBig - Run RNAfold repeatedly. */
{
char outFile[PATH_LEN];
char command[2*PATH_LEN];
struct lineFile *lf = lineFileOpen(inFa, TRUE);
struct dnaSeq seq;
FILE *f;

makeDir(outDir);
while (faSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name))
    {
    safef(outFile, sizeof(outFile), "%s/%s", outDir, seq.name);
    safef(command, sizeof(command), "RNAfold > %s", outFile);
    f = popen(command, "w");
    if (f == NULL)
        errnoAbort("Couldn't %s", command);
    mustWrite(f, seq.dna, seq.size);
    if (pclose(f) == -1)
        errnoAbort("Couldn't close pipe to %s", command);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
rnaFoldBig(argv[1], argv[2]);
return 0;
}
