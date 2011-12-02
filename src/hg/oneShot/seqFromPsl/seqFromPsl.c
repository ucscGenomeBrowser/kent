/* seqFromPsl - Extract masked sequence from database corresponding to psl file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "twoBit.h"


boolean hardMask = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "seqFromPsl - Extract masked sequence from database corresponding to psl file\n"
  "usage:\n"
  "   seqFromPsl in.psl in.2bit out.fa\n"
  "options:\n"
  "   -hardMask\n"
  );
}

static struct optionSpec options[] = {
   {"hardMask", OPTION_BOOLEAN},
   {NULL, 0},
};

void seqFromPsl(char *inPsl, char *inTwoBit, char *outFa)
/* seqFromPsl - Extract masked sequence from database corresponding to psl file. */
{
struct twoBitFile *tbf = twoBitOpen(inTwoBit);
struct lineFile *lf = pslFileOpen(inPsl);
FILE *f = mustOpen(outFa, "w");
struct psl *psl;

while ((psl = pslNext(lf)) != NULL)
    {
    char faHead[512];
    struct dnaSeq *seq = twoBitReadSeqFrag(tbf, psl->tName,
    	psl->tStart, psl->tEnd);
    if (psl->strand[0] == '-')
        reverseComplement(seq->dna, seq->size);
    safef(faHead, sizeof(faHead), "%s (%s:%d-%d)", 
    	psl->qName, psl->tName, psl->tStart+1, psl->tEnd);
    if (hardMask)
        lowerToN(seq->dna, seq->size);
    faWriteNext(f, faHead, seq->dna, seq->size);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hardMask = optionExists("hardMask");
seqFromPsl(argv[1], argv[2], argv[3]);
return 0;
}
