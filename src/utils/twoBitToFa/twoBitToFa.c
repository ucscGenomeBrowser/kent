/* twoBitToFa - Convert all or part of twoBit file to fasta. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: twoBitToFa.c,v 1.3 2004/02/23 06:49:10 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "twoBitToFa - Convert all or part of .2bit file to fasta\n"
  "usage:\n"
  "   twoBitToFa input.2bit output.fa\n"
  "options:\n"
  "   -seq=name - restrict this to just one sequence\n"
  "   -start=X  - start at given position in sequence (zero-based)\n"
  "   -end=X - end at given position in sequence (non-inclusive)\n"
  );
}

char *clSeq = NULL;	/* Command line sequence. */
int clStart = 0;	/* Start from command line. */
int clEnd = 0;		/* End from command line. */

static struct optionSpec options[] = {
   {"seq", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {NULL, 0},
};

void outputOne(struct twoBitHeader *tbh, char *seqName, FILE *f, 
	int start, int end)
/* Output sequence. */
{
struct dnaSeq *seq = twoBitReadSeqFrag(tbh, seqName, start, end);
faWriteNext(f, seq->name, seq->dna, seq->size);
dnaSeqFree(&seq);
}

void twoBitToFa(char *inName, char *outName)
/* twoBitToFa - Convert all or part of twoBit file to fasta. */
{
FILE *inFile = mustOpen(inName, "rb");
FILE *outFile = mustOpen(outName, "w");
struct twoBitHeader *tbh = twoBitHeaderRead(inName, inFile);
struct twoBitIndex *index;

if (clSeq == NULL)
    {
    for (index = tbh->indexList; index != NULL; index = index->next)
        {
	outputOne(tbh, index->name, outFile, clStart, clEnd);
	}
    }
else
    {
    outputOne(tbh, clSeq, outFile, clStart, clEnd);
    }
twoBitHeaderFree(&tbh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
clSeq = optionVal("seq", clSeq);
clStart = optionInt("start", clStart);
clEnd = optionInt("end", clEnd);
dnaUtilOpen();
twoBitToFa(argv[1], argv[2]);
return 0;
}
