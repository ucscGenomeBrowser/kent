/* cacheTwoBit - system for caching open two bit files and ranges of sequences in them */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "twoBit.h"
#include "rangeTree.h"
#include "cacheTwoBit.h"

struct cacheTwoBitSeq
/*  Cached ranges of single sequence in a twoBit file */
    {
    struct cacheTwoBitSeq *next;
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
/* An open two bit file from the internet and a hash of sequences in it */
    {
    struct cacheTwoBitUrl *next;    /* Next in list */
    char *url;			    /* Name of URL hashed */
    struct twoBitFile *tbf;	    /* Open two bit file */
    struct hash *seqHash;	    /* Hash of cacheTwoBitSeq */
    struct hash *rcSeqHash;	    /* Hash of reverse-complemented cacheTwoBitSeq */
    struct cacheTwoBitSeq *seqList;   /* List of sequences accessed */
    };
struct cacheTwoBitRanges *cacheTwoBitRangesNew()
/* Create a new cache for ranges or complete sequence in two bit files */
{
struct cacheTwoBitRanges *cache;
AllocVar(cache);
cache->urlHash = hashNew(0);
return cache;
}

struct cacheTwoBitSeq *cacheTwoBitSeqNew(struct cacheTwoBitUrl *cacheUrl, 
    char *seqName, boolean doRc)
/* Create a new cacheTwoBitSeq structure */
{
struct cacheTwoBitSeq *ctbSeq;
AllocVar(ctbSeq);
ctbSeq->seqName = cloneString(seqName);
ctbSeq->seqSize = twoBitSeqSize(cacheUrl->tbf, seqName);
ctbSeq->doRc = doRc;
ctbSeq->tbf = cacheUrl->tbf;
ctbSeq->rangeTree = rangeTreeNew();
return ctbSeq;
}



static struct dnaSeq *cacheTwoBitSeqFetch(struct cacheTwoBitSeq *ctbSeq, 
    int start, int end, int *retOffset)
/* Get dna sequence from file */
{
int size = end-start;
if (size <= 0)
   errAbort("start >= end in cacheTwoBitSeqFetch. start %d, end %d\n", start, end);
long offset = 0;
struct dnaSeq *seq = NULL;
ctbSeq->basesQueried += size;
ctbSeq->queryCount += 1;
if (ctbSeq->doRc)
    reverseIntRange(&start, &end, ctbSeq->seqSize);
struct rbTree *rangeTree = ctbSeq->rangeTree;
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
    seq = twoBitReadSeqFrag(ctbSeq->tbf, ctbSeq->seqName, start, end);
    if (seq != NULL)
	{
	if (ctbSeq->doRc)
	    reverseComplement(seq->dna, seq->size);
	rangeTreeAddVal(rangeTree, start, end, seq, NULL);
	}
    ctbSeq->basesRead += size;
    offset = start;
    }
*retOffset = offset;
return seq;
}


struct dnaSeq *cacheTwoBitRangesFetch(struct cacheTwoBitRanges *cacheAll, char *url, char *seqName, 
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
struct cacheTwoBitSeq *ctbSeq = hashFindVal(seqHash, seqName);

if (ctbSeq == NULL)
    {
    ctbSeq = cacheTwoBitSeqNew(cacheUrl, seqName, doRc);
    hashAdd(seqHash, seqName, ctbSeq);
    slAddHead(&cacheUrl->seqList, ctbSeq);
    }
struct dnaSeq *seq = cacheTwoBitSeqFetch(ctbSeq, start, end, retOffset);
return seq;
}


void cacheTwoBitRangesPrintStats(struct cacheTwoBitRanges *cache)
/* print cache statistics - Debugging routine */
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
    printf("%s has %d + strand, %d minus strand sequences cached\n",
	cachedUrl->url, cachedUrl->seqHash->elCount, cachedUrl->rcSeqHash->elCount);
    struct cacheTwoBitSeq *ctbSeq;
    for (ctbSeq = cachedUrl->seqList; ctbSeq != NULL; ctbSeq = ctbSeq->next)
	{
	printf("  %s %c strand\n", ctbSeq->seqName, ctbSeq->doRc ? '=' : '+');
	totalSeq += 1;
	totalRanges += ctbSeq->rangeTree->n;
	basesQueried += ctbSeq->basesQueried;
	basesRead += ctbSeq->basesRead;
	queryCount += ctbSeq->queryCount;
	struct range *range = rangeTreeList(ctbSeq->rangeTree);
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

