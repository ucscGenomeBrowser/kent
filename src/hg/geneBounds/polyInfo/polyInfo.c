/* polyInfo - Collect info on polyAdenylation signals etc. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "psl.h"
#include "fa.h"
#include "jksql.h"
#include "estInfo.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "polyInfo - Collect info on polyAdenylation signals etc\n"
  "usage:\n"
  "   polyInfo contig.psl contig.fa est.fa output.tab\n"
  );
}

struct pslList
/* A list of psl's */
    {
    struct pslList *next;	/* Next in this list. */
    struct psl *list;	/* List of psl's */
    };

struct hash *pslIntoHash(char *fileName)
/* Read psl into a hash of pslLists keyed by psl->qName. */
{
struct lineFile *lf = pslFileOpen(fileName);
struct hash *hash = newHash(18);
struct psl *psl;
struct pslList *pl, *plList = NULL;

while ((psl = pslNext(lf)) != NULL)
    {
    if ((pl = hashFindVal(hash, psl->qName)) == NULL)
	{
	AllocVar(pl);
	hashAdd(hash, psl->qName, pl);
	slAddHead(&plList, pl);
	}
    slAddHead(&pl->list, psl);
    }
lineFileClose(&lf);
return hash;
}

char *uglyBird = "AA001450";

int findIntronOrientation(struct psl *psl, struct dnaSeq *geno)
/* Return 0 if unspliced, otherwise number of forward strand splice
 * sites or minus number of negative strand splice sites. */
{
int sumSplice = 0;
DNA *dna = geno->dna;
int i, blockCount = psl->blockCount, s, e;
unsigned *sizes = psl->blockSizes, *starts = psl->tStarts;

for (i=1; i<blockCount; ++i)
    {
    s = starts[i-1] + sizes[i-1];
    e = starts[i];
    sumSplice += intronOrientation(dna+s,dna+e);
    }
if (psl->strand[0] == '-')
    sumSplice = -sumSplice;
return sumSplice;
}

int countCharAtEnd(char c, char *s, int size)
/* Count number of characters matching c at end of s. */
{
int i=0, count=0;
for (i=size; --i>=0; )
    {
    if (s[i] != c && s[i] != 'n')
        break;
    ++count;
    }
return count;
}

int countCharAtStart(char c, char *s, int size)
/* Count number of characters matching c at start of s. */
{
int i=0, count=0;
for (i=0; i<size; ++i)
    {
    if (s[i] != c && s[i] != 'n')
        break;
    ++count;
    }
return count;
}



void correctEst(struct psl *psl, struct dnaSeq *est, struct dnaSeq *geno)
/* Correct bases in EST to match genome where they align. */
{
int sumSplice = 0;
int i, blockCount = psl->blockCount;

if (psl->strand[0] == '-')
    reverseComplement(est->dna, est->size);
for (i=0; i<blockCount; ++i)
    {
    memcpy(est->dna + psl->qStarts[i], geno->dna + psl->tStarts[i], 
    	psl->blockSizes[i]);
    }
if (psl->strand[0] == '-')
    reverseComplement(est->dna, est->size);
}

int findOneSignalPos(struct dnaSeq *seq, char *signal, int sigSize,
	int seqStart, int seqEnd)
/* Look for signal between start and end. */
{
int i;
DNA *dna = seq->dna;

if (seqStart < 0)
    seqStart = 0;
for (i=seqEnd - sigSize; i>=seqStart; --i)
    {
    if (memcmp(signal, dna+i, sigSize) == 0)
	return i;
    }
return -1;
}

int findSignalPos(struct psl *psl, struct dnaSeq *est, int polyASize, char *signal)
/* Return position of start of polyadenlyation signal relative to end
 * of EST. */
{
int i, pos;
int end = est->size;
if (polyASize > 0)
    end -= polyASize;
pos = findOneSignalPos(est, signal, 6, end - 26, end);
if (pos >= 0)
    return est->size - pos;
return 0;
}

void fillInEstInfo(struct estInfo *ei, struct dnaSeq *est, struct dnaSeq *geno,
	struct psl *psl)
/* Fill in estInfo struct by examining alignment and est and genomic
 * sequences.  Corrects EST according to genome and may reverse complement's 
 * est as a side effect. */
{
static char *signals[] = {"aataaa", "attaaa"};
ei->chrom = psl->tName;
ei->chromStart = psl->tStart;
ei->chromEnd = psl->tEnd;
ei->name = psl->qName;
ei->intronOrientation = findIntronOrientation(psl, geno);
ei->sizePolyA = countCharAtEnd('a', est->dna, est->size);
ei->revSizePolyA = countCharAtStart('t', est->dna, est->size);
correctEst(psl, est, geno);

/* Find poly A signal - first looking for most common signal, then
 * for less common signal on both strands. */
ei->signalPos = findSignalPos(psl, est, ei->sizePolyA, signals[0]);
reverseComplement(est->dna, est->size);
ei->revSignalPos = findSignalPos(psl, est, ei->revSizePolyA, signals[0]);
if (ei->signalPos == 0 && ei->revSignalPos == 0)
    {
    ei->revSignalPos = findSignalPos(psl, est, ei->revSizePolyA, signals[1]);
    if (ei->revSignalPos == 0)
	{
	reverseComplement(est->dna, est->size);
	ei->signalPos = findSignalPos(psl, est, ei->sizePolyA, signals[1]);
	}
    }

}

void polyInfo(char *pslFile, char *genoFile, char *estFile, char *outputFile)
/* polyInfo - Collect info on polyAdenylation signals etc. */
{
struct hash *pslHash = NULL;
struct dnaSeq *geno = NULL;
static struct dnaSeq est;
struct lineFile *lf = NULL;
FILE *f = NULL;

pslHash = pslIntoHash(pslFile);
geno = faReadDna(genoFile);
lf = lineFileOpen(estFile, TRUE);
f = mustOpen(outputFile, "w");

while (faSpeedReadNext(lf, &est.dna, &est.size, &est.name))
    {
    struct pslList *pl;
    struct psl *psl;
    struct estInfo ei;
    if ((pl = hashFindVal(pslHash, est.name)) != NULL)
        {
	for (psl = pl->list; psl != NULL; psl = psl->next)
	    {
	    ZeroVar(&ei);
	    fillInEstInfo(&ei, &est, geno, psl);
	    estInfoTabOut(&ei, f);
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 5)
    usage();
polyInfo(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
