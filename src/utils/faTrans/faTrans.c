/* faTrans - Translate DNA .fa file to peptide. */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "cheapcgi.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "faTrans - Translate DNA .fa file to peptide\n"
  "usage:\n"
  "   faTrans in.fa out.fa\n"
  "options:\n"
  "   -stop stop at first stop codon (otherwise puts in Z for stop codons)"
  "   -offset=N start at a particular offset.");
}

void faTrans(char *inFile, char *outFile, boolean stop, int offset)
/* faTrans - Translate DNA .fa file to peptide. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct dnaSeq dna, *pep;

while (faSpeedReadNext(lf, &dna.dna, &dna.size, &dna.name))
    {
    pep = translateSeq(&dna, offset, stop);
    faWriteNext(f, pep->name, pep->dna, pep->size);
    freeDnaSeq(&pep);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
faTrans(argv[1], argv[2], cgiVarExists("stop"), cgiOptionalInt("offset", 0));
return 0;
}
