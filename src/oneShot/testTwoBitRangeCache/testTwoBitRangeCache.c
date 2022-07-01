/* testTwoBitRangeCache - Test a caching capability for sequences from two bit files.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "twoBit.h"
#include "rangeTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testTwoBitRangeCache - Test a caching capability for sequences from two bit files.\n"
  "usage:\n"
  "   testTwoBitRangeCache input.tab\n"
  "Where input.tab is a tab-separated-file with columns:\n"
  "   file.2bit	seqName	strand	startBase   endBase\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct cacheSeqRanges
/*  Cached ranges of single sequence*/
    {
    struct cacheSeqRanges *next;
    char *seqName;	// Name of sequence
    int seqSize;	// Size of sequence
    struct twoBitFile *tbf;  // Associated twoBit file;
    boolean doRc;	// If set, cache reverse complement
    struct rbTree *rangeTree;	// Values of this range tree are DNA seqs 
    long basesQueried;	// Total bases read from cache
    long basesRead;	// Total bases read by cache
    int queryCount;	// Number of queries made
    };

struct cacheTwoBitUrl
/* An open two bit file and a hash of sequences in it */
    {
    struct cacheTwoBitUrl *next;    /* Next in list */
    char *url;			    /* Name of URL hashed */
    struct twoBitFile *tbf;	    /* Open two bit file */
    struct hash *seqHash;	    /* Hash of cacheSeqRanges */
    struct hash *rcSeqHash;	    /* Hash of reverse-complemented cacheSeqRanges */
    struct cacheSeqRanges *seqList;   /* List of sequences accessed */
    };

struct cacheTwoBits
/* Cache open two bit files and sequences */
    {
    struct cacheTwoBits *next;	/* Next in list of such caches if you need. */
    struct hash *urlHash;	/* Hash of twoBit file URLs */
    struct cacheTwoBitUrl *urlList;  /* List of values in above cache */
    };

struct cacheTwoBits *cacheTwoBitsNew()
/* Create a new cache for two bits */
{
struct cacheTwoBits *cache;
AllocVar(cache);
cache->urlHash = hashNew(0);
return cache;
}

struct cacheSeqRanges *cacheSeqRangesNew(struct cacheTwoBitUrl *cacheUrl, char *seqName, 
    boolean doRc)
/* Create a new cacheSeqRanges structure */
{
struct cacheSeqRanges *csRange;
AllocVar(csRange);
csRange->seqName = cloneString(seqName);
csRange->seqSize = twoBitSeqSize(cacheUrl->tbf, seqName);
csRange->doRc = doRc;
csRange->tbf = cacheUrl->tbf;
csRange->rangeTree = rangeTreeNew();
return csRange;
}



struct dnaSeq *cacheSeqRangesFetch(struct cacheSeqRanges *csRange, int start, int end, int *retOffset)
/* Get two bit file */
{
int size = end-start;
if (size <= 0)
   errAbort("start >= end in cacheSeqRangesFetch. start %d, end %d\n", start, end);
long offset = 0;
struct dnaSeq *seq = NULL;
if (csRange->doRc)
    reverseIntRange(&start, &end, csRange->seqSize);
csRange->basesQueried += size;
csRange->queryCount += 1;
struct rbTree *rangeTree = csRange->rangeTree;
struct range *enclosing = rangeTreeFindEnclosing(rangeTree, start, end);
if (enclosing != NULL)
    {
    seq = enclosing->val;
    offset = enclosing->start;
    }
else
    {
    struct range *overlappingList = rangeTreeAllOverlapping(rangeTree, start, end);

    /* Expand start end so it includes range of all overlapping */
    struct range *range;
    for (range = overlappingList; range != NULL; range = range->next)
        {
	if (start > range->start) start = range->start;
	if (end < range->end) end = range->end;
	}
    seq = twoBitReadSeqFrag(csRange->tbf, csRange->seqName, start, end);
    if (seq != NULL)
	rangeTreeAddVal(rangeTree, start, end, seq, NULL);
    csRange->basesRead += size;
    offset = start;
    }
*retOffset = offset;
return seq;
}


