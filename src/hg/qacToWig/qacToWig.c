/* qacToWig - convert from compressed quality score format to wig ASCII. */
#include "common.h"
#include "linefile.h"
#include "qaSeq.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: qacToWig.c,v 1.2 2004/02/07 02:02:46 angie Exp $";

static char *name = NULL;

static struct optionSpec options[] = {
        {"name", OPTION_STRING},
        {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
"qacToWig - convert from compressed quality score format to wiggle format\n"
"quality score format.\n"
"usage:\n"
"   qacToWig in.qac outFileOrDir\n"
   "\t-name=name  restrict output to just this sequence name\n"
"   If -name is not used, outFileOrDir is a directory which will be created\n"
"   if it does not already exist.  If -name is used, outFileOrDir is a file\n"
"   (or \"stdout\").\n"
    );

}

void wigWrite(char *fileName, struct qaSeq *qa)
/* write a qa entry in wig format to fileNmae */
{
int i;
FILE *out = mustOpen(fileName, "wb");
for (i=0; i < qa->size; i++)
    {
    fprintf(out, "%d %d\n", i+1, qa->qa[i]);
    }
carefulClose(&out);
}

void qacToWig(char *inName, char *outDir)
/* qacToWig - convert from compressed to wiggle format files in out dir */
{
boolean isSwapped;
FILE *in = qacOpenVerify(inName, &isSwapped);
FILE *out;
struct qaSeq *qa;
char outPath[1024];
int i;
int outFileCount = 0;

if (name == NULL)
    makeDir(outDir);
while ((qa = qacReadNext(in, isSwapped)) != NULL)
    {
    if (name != NULL && sameString(qa->name, name))
	{
	wigWrite(outDir, qa);
	qaSeqFree(&qa);
	outFileCount++;
	break;
	}
    else if (name == NULL)
	{
	safef(outPath, sizeof outPath, "%s/%s.wig", outDir, qa->name);
	wigWrite(outPath, qa);
	qaSeqFree(&qa);
	outFileCount++;
	}
    }
carefulClose(&in);
if (name == NULL)
    printf("Made %d .wig files in %s\n", outFileCount, outDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);   
if (argc < 2)
    usage();
name = optionVal("name", NULL);
qacToWig(argv[1], argv[2]);
return 0;
}
