/* olly25 - Look for matches and near matches to 25-mers genome-wide. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "localmem.h"
#include "dnautil.h"
#include "fa.h"
#include "nib.h"

int maxDiff = 3;
int ollySize = 25;
int ramMb = 490;
boolean easyOut = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "olly25 - Look for matches and near matches to short sequences genome-wide\n"
  "Output can be loaded as a wiggle track\n"
  "usage:\n"
  "   olly25 nibDir chrom start stop out.sample\n"
  "example:\n"
  "   olly25 ~/oo/mixedNib chr1 0 1000 chr1_0_1000.sample\n"
  "options:\n"
  "   -maxDiff=N (default %d) Maximum variation in bases. Must be 3 or less.\n"
  "   -ollySize=N (default %d) Size of oligo.\n"
  "   -ramMb=N (default %d) Size of RAM to use\n"
  "   -makeBatch=parasolSpec Make batch file for parasol\n"
  "       In this case just do \n"
  "         olly nibDir -makeBatch=spec.\n"
  "       Spec will be a parasol spec to do everything in nibDir\n" 
  , maxDiff, ollySize, ramMb
  );
}

/* Seeds for 2 and 3 mismatches calculated with checkSeed. */

char seed0[] = "111111111111";		/* This seed works down to 12-mers for
                                         * no mismatches. */
char seed1[] = "1111110111111";		/* This seed works down to 19-mers for
                                         * single mismaches allowed. */
char seed2[] = "111100110101111";	/* One of 12 seeds that will work 
                                         * for 11 significant bases, 2
					 * mismatches allowed on 25-mers.
					 * There are no possible 12 significant
					 * base seeds. */
char seed3[] = "11101001000111";	/* This is the shortest seed that
                                         * works for 8 significant bases,
					 * 3 mismatches allowed on 25-mers.
					 * It will work for 24-mers too.
					 * There are no possible 9 significant
					 * base seeds. */
char *seedForMiss[4] = {seed0, seed1, seed2, seed3};
int minOllySize[4] = {12, 19, 25, 24};

int maxMem;		/* Maximum amount of memory to use. */
int bigChromSize = 240000000;

int seedWeight;	 /* Number of non-zeroes in seed. */
int seedSpan;	 /* Number of bases covered by seed. */
int *seedOff;	 /* Offsets of significant characters in seed. */
char *seedChars; /* 0's and 1's that make up seed. */
int seedIndexSize; /* Size of seed index. */

void seedInit(char *seed)
/* Initialize seed system from a string of ascii 0 and 1's. */
{
int i, wIx = 0;
seedSpan = strlen(seed);
seedChars = seed;
seedWeight = countChars(seed, '1');
seedIndexSize = (1<<(2*seedWeight));
AllocArray(seedOff, seedWeight);
for (i=0; i<seedSpan; ++i)
    {
    if (seed[i] == '1')
        seedOff[wIx++] = i;
    }
uglyf("%s\n", seed);
for (i=0; i<seedWeight; ++i)
    uglyf("%d,", seedOff[i]);
uglyf("\n");
}

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
return seedIndexSize+1;
}

struct oHash *oHashNew()
/* Allocate oligomer hash */
{
struct oHash *hash;
int tableSize = oHashTableSize();
uglyf("tableSize %d\n", tableSize);
AllocVar(hash);
AllocArray(hash->table, tableSize);
hash->lm = lmInit(1024 * 256);
return hash;
}

int oHashFunc(DNA *dna)
/* DNA hash function. */
{
int oneVal;
int val = 0;
int nt;
int i;
for (i=0;i<seedWeight;++i)
    {
    oneVal = ntValLower[dna[seedOff[i]]];
    if (oneVal < 0)
	{
	val = seedIndexSize;	/* This slot is always empty */
	break;
	}
    val <<= 2;
    val += oneVal;
    }
return val;
}


struct oHashEl *oHashAdd(struct oHash *hash, DNA *key)
/* Add to hash */
{
struct oHashEl *el, **pList;
lmAllocVar(hash->lm, el);
el->pt = key;
pList = hash->table + oHashFunc(key);
slAddHead(pList, el);
return el;
}

DNA *oHashFindVal(struct oHash *hash, DNA *key)
/* Find value in hash */
{
struct oHashEl *el;
for (el = hash->table[oHashFunc(key)]; el != NULL; el = el->next)
    {
    if (closeEnough(el->pt, key))
	return el->pt;
    }
return NULL;
}

boolean goodSeq(DNA *dna)
/* Return TRUE if all lower case and non-n. */
{
int i;
return oHashFunc(dna) != seedIndexSize;
}

