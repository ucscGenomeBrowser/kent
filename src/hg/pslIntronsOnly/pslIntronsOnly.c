/* pslIntronsOnly - Filter psl files to only include those with introns. */
#include "common.h"
#include "linefile.h"
#include "psl.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslIntronsOnly - Filter psl files to only include those with introns\n"
  "usage:\n"
  "   pslIntronsOnly in.psl in.nib|in.fa out.psl\n");
}

void nibPslIntronsOnly(char *inPslName, char *inNibName, char *outPslName)
/* Filter psl files to only include those with introns, with sequence from
 * nib */
{
struct dnaSeq *nibSeq = NULL;
int nibStart = 0, nibEnd = 0, nibSize = 0;
int nibSegTargetSize = 32*1024*1024;
int nibSegSize = 0, pslSegSize = 0;
struct lineFile *lf = NULL;
FILE *nibFile = NULL, *outFile = NULL;
struct psl *psl;
int count = 0, intronCount = 0;

lf = pslFileOpen(inPslName);
nibOpenVerify(inNibName, &nibFile, &nibSize);
outFile = mustOpen(outPslName, "w");
while ((psl = pslNext(lf)) != NULL)
    {
    ++count;
    pslSegSize = psl->tEnd - psl->tStart;
    if (rangeIntersection(psl->tStart, psl->tEnd, nibStart, nibEnd) < pslSegSize)
        {
	freeDnaSeq(&nibSeq);
	nibStart = psl->tStart;
	nibEnd = nibStart + nibSegTargetSize;
	if (psl->tEnd > nibEnd) nibEnd = psl->tEnd;
	if (nibEnd > nibSize) nibEnd = nibSize;
	uglyf("Processing %s:%d-%d\n", inNibName, nibStart, nibEnd);
	nibSeq = nibLdPart(inNibName, nibFile, nibSize, nibStart, nibEnd - nibStart);
	}
    if (pslHasIntron(psl, nibSeq, nibStart))
        {
	++intronCount;
	pslTabOut(psl, outFile);
	}
    pslFree(&psl);
    }
carefulClose(&outFile);
carefulClose(&nibFile);
lineFileClose(&lf);
printf("%d of %d in %s have introns\n", intronCount, count, inPslName);
}

struct dnaSeq *findFaDna(char *id, struct dnaSeq *allSeqs, char *inFaName)
/* find the specified sequence id in a list of sequences from a fasta file */
{
struct dnaSeq *seq;
for (seq = allSeqs; seq != NULL; seq = seq->next)
    if (sameString(seq->name, id))
        return seq;
errAbort("sequence %s not found in %s", id, inFaName);
return NULL;
}

void faPslIntronsOnly(char *inPslName, char *inFaName, char *outPslName)
/* Filter psl files to only include those with introns, with sequence from
 * a fasta file */
{
struct dnaSeq *allSeqs = faReadAllDna(inFaName);
struct dnaSeq *seq = NULL;
struct lineFile *lf = NULL;
FILE *outFile = NULL;
struct psl *psl;
int count = 0, intronCount = 0;

lf = pslFileOpen(inPslName);

outFile = mustOpen(outPslName, "w");
while ((psl = pslNext(lf)) != NULL)
    {
    ++count;
    seq = findFaDna(psl->tName, allSeqs, inFaName);

    if (pslHasIntron(psl, seq, 0))
        {
	++intronCount;
	pslTabOut(psl, outFile);
	}
    pslFree(&psl);
    }
carefulClose(&outFile);
freeDnaSeqList(&allSeqs);
lineFileClose(&lf);
printf("%d of %d in %s have introns\n", intronCount, count, inPslName);
}

void pslIntronsOnly(char *inPslName, char *inName, char *outPslName)
/* pslIntronsOnly - Filter psl files to only include those with introns. */
{
if (isNib(inName))
    nibPslIntronsOnly(inPslName, inName, outPslName);
else
    faPslIntronsOnly(inPslName, inName, outPslName);
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
