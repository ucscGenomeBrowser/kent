/* qaToQa - convert from compressed to uncompressed 
 * quality score format. */
#include "common.h"
#include "linefile.h"
#include "qaSeq.h"
#include "options.h"
#include "portable.h"

static char const rcsid[] = "$Id: qacToWig.c,v 1.1 2004/02/01 01:53:45 kate Exp $";

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
"   qacToWig in.qac outfile | outdir\n"
   "\t-name=name  restrict output to just this sequence name\n"
    );

}

void wigWrite(char *fileName, struct qaSeq *qa)
/* write a qa entry in wig format to fileNmae */
{
int i;

FILE *out = mustOpen(fileName, "wb");
for (i=0; i < qa->size; i++)
    {
    fprintf(out, "%d %d\n", i, qa->qa[i]);
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
    if (name != NULL)
        if (sameString(qa->name, name))
            {
            wigWrite(outDir, qa);
            qaSeqFree(&qa);
            outFileCount++;
            break;
            }
    safef(outPath, sizeof outPath, "%s/%s.wig", outDir, qa->name);
    wigWrite(outPath, qa);
    qaSeqFree(&qa);
    outFileCount++;
    }
carefulClose(&in);
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
