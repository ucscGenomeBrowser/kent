/* olly - Look for matches and near matches to short sequences genome-wide. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "localmem.h"
#include "dnautil.h"
#include "fa.h"
#include "nib.h"

int maxDiff = 2;
int ollySize = 25;
int ramMb = 490;
boolean easyOut = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "olly - Look for matches and near matches to short sequences genome-wide\n"
  "Output can be loaded as a wiggle track\n"
  "usage:\n"
  "   olly nibDir chrom start stop out.sample\n"
  "example:\n"
  "   olly ~/oo/mixedNib chr1 0 1000 chr1_0_1000.sample\n"
  "options:\n"
  "   -maxDiff=N (default %d) Maximum variation in bases.\n"
  "   -ollySize=N (default %d) Size of oligo.  Sizes > 26 will be slow\n"
  "   -ramMb=N (default %d) Size of RAM to use\n"
  "   -makeBatch=parasolSpec Make batch file for parasol\n"
  "       In this case just do \n"
  "         olly nibDir -makeBatch=spec.\n"
  "       Spec will be a parasol spec to do everything in nibDir\n" 
  "   -easyOut Output is in a simpler format\n"
  , maxDiff, ollySize, ramMb
  );
}

int oHashSize = 25;
int maxMem;		/* Maximum amount of memory to use. */
int bigChromSize = 240000000;

struct oHashEl
/* Oligomer hash element. */
  {
  struct oHashEl *next;   /* Next in list. */
  DNA *pt;                /* Pointer to start of oligomer. Not allocated here */
  };

struct oHash
/* Oligomer hash, keyed by first part of oligomre. */
    {
    struct oHashEl **table;   /* The hash table. */
    int mask;                 /* Mask to get hash function in range of hash table */
    struct lm *lm;            /* Local memory buffer for hash elements. */
    };

boolean closeEnough(DNA *a, DNA *b)
/* Return TRUE if a and b differ by maxDiff or less. */
{
int i;
int missCount = 0;
for (i=0; i<ollySize; ++i)
    {
    if (a[i] != b[i])
	{
	if (++missCount > maxDiff)
	    return FALSE;
	}
    }
return TRUE;
}

int oHashTableSize()
/* Return size of table. */
{
return (1<<oHashSize);
}

struct oHash *oHashNew()
/* Allocate oligomer hash */
{
struct oHash *hash;
int tableSize = oHashTableSize();
AllocVar(hash);
AllocArray(hash->table, tableSize);
hash->lm = lmInit(1024 * 256);
hash->mask = tableSize-1;
return hash;
}

int oHashFunc(DNA *dna)
/* DNA hash function. */
{
int val = 0;
int nt;
int i;
for (i=0;i<ollySize;++i)
    {
    val <<= 1;
    val += ntVal[dna[i]];
    }
return val;
}


struct oHashEl *oHashAdd(struct oHash *hash, DNA *key)
/* Add to hash */
{
struct oHashEl *el, **pList;
lmAllocVar(hash->lm, el);
el->pt = key;
pList = hash->table + (oHashFunc(key) & hash->mask);
slAddHead(pList, el);
return el;
}

DNA *oHashFindVal(struct oHash *hash, DNA *key)
/* Find value in hash */
{
struct oHashEl *el;
for (el = hash->table[oHashFunc(key)&hash->mask]; el != NULL; el = el->next)
    {
    if (closeEnough(el->pt, key))
	return el->pt;
    }
return NULL;
}

/* Parameters to recursive function below. */
static struct oHash *rHash;	/* Hash to put oligo and varients into */
static DNA *rOlly;		/* Oligomer sequence. */
static bool *rMask;		/* Mask of positions we've already varied. */
static int rCount;		/* Count of variations. */
static int rMemUsed;		/* Amount of memory used. */

