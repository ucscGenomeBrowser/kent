/* pslxToFa - convert pslx to fasta file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslxToFa - convert pslx to fasta file\n"
  "usage:\n"
  "   pslxToFa XXX\n"
  "options:\n"
  "   -liftOut=liftFile.lft\n"
  );
}

static struct optionSpec options[] = {
    {"liftFile", OPTION_STRING},
   {NULL, 0},
};

void pslxToFa(char *pslName, char *faName, char *liftName)
/* pslxToFa - convert pslx to fasta file. */
{
FILE *lift = NULL;
struct lineFile *in = pslFileOpen(pslName);
FILE *out = mustOpen(faName, "w");
struct psl *psl;

if (liftName != NULL)
    lift = mustOpen(liftName, "w");

while ((psl = pslNext(in)) != NULL)
    {
    int ii=0;
    int sum = 0;
    if (lift != NULL)
	{
	fprintf(lift,"%d\t%s/%s_%d_%d\t%d\t%s\t%d\n",
		0, "1", psl->qName,0,psl->blockCount, strlen(psl->qSequence[0]), psl->qName, psl->qSize);
	sum += strlen(psl->qSequence[0]);
	}
    fprintf(out,">%s_%d_%d\n%s\n",psl->qName, 0, psl->blockCount, psl->qSequence[0]);

    for(ii=1; ii < psl->blockCount; ii++)
	{
	if (lift != NULL)
	    {
	    fprintf(lift,"%d\t%s/%s_%d_%d\t%d\t%s\t%d\n",
		sum, "1", psl->qName,ii,psl->blockCount, strlen(psl->qSequence[ii]), psl->qName, psl->qSize);
	    sum += strlen(psl->qSequence[ii]);
	    }
	fprintf(out,">%s_%d_%d\n%s\n",psl->qName, ii, psl->blockCount,  psl->qSequence[ii]);
	}
    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *liftFile;

optionInit(&argc, argv, options);
liftFile = optionVal("liftFile", NULL);
if (argc != 3)
    usage();
pslxToFa(argv[1], argv[2], liftFile);
return 0;
}
