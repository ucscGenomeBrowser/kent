/* twoBitToFa - Convert all or part of twoBit file to fasta. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "twoBit.h"

static char const rcsid[] = "$Id: twoBitToFa.c,v 1.10 2006/07/13 00:50:35 baertsch Exp $";

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
  "                    to output of form the seqSpec[:start-end]\n"
  "   -noMask - convert sequence to all upper case\n"
  "\n"
  "Sequence and range may also be specified as part of the input\n"
  "file name using the syntax:\n"
  "      /path/input.2bit:name\n"
  "   or\n"
  "      /path/input.2bit:name\n"
  "   or\n"
  "      /path/input.2bit:name:start-end\n"
  );
}

char *clSeq = NULL;	/* Command line sequence. */
int clStart = 0;	/* Start from command line. */
int clEnd = 0;		/* End from command line. */
char *clSeqList = NULL; /* file containing list of seq names */
bool noMask = FALSE;  /* convert seq to upper case */

static struct optionSpec options[] = {
   {"seq", OPTION_STRING},
   {"seqList", OPTION_STRING},
   {"start", OPTION_INT},
   {"end", OPTION_INT},
   {"noMask", OPTION_BOOLEAN},
   {NULL, 0},
};

void outputOne(struct twoBitFile *tbf, char *seqSpec, FILE *f, 
	int start, int end)
/* Output sequence. */
{
struct dnaSeq *seq = twoBitReadSeqFrag(tbf, seqSpec, start, end);
if (noMask)
    toUpperN(seq->dna, seq->size);
faWriteNext(f, seq->name, seq->dna, seq->size);
dnaSeqFree(&seq);
}

static void processAllSeqs(struct twoBitFile *tbf, FILE *outFile)
/* get all sequences in a file */
{
struct twoBitIndex *index;
for (index = tbf->indexList; index != NULL; index = index->next)
    outputOne(tbf, index->name, outFile, 0, 0);
}

static void processSeqSpecs(struct twoBitFile *tbf, struct twoBitSeqSpec *tbss,
                            FILE *outFile)
/* process list of twoBitSeqSpec objects */
{
struct twoBitSeqSpec *s;
for (s = tbss; s != NULL; s = s->next)
    outputOne(tbf, s->name, outFile, s->start, s->end);
}

void twoBitToFa(char *inName, char *outName)
/* twoBitToFa - Convert all or part of twoBit file to fasta. */
{
struct twoBitFile *tbf;
FILE *outFile = mustOpen(outName, "w");
struct twoBitSpec *tbs;

if (clSeq != NULL)
    {
    char seqSpec[2*PATH_LEN];
    if (clEnd > clStart)
        safef(seqSpec, sizeof(seqSpec), "%s:%s:%d-%d", inName, clSeq, clStart, clEnd);
    else
        safef(seqSpec, sizeof(seqSpec), "%s:%s", inName, clSeq);
    tbs = twoBitSpecNew(seqSpec);
    }
else if (clSeqList != NULL)
    tbs = twoBitSpecNewFile(inName, clSeqList);
else
    tbs = twoBitSpecNew(inName);
tbf = twoBitOpen(tbs->fileName);
if (tbs->seqs == NULL)
    processAllSeqs(tbf, outFile);
else
    processSeqSpecs(tbf, tbs->seqs, outFile);
twoBitSpecFree(&tbs);
carefulClose(&outFile);
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
clSeqList = optionVal("seqList", clSeqList);
if ((clStart > clEnd) && (clSeq == NULL))
    errAbort("must specify -seq with -start and -end");
if ((clSeq != NULL) && (clSeqList != NULL))
    errAbort("can't specify both -seq and -seqList");
noMask = optionExists("noMask");
dnaUtilOpen();
twoBitToFa(argv[1], argv[2]);
return 0;
}