void raddVarients(int maxMiss, int startIx)
/* Add all varients of olly that differ from olly by at most maxMiss.  Don't bother
 * varying at mask position. */
{
++rCount;
oHashAdd(rHash, rOlly);
rMemUsed += sizeof(struct oHashEl);
if (rMemUsed >= maxMem)
    errAbort("Out of memory.  Try using less sequence or a smaller maxDiff");
if (maxMiss > 0)
    {
    int i,j;
    DNA save;
    static char nt[4] = {'a', 'c', 'g', 't'};
    for (i=startIx; i<ollySize; ++i)
	{
	if (!rMask[i])
	    {
	    rMask[i] = TRUE;
	    save = rOlly[i];
	    for (j=0; j<4; ++j)
		{
		char c = nt[j];
		if (c != save)
		    {
		    rOlly[i] = c;
		    raddVarients(maxMiss-1, i+1);
		    }
		}
	    rMask[i] = FALSE;
	    rOlly[i] = save;
	    }
	}
    }
}


boolean goodSeq(DNA *dna, int size)
/* Return TRUE if all lower case and non-n. */
{
int i;
for (i=0; i<size; ++i)
    {
    if (ntValLower[dna[i]] < 0)
        return FALSE;
    }
return TRUE;
}

struct oHash *makeOllyHash(struct dnaSeq *rootSeq, int maxMiss)
/* Make hash of oligos that miss by at most maxMiss */
{
struct oHash *hash = oHashNew();
DNA *dna = rootSeq->dna;
DNA *end = dna + rootSeq->size - ollySize;
int realCount = 0;

AllocArray(rMask, ollySize);
rHash = hash;
rMemUsed = oHashTableSize() * sizeof(hash->table[0]); 
while (dna <= end)
    {
    if (goodSeq(dna, ollySize))
        {
	rOlly = dna;
	rCount = 0;
	raddVarients(maxMiss, 0);
	++realCount;
	}
    ++dna;
    }
freez(&rMask);
if (realCount == 0)
    {
    warn("No unmasked oligoes");
    return NULL;
    }
return hash;
}


void scanSeq(struct oHash *hash, struct dnaSeq *seq, DNA *base, int *counts)
/* Scan a sequence for hits. */
{
int i;
DNA *dna, *end = seq->dna + seq->size - ollySize;
DNA *hit;
for (dna = seq->dna; dna <= end; dna += 1)
    {
    struct oHashEl *el;
    for (el = hash->table[oHashFunc(dna)&hash->mask]; el != NULL; el = el->next)
	{
	if (closeEnough(el->pt, dna))
	    {
	    ++counts[el->pt - base];
	    break;
	    }
	}
    }
}

void scanNib(struct oHash *hash, char *fileName, DNA *base, int *counts)
/* Scan nib file for hits */
{
struct dnaSeq *seq = nibLoadAll(fileName);
scanSeq(hash, seq, base, counts);
reverseComplement(seq->dna, seq->size);
freeDnaSeq(&seq);
}

void olly(char *nibDir, char *chrom, int start, int end, char *hitOut)
/* olly - Look for matches and near matches to short sequences genome-wide. */
{
char nibName[512];
struct slName *dirList, *el;
FILE *f = mustOpen(hitOut, "w");
struct dnaSeq *chromSeq;
struct dnaSeq *ollyList;
struct oHash *hash;
int seqSize = end - start;
int ollyCount = seqSize - ollySize + 1;
int i, *counts;

/* Load up patch to scan for oligos.  We want the repeats in upper
 * case and the unique parts in lower case. */
sprintf(nibName, "%s/%s.nib", nibDir, chrom);
chromSeq = nibLoadPartMasked(NIB_MASK_MIXED, nibName, start, seqSize);
toggleCase(chromSeq->dna, chromSeq->size);

hash = makeOllyHash(chromSeq, maxDiff);
AllocArray(counts, seqSize);
if (hash != NULL)
    {
    dirList = listDir(nibDir, "*.nib");
    for (el = dirList; el != NULL; el = el->next)
	{
	sprintf(nibName, "%s/%s", nibDir, el->name);
	uglyf("%s\n", nibName);
	scanNib(hash, nibName, chromSeq->dna, counts);
	}
    if (easyOut)
	{
	fprintf(f, "olly %s %d %d\n", chrom, start, ollyCount);
	for (i=0; i<ollyCount; ++i)
	    fprintf(f, "%d\n", counts[i]);
	}
    else
	{
	int total = 0;
	int firstReal = -1, lastReal = 0;
	for (i=0; i<ollyCount; ++i)
	    {
	    if (counts[i] != 0) 
		{
	    	++total;
		if (firstReal < 0)
		    firstReal = i;
		lastReal = i;
		}
	    }
	if (total > 0)
	    {
	    fprintf(f, "%s\t%d\t%d\t", chrom, start + firstReal + ollySize/2, start + lastReal + ollySize/2 + 1);
	    fprintf(f, ".\t0\t+\t%d\t", total);
	    for (i=0; i<ollyCount; ++i)
		{
		if (counts[i] != 0)
		    fprintf(f, "%d,", i-firstReal);
		}
	    fprintf(f, "\t");
	    for (i=0; i<ollyCount; ++i)
	        {
		int c = counts[i];
		if (c != 0)
		    {
		    fprintf(f, "%d,", c);
		    }
		}
	    fprintf(f, "\n");
	    }
	}
    }
}

