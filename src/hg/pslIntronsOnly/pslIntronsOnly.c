/* pslIntronsOnly - Filter psl files to only include those with introns. */
#include "common.h"
#include "linefile.h"
#include "psl.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nib.h"
#include "fa.h"

static char const rcsid[] = "$Id: pslIntronsOnly.c,v 1.6 2003/05/06 07:22:34 kate Exp $";

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
  "   /path/file.nib:start-end\n"
  "With the second form, a sequence id of file:start-end will be used.\n");
}

/* cache of current sequence */
struct dnaSeq *genoSeqCache = NULL;

/* input file */
char genoFileName[PATH_LEN];

/* data for seqments or subrange nibs */
boolean nibSubrange = FALSE;
FILE *nibFile = NULL;
int nibStart = 0, nibEnd = 0, nibSize = 0;
int nibSegTargetSize = 32*1024*1024;

/* data for fasta sequences */
struct dnaSeq *faAllSeqs = NULL;

struct dnaSeq* chromNibGet(char *tName, int tStart, int tEnd, int *tOffsetPtr)
/* Get the DNA sequence from a chromosome nib */
{
int pslSegSize = tEnd - tStart;
if (rangeIntersection(tStart, tEnd, nibStart, nibEnd) < pslSegSize)
    {
    freeDnaSeq(&genoSeqCache);
    nibStart = tStart;
    nibEnd = nibStart + nibSegTargetSize;
    if (tEnd > nibEnd)
        nibEnd = tEnd;
    if (nibEnd > nibSize)
        nibEnd = nibSize;
    genoSeqCache = nibLdPart(genoFileName, nibFile, nibSize, nibStart,
                             nibEnd - nibStart);
    tolowers(genoSeqCache->dna);
    }
*tOffsetPtr = nibStart;
return genoSeqCache;
}

struct dnaSeq* subNibGet(char *tName, int tStart, int tEnd, int *tOffsetPtr)
/* Get the DNA sequence from a subrange nib */
{
if (!sameString(tName, genoSeqCache->name))
    errAbort("sequence name %s does not match nib subrange name %s from %s",
             tName, genoSeqCache->name, genoFileName);
*tOffsetPtr = 0;
return genoSeqCache;
}

struct dnaSeq* faGet(char *tName, int tStart, int tEnd, int *tOffsetPtr)
/* Get the DNA sequence from a fasta file */
{
if ((genoSeqCache == NULL) || !sameString(tName, genoSeqCache->name))
    {
    for (genoSeqCache = faAllSeqs; genoSeqCache != NULL;
         genoSeqCache = genoSeqCache->next)
        {
        if (sameString(tName, genoSeqCache->name))
            break;
        }
    if (genoSeqCache == NULL)
        errAbort("sequence %s not found in %s", tName, genoFileName);
    tolowers(genoSeqCache->dna);
    }
*tOffsetPtr = 0;
return genoSeqCache;
}

void initGenoRead(char *inGenoFile)
/* initialize for reading the genome sequence */
{
strcpy(genoFileName, inGenoFile);
if (isNibSubrange(inGenoFile))
    {
    nibSubrange = TRUE;
    genoSeqCache = nibLoadAllMasked(NIB_MASK_MIXED|NIB_BASE_NAME,
                                    genoFileName);
    tolowers(genoSeqCache->dna);
    }
else if (isNib(inGenoFile))
    {
    nibOpenVerify(genoFileName, &nibFile, &nibSize);
    }
else
    {
    faAllSeqs = faReadAllDna(genoFileName);
    }
}

struct dnaSeq *getDnaSeq(char *tName, int tStart, int tEnd, int *tOffsetPtr)
/* get the DNA sequence containing the range. */
{
struct dnaSeq *genoSeq;
if (nibSubrange)
    genoSeq = subNibGet(tName, tStart, tEnd, tOffsetPtr);
else if (nibFile != NULL)
    genoSeq = chromNibGet(tName, tStart, tEnd, tOffsetPtr);
else
    genoSeq = faGet(tName, tStart, tEnd, tOffsetPtr);
if (tEnd > (genoSeq->size+*tOffsetPtr))
    errAbort("sequence %s in PSL is longer than sequence in %s", tName,
             genoFileName);
return genoSeq;
}

void pslIntronsOnly(char *inPslName, char *inName, char *outPslName)
/* pslIntronsOnly - Filter psl files to only include those with introns. */
{
struct lineFile *lf = NULL;
FILE *outFile = NULL;
int genoSeqOff = 0;
struct psl *psl;
int count = 0, intronCount = 0;

initGenoRead(inName);

lf = pslFileOpen(inPslName);
outFile = mustOpen(outPslName, "w");
while ((psl = pslNext(lf)) != NULL)
    {
    struct dnaSeq* genoSeq = getDnaSeq(psl->tName, psl->tStart, psl->tEnd,
                                       &genoSeqOff);
    if (pslHasIntron(psl, genoSeq, genoSeqOff))
        {
	++intronCount;
	pslTabOut(psl, outFile);
	}
    pslFree(&psl);
    ++count;
    }
carefulClose(&outFile);
carefulClose(&nibFile);
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
