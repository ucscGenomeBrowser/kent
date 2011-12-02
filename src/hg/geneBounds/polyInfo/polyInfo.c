/* polyInfo - Collect info on polyAdenylation signals etc. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"
#include "fa.h"
#include "nib.h"
#include "twoBit.h"
#include "estOrientInfo.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "polyInfo - Collect info on polyAdenylation signals etc\n"
  "usage:\n"
  "   polyInfo contig.psl genoFile est.fa output.tab\n"
  "\n"
  "The genoFile can be a fasta file or a nib.\n"
  "Subranges of nib files may specified using the syntax:\n"
  "   /path/file.nib:seqid:start-end\n"
  "or\n"
  "   /path/file.2bit:seqid:start-end\n"
  "or\n"
  "   /path/file.nib:start-end\n"
  "With the second form, a sequence id of file:start-end will be used.\n"
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
    if (psl->strand[1] == '-')
        reverseIntRange(&s, &e, psl->tSize);
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
int pos;
int end = est->size;
if (polyASize > 0)
    end -= polyASize;
pos = findOneSignalPos(est, signal, 6, end - 26, end);
if (pos >= 0)
    return est->size - pos;
return 0;
}

void fillInEstInfo(struct estOrientInfo *ei, struct dnaSeq *est, struct dnaSeq *geno,
	struct psl *psl)
/* Fill in estOrientInfo struct by examining alignment and est and genomic
 * sequences.  Corrects EST according to genome and may reverse complement's 
 * est as a side effect. */
{
static char *signals[] = {"aataaa", "attaaa"};
int unAliSize;
struct dnaSeq* revEst;
ei->chrom = psl->tName;
ei->chromStart = psl->tStart;
ei->chromEnd = psl->tEnd;
ei->name = psl->qName;
ei->intronOrientation = findIntronOrientation(psl, geno);
ei->sizePolyA = countCharAtEnd('a', est->dna, est->size);
unAliSize = psl->qSize - psl->qEnd;
if (ei->sizePolyA > unAliSize)
    ei->sizePolyA = unAliSize;
unAliSize = psl->qStart;
ei->revSizePolyA = countCharAtStart('t', est->dna, est->size);
if (ei->revSizePolyA > unAliSize)
    ei->revSizePolyA = unAliSize;
correctEst(psl, est, geno);

/* Find poly A signal - first looking for most common signal, then
 * for less common signal on both strands. */
revEst = cloneDnaSeq(est);
reverseComplement(revEst->dna, est->size);
ei->signalPos = findSignalPos(psl, est, ei->sizePolyA, signals[0]);
ei->revSignalPos = findSignalPos(psl, revEst, ei->revSizePolyA, signals[0]);
if (ei->signalPos == 0 && ei->revSignalPos == 0)
    {
    ei->signalPos = findSignalPos(psl, est, ei->sizePolyA, signals[1]);
    ei->revSignalPos = findSignalPos(psl, revEst, ei->revSizePolyA, signals[1]);
    }
dnaSeqFree(&revEst);
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

void polyInfo(char *pslFile, char *genoFile, char *estFile, char *outputFile)
/* polyInfo - Collect info on polyAdenylation signals etc. */
{
struct hash *pslHash = NULL;
struct hash *genoHash = loadGeno(genoFile);
static struct dnaSeq est;
struct lineFile *lf = NULL;
FILE *f = NULL;

pslHash = pslIntoHash(pslFile);
lf = lineFileOpen(estFile, TRUE);
f = mustOpen(outputFile, "w");

while (faSpeedReadNext(lf, &est.dna, &est.size, &est.name))
    {
    struct pslList *pl;
    struct psl *psl;
    struct estOrientInfo ei;
    if ((pl = hashFindVal(pslHash, est.name)) != NULL)
        {
	for (psl = pl->list; psl != NULL; psl = psl->next)
	    {
            struct dnaSeq *geno = hashMustFindVal(genoHash, psl->tName);
	    if (psl->tSize != geno->size)
	        errAbort("psl generated on a different version of the genome");
	    ZeroVar(&ei);
	    fillInEstInfo(&ei, &est, geno, psl);
	    estOrientInfoTabOut(&ei, f);
	    }
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage();
polyInfo(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
