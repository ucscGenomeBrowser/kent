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
  "pslxToFa - convert pslx (with sequence) to fasta file\n"
  "usage:\n"
  "   pslxToFa in.psl out.fa\n"
  "options:\n"
  "   -liftTarget=liftTarget.lft\n"
  "   -liftQuery=liftQuery.lft\n"
  );
}

static struct optionSpec options[] = {
    {"liftTarget", OPTION_STRING},
    {"liftQuery", OPTION_STRING},
   {NULL, 0},
};

void pslxToFa(char *pslName, char *faName, char *liftTargetName, char *liftQueryName)
/* pslxToFa - convert pslx to fasta file. */
{
FILE *liftTarget = NULL;
FILE *liftQuery = NULL;
struct lineFile *in = pslFileOpen(pslName);
FILE *out = mustOpen(faName, "w");
struct psl *psl;

if (liftQueryName != NULL)
    liftQuery = mustOpen(liftQueryName, "w");

if (liftTargetName != NULL)
    liftTarget = mustOpen(liftTargetName, "w");

while ((psl = pslNext(in)) != NULL)
    {
    int ii=0;
    int sumQuery = 0;
    if (liftQuery != NULL)
	{
	fprintf(liftQuery,"%d\t%s/%s_%d_%d\t%d\t%s\t%d\n",
		0, "1", psl->qName,0,psl->blockCount, strlen(psl->qSequence[0]), psl->qName, psl->qSize);
	sumQuery += strlen(psl->qSequence[0]);
	}
    if (liftTarget != NULL)
	{
	fprintf(liftTarget,"%d\t%s/%s_%d_%d\t%d\t%s\t%d\n",
		psl->tStarts[0], &psl->tName[3], psl->qName,0,psl->blockCount, strlen(psl->qSequence[0]), psl->tName, psl->tSize);
	}
    fprintf(out,">%s_%d_%d\n%s\n",psl->qName, 0, psl->blockCount, psl->qSequence[0]);

    for(ii=1; ii < psl->blockCount; ii++)
	{
	if (liftTarget != NULL)
	    {
	    fprintf(liftTarget,"%d\t%s/%s_%d_%d\t%d\t%s\t%d\n",
		psl->tStarts[ii], &psl->tName[3], psl->qName,ii,psl->blockCount, strlen(psl->qSequence[ii]), psl->tName, psl->tSize);
	    }
	if (liftQuery != NULL)
	    {
	    fprintf(liftQuery,"%d\t%s/%s_%d_%d\t%d\t%s\t%d\n",
		sumQuery, "1", psl->qName,ii,psl->blockCount, strlen(psl->qSequence[ii]), psl->qName, psl->qSize);
	    sumQuery += strlen(psl->qSequence[ii]);
	    }
	fprintf(out,">%s_%d_%d\n%s\n",psl->qName, ii, psl->blockCount,  psl->qSequence[ii]);
	}
    pslFree(&psl);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *liftTarget, *liftQuery;

optionInit(&argc, argv, options);
liftTarget = optionVal("liftTarget", NULL);
liftQuery = optionVal("liftQuery", NULL);
if (argc != 3)
    usage();
pslxToFa(argv[1], argv[2], liftTarget, liftQuery);
return 0;
}