struct dnaSeq *cacheTwoBitsFetchSeq(struct cacheTwoBits *cacheAll, char *url, char *seqName, 
    int start, int end, boolean doRc, int *retOffset)
/* fetch a sequence from a 2bit.  Caches open two bit files and sequence in 
 * both forward and reverse strand */
{
/* Init static url hash */
struct hash *urlHash = cacheAll->urlHash;  // hash of open files

struct cacheTwoBitUrl *cacheUrl = hashFindVal(urlHash, url);
if (cacheUrl == NULL)
    {
    AllocVar(cacheUrl);
    cacheUrl->url = cloneString(url);
    cacheUrl->tbf = twoBitOpen(url);
    cacheUrl->seqHash = hashNew(0);
    cacheUrl->rcSeqHash = hashNew(0);
    hashAdd(urlHash, url, cacheUrl);
    slAddHead(&cacheAll->urlList, cacheUrl);
    }
struct hash *seqHash = (doRc ? cacheUrl->rcSeqHash : cacheUrl->seqHash);
struct cacheSeqRanges *csRange = hashFindVal(seqHash, seqName);

if (csRange == NULL)
    {
    csRange = cacheSeqRangesNew(cacheUrl, seqName, doRc);
    hashAdd(seqHash, seqName, csRange);
    slAddHead(&cacheUrl->seqList, csRange);
    }
struct dnaSeq *seq = cacheSeqRangesFetch(csRange, start, end, retOffset);
return seq;
}

void printCacheStats(struct cacheTwoBits *cache)
{
printf("caching %d twoBit files\n", slCount(cache->urlList));
struct cacheTwoBitUrl *cachedUrl;
int totalSeq = 0;
int totalRanges = 0;
long basesQueried = 0;	// Total bases read from cache
long basesRead = 0;	// Total bases read by cache
int queryCount = 0;
for (cachedUrl = cache->urlList; cachedUrl != NULL; cachedUrl = cachedUrl->next)
    {
    struct cacheSeqRanges *csRange;
    for (csRange = cachedUrl->seqList; csRange != NULL; csRange = csRange->next)
	{
	printf("  %s %c strand\n", csRange->seqName, csRange->doRc ? '=' : '+');
	totalSeq += 1;
	totalRanges += csRange->rangeTree->n;
	basesQueried += csRange->basesQueried;
	basesRead += csRange->basesRead;
	queryCount += csRange->queryCount;
	struct range *range = rangeTreeList(csRange->rangeTree);
	for ( ; range != NULL; range = range->next)
	    {
	    printf("    %d %d\n", range->start, range->end);
	    }
	}
    }
printf("total sequences cached %d in %d ranges covering %d queries\n", 
    totalSeq, totalRanges, queryCount);
printf("basesRead %ld bases queried %ld\n", basesRead, basesQueried);
}

void testTwoBitRangeCache(char *input)
/* testTwoBitRangeCache - Test a caching capability for sequences from two bit files.. */
{
struct cacheTwoBits *cache = cacheTwoBitsNew();
/* Create a new cache for two bits */
struct lineFile *lf = lineFileOpen(input, TRUE);
char *row[5];
int rowCount = 0;
long totalBases = 0;
while (lineFileRow(lf, row))
    {
    char *fileName = row[0];
    char *seqName = row[1];
    char *strand = row[2];
    int startBase = lineFileNeedNum(lf, row, 3);
    int endBase = lineFileNeedNum(lf, row, 4);
    int baseSize = endBase - startBase;
    boolean isRc = (strand[0] == '-');
    int seqOffset=0;
    struct dnaSeq *seq = cacheTwoBitsFetchSeq(cache, fileName, 
	seqName, startBase, endBase, isRc, &seqOffset);
    if (seq == NULL)
        warn("Missing seq %s:%s\n", fileName, seqName);
    totalBases += baseSize;
    ++rowCount;
    }
printf("%ld total bases in %d rows, %d 2bit files\n", totalBases, rowCount, cache->urlHash->elCount);
printCacheStats(cache);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
testTwoBitRangeCache(argv[1]);
return 0;
}
