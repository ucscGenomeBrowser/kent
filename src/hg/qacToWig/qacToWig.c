/* qacToWig - convert from compressed quality score format to wig ASCII. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "qaSeq.h"
#include "options.h"
#include "portable.h"


static char *name = NULL;
static bool fixed = FALSE;

static struct optionSpec options[] = {
        {"name", OPTION_STRING},
        {"fixed", OPTION_BOOLEAN},
        {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
"qacToWig - convert from compressed quality score format to wiggle format.\n"
"usage:\n"
"   qacToWig in.qac outFileOrDir\n"
   "\t-name=name    restrict output to just this sequence name\n"
   "\t-fixed        output single file with wig headers and fixed step size\n"
"   If neither -name nor -fixed is used, outFileOrDir is a directory which\n"
"   will be created if it does not already exist.  If -name and/or -fixed is\n"
"   used, outFileOrDir is a file (or \"stdout\").\n"
    );

}

void wigWrite(char *fileName, struct qaSeq *qa)
/* write a qa entry in wig format to fileName */
{
int i;
FILE *out = mustOpen(fileName, "wb");
for (i=0; i < qa->size; i++)
    {
    fprintf(out, "%d %d\n", i+1, qa->qa[i]);
    }
carefulClose(&out);
}

void wigFixedWrite(FILE *out , struct qaSeq *qa)
/* write out a qa entry in fixed wig format */
{
int i;
fprintf(out, "fixedStep chrom=%s start=1 step=1\n", qa->name);
for (i=0; i < qa->size; i++)
    {
    fprintf(out, "%d\n", qa->qa[i]);
    }
}

void qacToWig(char *inName, char *outDir)
/* qacToWig - convert from compressed to wiggle format files in out dir */
{
boolean isSwapped;
FILE *in = qacOpenVerify(inName, &isSwapped);
FILE *out = NULL;
struct qaSeq *qa;
int outFileCount = 0;

if (fixed)
    out = mustOpen(outDir, "wb");
else if (name == NULL)
    makeDir(outDir);

while ((qa = qacReadNext(in, isSwapped)) != NULL)
    {
    if (name != NULL)
	{
	if (sameString(qa->name, name))
	    {
	    if (fixed)
		wigFixedWrite(out, qa);
	    else
		wigWrite(outDir, qa);
	    qaSeqFree(&qa);
	    outFileCount++;
	    break;
	    }
	}
    else if (fixed)
        {
        wigFixedWrite(out, qa);
	qaSeqFree(&qa);
        outFileCount = 1;
        continue;
        }
    else
	{
	char outPath[1024];
	safef(outPath, sizeof outPath, "%s/%s.wig", outDir, qa->name);
	wigWrite(outPath, qa);
	outFileCount++;
	}
    qaSeqFree(&qa);
    }
carefulClose(&in);
if (fixed)
    carefulClose(&out);
if (name == NULL)
    verbose(1, "Made %d .wig files in %s\n", outFileCount, outDir);
else if (outFileCount < 1)
    warn("Found no sequences with name \"%s\"\n", name);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);   
if (argc < 2)
    usage();
name = optionVal("name", NULL);
fixed = optionExists("fixed");
qacToWig(argv[1], argv[2]);
return 0;
}