int ollySizeNeighborhood()
/* Count number of oligos that differ by maxDiff
 * or less.*/
{
rHash = oHashNew();
rOlly = needMem(ollySize+1);
memset(rOlly, 'a', ollySize);
AllocArray(rMask, ollySize);
rCount = 0;
raddVarients(maxDiff, 0);
/* This guy doesn't need to clean up for now at least. */
return rCount;
}

void batchLineOut(FILE *f, char *nibDir, char *chromName, 
	int start, int end)
/* Output one line to batch file. */
{
fprintf(f, "olly %s %s %d %d -ollySize=%d -maxDiff=%d -ramMb=%d",
	nibDir, chromName, start, end, ollySize, maxDiff, ramMb);
if (easyOut)
    fprintf(f, " -easyOut");
fprintf(f, " {check out line out/%s_%d.olly}\n", chromName, start);
}

void makeBatch(char *batchFile, char *nibDir)
/* Make a batch file */
{
char nibName[512];
char chromName[128];
struct slName *dirList,  *dirEl;
FILE *f = mustOpen(batchFile, "w");
int ollySizePer = ollySizeNeighborhood() * sizeof(struct hashEl);
int fixedSize = oHashTableSize() * sizeof(struct hashEl *);
struct dnaSeq *chromSeq;
DNA *dna;
int i, lastOlly;
int rangeStart, goodCount;
int maxGood;

maxGood = (maxMem - fixedSize - bigChromSize)/ollySizePer;
uglyf("maxGood = %d\n", maxGood);
dirList = listDir(nibDir, "*.nib");
for (dirEl = dirList; dirEl != NULL; dirEl = dirEl->next)
    {
    sprintf(nibName, "%s/%s", nibDir, dirEl->name);
    splitPath(dirEl->name, NULL, chromName, NULL);
    chromSeq = nibLoadAllMasked(NIB_MASK_MIXED, nibName);
    printf("%s \n", chromName);
    dna = chromSeq->dna;
    toggleCase(dna, chromSeq->size);
    lastOlly = chromSeq->size - ollySize;
    rangeStart = goodCount = 0;
    for (i=0; i <= lastOlly; ++i)
        {
	if (goodSeq(dna+i, ollySize))
	    {
	    ++goodCount;
	    if (goodCount >= maxGood)
	        {
		batchLineOut(f, nibDir, chromName, rangeStart, i+ollySize);
		rangeStart = i;
		goodCount = 0;
		}
	    }
	}
    if (goodCount != 0)
        {
	batchLineOut(f, nibDir, chromName, rangeStart, chromSeq->size);
	}
    freeDnaSeq(&chromSeq);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
dnaUtilOpen();
ollySize = optionInt("ollySize", ollySize);
maxDiff = optionInt("maxDiff", maxDiff);
ramMb = optionInt("ramMb", ramMb);
maxMem = 1024 * 1024 * ramMb;
easyOut = optionExists("easyOut");
if (optionExists("makeBatch"))
    {
    if (argc < 2)
        errAbort("Need nibDir");
    makeBatch(optionVal("makeBatch", NULL), argv[1]);
    }
else
    {
    if (argc != 6)
	usage();
    if (maxDiff > 3)
	errAbort("Can only handle maxDiff up to 3");
    olly(argv[1], argv[2], atoi(argv[3]), atoi(argv[4]), argv[5]);
    }
return 0;
}
