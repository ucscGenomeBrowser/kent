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

int maxDiff = 3;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "olly - Look for matches and near matches to short sequences genome-wide\n"
  "usage:\n"
  "   olly nibDir oligoes.fa out.hit\n"
  "options:\n"
  "   -maxDiff=N (default %d) Maximum variation in bases.\n"
  , maxDiff
  );
}

int ollySize;	/* Oligomer size. */
int oHashSize = 25;

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
//    int queryCount;	      /* Number of things we've looked for. */
 //   int checkCount;	      /* Number of things we've checked. */
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

struct oHash *oHashNew()
/* Allocate oligomer hash */
{
struct oHash *hash;
int tableSize = (1<<(oHashSize));
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
// ++hash->queryCount;
for (el = hash->table[oHashFunc(key)&hash->mask]; el != NULL; el = el->next)
    {
    // ++hash->checkCount;
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

void raddVarients(int maxMiss)
/* Add all varients of olly that differ from olly by at most maxMiss.  Don't bother
 * varying at mask position. */
{
++rCount;
oHashAdd(rHash, rOlly);
if (maxMiss > 0)
    {
    int i,j;
    DNA save;
    static char nt[4] = {'a', 'c', 'g', 't'};
    for (i=0; i<ollySize; ++i)
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
		    raddVarients(maxMiss-1);
		    }
		}
	    rMask[i] = FALSE;
	    rOlly[i] = save;
	    }
	}
    }
}

struct oHash *makeOllyHash(struct dnaSeq *ollyList, int maxMiss)
/* Make hash of oligos that miss by at most maxMiss */
{
struct oHash *hash = oHashNew();
struct dnaSeq *olly;
bool *mask;

AllocArray(mask, ollySize);
rMask = mask;
rHash = hash;
for (olly = ollyList; olly != NULL; olly = olly->next)
    {
    rOlly = olly->dna;
    rCount = 0;
    raddVarients(maxMiss);
    uglyf("%s has %d variants\n", olly->dna, rCount);
    }
freez(&mask);
return hash;
}

struct dnaSeq *readAllSameSize(char *fileName)
/* Read a fa file full of oligoes of the same size . */
{
struct dnaSeq *seqList = faReadAllDna(fileName), *seq;
int size;
if (seqList == NULL)
    errAbort("Nothing in %s", fileName);
size = seqList->size;
for (seq = seqList; seq != NULL; seq = seq->next)
    {
    if (size != seq->size)
	errAbort("First sequence in %s is %d bases, but %s is %d", fileName, size, seq->name, seq->size);
    }
return seqList;
}

void scanNib(struct oHash *hash, char *fileName, FILE *f)
/* Scan nib file for hits */
{
struct dnaSeq *seq = nibLoadAll(fileName);
int i;
char save;
DNA *dna, *end = seq->dna + seq->size - ollySize, *e;
DNA *hit;
for (dna = seq->dna; dna < end; dna += 1)
    {
    e = dna + ollySize;
    save = *e;
    *e = 0;
    if ((hit = oHashFindVal(hash, dna)) != NULL)
	 fprintf(f, "%s\t%d\n%s\n%s\n\n",  fileName, dna - seq->dna, 
		dna, hit);
    *e = save;
    }
freeDnaSeq(&seq);
}

void olly(char *nibDir, char *oligoSpec, char *hitOut)
/* olly - Look for matches and near matches to short sequences genome-wide. */
{
char nibName[512];
struct slName *dirList, *el;
FILE *f = mustOpen(hitOut, "w");
struct dnaSeq *ollyList = readAllSameSize(oligoSpec);
struct oHash *hash;

ollySize = ollyList->size;
hash = makeOllyHash(ollyList, maxDiff);
dirList = listDir(nibDir, "*.nib");
for (el = dirList; el != NULL; el = el->next)
    {
    sprintf(nibName, "%s/%s", nibDir, el->name);
    uglyf("%s\n", nibName);
    scanNib(hash, nibName, f);
    }
// uglyf("%d queries, %d checks, ratio %f\n", hash->queryCount, hash->checkCount, (double)hash->checkCount/hash->queryCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
dnaUtilOpen();
if (argc != 4)
    usage();
maxDiff = optionInt("maxDiff", maxDiff);
if (maxDiff > 3)
    errAbort("Can only handle maxDiff up to 3");
olly(argv[1], argv[2], argv[3]);
return 0;
}