struct oHash *makeOllyHash(struct dnaSeq *rootSeq, int maxMiss)
/* Make hash of oligos that miss by at most maxMiss */
{
struct oHash *hash = oHashNew();
DNA *dna = rootSeq->dna;
DNA *end = dna + rootSeq->size - seedSpan;
int realCount = 0;

while (dna <= end)
    {
    if (goodSeq(dna))
        {
	oHashAdd(hash, dna);
	++realCount;
	}
    ++dna;
    }
if (realCount == 0)
    {
    warn("No unmasked oligoes");
    return NULL;
    }
return hash;
}

struct hit
/* A single hit - matching 25-mer. */
    {
    struct hit *next;
    int tPos;		/* Target position of start of hit */
    };

void scanNeighborhood(struct dnaSeq *tSeq, DNA *tHit, char strand,
	DNA *query, int qSize, DNA *qHit, 
	struct hit **hits,  struct lm *lm, int *counts)
/* Given a hit, scan for 25-mers that overlap hit that are
 * good enough. */
{
DNA *target = tSeq->dna;
int tSize = tSeq->size;
int ollySeedDiff = ollySize - seedSpan;
int tStart = tHit - target - ollySeedDiff;
int tEnd = tHit - target + seedSpan + ollySeedDiff;
int qStart = qHit - query - ollySeedDiff;
int qEnd = qHit - query + seedSpan + ollySeedDiff;
int pastEnd, regionSize, diffCount = 0;
int ollyInSize = ollySize - 1;
int i;

/* Clip. */
if (qStart < 0)
    {
    tStart -= qStart;
    qStart = 0;
    }
if (tStart < 0)
    {
    qStart -= tStart;
    tStart = 0;
    }
pastEnd = qEnd -  qSize;
if (pastEnd > 0)
    {
    qEnd -= pastEnd;
    tEnd -= pastEnd;
    }
pastEnd = tEnd - tSize;
if (pastEnd > 0)
    {
    qEnd -= pastEnd;
    tEnd -= pastEnd;
    }
regionSize = qEnd - qStart;
assert(regionSize == tEnd - tStart);
assert(regionSize >= ollySize);

/* Do initial scan to count up differences in first ollyInSize-1
 * bases. */
for (i=0; i<ollyInSize; ++i)
    {
    if (target[i+tStart] != query[i+qStart])
       ++diffCount;
    }

/* Add base to end of oligoSize window, output if matches well enough,
 * then subtract base at start of window. */
for (i=ollyInSize; i<regionSize; ++i)
    {
    int tPos = i + tStart;
    int qPos = i + qStart;
    if (target[tPos] != query[qPos])
       ++diffCount;
    tPos -= ollyInSize;
    qPos -= ollyInSize;
    if (diffCount <= maxDiff)
       {
       boolean alreadyRecorded = FALSE;
       struct hit *hit;
       for (hit = hits[qPos]; hit != NULL; hit = hit->next)
           {
	   if (hit->tPos == tPos)
	      {
	      alreadyRecorded = TRUE;
	      break;
	      }
	   }
       if (!alreadyRecorded)
           {
	   counts[qPos] += 1;
	   lmAllocVar(lm, hit);
	   hit->tPos = tPos;
	   slAddHead(&hits[qPos], hit);
	   }
       }
    if (target[tPos] != query[qPos])
       --diffCount;
    }
}

void scanSeq(struct oHash *hash, struct dnaSeq *geno, DNA *query, int querySize,
	int *counts, char strand)
/* Scan a sequence for hits. */
{
int i;
DNA *dna, *end = geno->dna + geno->size - seedSpan;
struct lm *lm = lmInit(1024);
struct hit **hits;

AllocArray(hits, querySize);
for (dna = geno->dna; dna <= end; dna += 1)
    {
    struct oHashEl *el;
    for (el = hash->table[oHashFunc(dna)]; el != NULL; el = el->next)
	{
	scanNeighborhood(geno, dna, strand, query, querySize, el->pt,
		hits, lm, counts);
	}
    }
freez(&hits);
lmCleanup(&lm);
}

void scanNib(struct oHash *hash, char *fileName, DNA *base, int baseSize,
	int *counts)
/* Scan nib file for hits */
{
struct dnaSeq *geno = nibLoadAll(fileName);
scanSeq(hash, geno, base, baseSize, counts, '+');
reverseComplement(geno->dna, geno->size);
scanSeq(hash, geno, base, baseSize, counts, '-');
freeDnaSeq(&geno);
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
	scanNib(hash, nibName, chromSeq->dna, chromSeq->size, counts);
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


#ifdef OLD
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
	if (goodSeq(dna+i))
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
#endif /* OLD */

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

if (maxDiff > 3)
   errAbort("maxDiff can only be up to 3");
if (ollySize < minOllySize[maxDiff] )
   errAbort("For %d mismatches, minimum oligo size is %d", maxDiff, ollySize);

if (argc != 6)
     usage();
seedInit(seedForMiss[maxDiff]);
olly(argv[1], argv[2], atoi(argv[3]), atoi(argv[4]), argv[5]);
return 0;
}
