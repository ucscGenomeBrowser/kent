/* twoBitToFa - Convert all or part of twoBit file to fasta. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: twoBitToFa.c,v 1.7 2005/03/24 22:51:26 baertsch Exp $";

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
  "   -seqList=file - file containing list of sequence names \n"
  "                    to output of form the seqSpec:[start-end]\n"
  "\n"
  "Sequence and range may also be specified as part of the input\n"
  "file name using the syntax:\n"
  "      /path/input.2bit:name\n"
  "   or\n"
  "      /path/input.2bit:name:start-end\n"
  );
}

char *clSeq = NULL;	/* Command line sequence. */
int clStart = 0;	/* Start from command line. */
int clEnd = 0;		/* End from command line. */
char *seqList = NULL;    /* file containing list of seq names */

static struct optionSpec options[] = {
   {"seq", OPTION_STRING},
   {"seqList", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {NULL, 0},
};

void outputOne(struct twoBitFile *tbf, char *seqSpec, FILE *f, 
	int start, int end)
/* Output sequence. */
{
struct dnaSeq *seq = twoBitReadSeqFrag(tbf, seqSpec, start, end);
faWriteNext(f, seq->name, seq->dna, seq->size);
dnaSeqFree(&seq);
}

void twoBitToFa(char *inName, char *outName)
/* twoBitToFa - Convert all or part of twoBit file to fasta. */
{
FILE *outFile = mustOpen(outName, "w");
struct twoBitFile *tbf;
struct twoBitIndex *index;
char *line = NULL;
int lineSize = 0;
char seqSpec[PATH_LEN];
    
if (seqList != NULL)
    {
    struct lineFile *seqFile = lineFileOpen(seqList, TRUE);
    tbf = twoBitOpen(inName);
    while (lineFileNextReal(seqFile, &line))
        {
        line = trimSpaces(line);
        safef(seqSpec, sizeof(seqSpec),"%s:%s",inName,line);
        twoBitParseRange(seqSpec, NULL, &clSeq, &clStart, &clEnd);
	outputOne(tbf, clSeq, outFile, clStart, clEnd);
        }
    }
else
/* check for sequence/range in path */
    {
    if (twoBitIsRange(inName))
        twoBitParseRange(inName, &inName, &clSeq, &clStart, &clEnd);
        tbf = twoBitOpen(inName);

    if (clSeq == NULL)
        {
        for (index = tbf->indexList; index != NULL; index = index->next)
            {
            outputOne(tbf, index->name, outFile, clStart, clEnd);
            }
        }
    else
        {
        outputOne(tbf, clSeq, outFile, clStart, clEnd);
        }
    }
twoBitClose(&tbf);
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
seqList = optionVal("seqList", seqList);
dnaUtilOpen();
twoBitToFa(argv[1], argv[2]);
return 0;
}
