/* pslIntronsOnly - Filter psl files to only include those with introns. */
#include "common.h"
#include "linefile.h"
#include "psl.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "twoBit.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslIntronsOnly - Filter psl files to only include those with introns\n"
  "usage:\n"
  "   pslIntronsOnly in.psl genoFile out.psl\n"
  "\n"
  "The genoFile can be a fasta file, a chromosome nib, or a nib subrange.\n"
  "Subranges of nib files may specified using the syntax:\n"
  "   /path/file.nib:seqid:start-end\n"
  "or\n"
  "   /path/file.2bit:seqid:start-end\n"
  "or\n"
  "   /path/file.nib:start-end\n"
  "With the second form, a sequence id of file:start-end will be used.\n");
}

struct hash *loadGeno(char *genoFile)
/* load genome sequences into a hash.  This supports the multi-sequence
 * specs of twoBitLoadAll */
{
struct dnaSeq *genos = NULL, *geno;
struct hash *genoHash = hashNew(0);

if (nibIsFile(genoFile))
    genos = nibLoadAllMasked(NIB_MASK_MIXED|NIB_BASE_NAME, genoFile);
else if (twoBitIsSpec(genoFile))
    genos = twoBitLoadAll(genoFile);
else
    genos = faReadDna(genoFile);

while ((geno = slPopHead(&genos)) != NULL)
    {
    tolowers(geno->dna);
    hashAdd(genoHash, geno->name, geno);
    }
return genoHash;
}

void pslIntronsOnly(char *inPslName, char *genoFile, char *outPslName)
/* pslIntronsOnly - Filter psl files to only include those with introns. */
{
struct lineFile *lf = NULL;
FILE *outFile = NULL;
struct hash *genoHash = loadGeno(genoFile);
struct psl *psl;
int count = 0, intronCount = 0;

lf = pslFileOpen(inPslName);
outFile = mustOpen(outPslName, "w");
while ((psl = pslNext(lf)) != NULL)
    {
    struct dnaSeq *geno = hashMustFindVal(genoHash, psl->tName);
    if (pslHasIntron(psl, geno, 0))
        {
	++intronCount;
	pslTabOut(psl, outFile);
	}
    pslFree(&psl);
    ++count;
    }
carefulClose(&outFile);
lineFileClose(&lf);
printf("%d of %d in %s have introns\n", intronCount, count, inPslName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
dnaUtilOpen();
pslIntronsOnly(argv[1], argv[2], argv[3]);
return 0;
}
