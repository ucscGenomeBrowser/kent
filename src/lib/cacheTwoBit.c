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
    bool doRc;	// If set, cache reverse complement
    bool doUpper; // If set sequence is upper cased before caching
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

struct cacheTwoBitRanges *cacheTwoBitRangesNew(boolean doUpper)
/* Create a new cache for ranges or complete sequence in two bit files */
{
struct cacheTwoBitRanges *cache;
AllocVar(cache);
cache->urlHash = hashNew(0);
cache->doUpper = doUpper;
return cache;
}

struct cacheTwoBitSeq *cacheTwoBitSeqNew(struct cacheTwoBitUrl *cacheUrl, 
    char *seqName, boolean doRc, boolean doUpper)
/* Create a new cacheTwoBitSeq structure */
{
struct cacheTwoBitSeq *ctbSeq;
AllocVar(ctbSeq);
ctbSeq->seqName = cloneString(seqName);
ctbSeq->seqSize = twoBitSeqSize(cacheUrl->tbf, seqName);
ctbSeq->doRc = doRc;
ctbSeq->doUpper = doUpper;
ctbSeq->tbf = cacheUrl->tbf;
ctbSeq->rangeTree = rangeTreeNew();
return ctbSeq;
}



static struct dnaSeq *cacheTwoBitSeqFetch(struct cacheTwoBitSeq *ctbSeq, 
    int start, int end, int *retOffset)
/* Get dna sequence from file */
{
if (start == 0 && end == 0)  /* Spacial case, we read whole seq */
    end = twoBitSeqSize(ctbSeq->tbf, ctbSeq->seqName);

int size = end-start;
if (size <= 0)
   errAbort("start >= end in cacheTwoBitSeqFetch. start %d, end %d\n", start, end);
long offset = 0;
struct dnaSeq *seq = NULL;
ctbSeq->basesQueried += size;
ctbSeq->queryCount += 1;
struct rbTree *rangeTree = ctbSeq->rangeTree;
struct range *enclosing = rangeTreeFindEnclosing(rangeTree, start, end);
if (enclosing != NULL)
    {
    seq = enclosing->val;
    offset = enclosing->start;
    // fprintf(stderr,  "reusing %d %s:%d+%d\n", ctbSeq->doRc, ctbSeq->seqName, start, size);
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

    // fprintf(stderr,  "fetching new %d %s:%d+%d\n", ctbSeq->doRc, ctbSeq->seqName, start, size); 
    int rcStart = start, rcEnd = end;
    if (ctbSeq->doRc)
	{
	reverseIntRange(&rcStart, &rcEnd, ctbSeq->seqSize);
	}
    seq = twoBitReadSeqFrag(ctbSeq->tbf, ctbSeq->seqName, rcStart, rcEnd);
    if (seq != NULL)
	{
	if (ctbSeq->doUpper)
	    toUpperN(seq->dna, seq->size);
	if (ctbSeq->doRc)
	    {
	    reverseComplement(seq->dna, seq->size);
	    }
	rangeTreeAddVal(rangeTree, start, end, seq, NULL);
	ctbSeq->basesRead += size;
	offset = start;
	}
    }
*retOffset = offset;
return seq;
}


static struct dnaSeq *cacheTwoBitRangesFetchOrNot(struct cacheTwoBitRanges *cacheAll, 
    char *url, char *seqName, int start, int end, boolean doRc, boolean missOk, int *retOffset)
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
if (missOk && !twoBitHasSeq(cacheUrl->tbf, seqName))
   {
   return NULL;
   }
struct hash *seqHash = (doRc ? cacheUrl->rcSeqHash : cacheUrl->seqHash);
struct cacheTwoBitSeq *ctbSeq = hashFindVal(seqHash, seqName);

if (ctbSeq == NULL)
    {
    ctbSeq = cacheTwoBitSeqNew(cacheUrl, seqName, doRc, cacheAll->doUpper);
    hashAdd(seqHash, seqName, ctbSeq);
    slAddHead(&cacheUrl->seqList, ctbSeq);
    }
struct dnaSeq *seq = NULL;
if (start < end || !missOk)
    seq = cacheTwoBitSeqFetch(ctbSeq, start, end, retOffset);
return seq;
}

struct dnaSeq *cacheTwoBitRangesFetch(struct cacheTwoBitRanges *cacheAll, 
    char *url, char *seqName, int start, int end, boolean doRc, int *retOffset)
/* Fetch a sequence from a twoBit cache. The result in retOffset is where the return dnaSeq
 * sits within the named sequence, the whole of which is stored in the subtracted 
 * associated twoBit file. Do not free the returned sequence. Complains and aborts if
 * url not found, or if seqName not found in URL */
{
return cacheTwoBitRangesFetchOrNot(cacheAll, url, seqName, start, end, doRc, FALSE, retOffset);
}

struct dnaSeq *cacheTwoBitRangesMayFetch(struct cacheTwoBitRanges *cacheAll, 
	char *url, char *seqName, int start, int end, boolean doRc, int *retOffset)
/* Fetch a sequence from a twoBit cache. The result in retOffset is where the return dnaSeq
 * sits within the named sequence, the whole of which is stored in the subtracted 
 * associated twoBit file. Do not free the returned sequence. Returns NULL if sequence not
 * found in any of the files we are caching without complaint. */
{
return cacheTwoBitRangesFetchOrNot(cacheAll, url, seqName, start, end, doRc, TRUE, retOffset);
}


void cacheTwoBitRangesPrintStats(struct cacheTwoBitRanges *cache, FILE *f)
/* print cache statistics - Debugging routine */
{
fprintf(f, "caching %d twoBit files\n", slCount(cache->urlList));
struct cacheTwoBitUrl *cachedUrl;
int totalSeq = 0;
int totalRanges = 0;
long basesQueried = 0;	// Total bases read from cache
long basesRead = 0;	// Total bases read by cache
int queryCount = 0;
for (cachedUrl = cache->urlList; cachedUrl != NULL; cachedUrl = cachedUrl->next)
    {
    fprintf(f, "%s has %d + strand, %d minus strand sequences cached\n",
	cachedUrl->url, cachedUrl->seqHash->elCount, cachedUrl->rcSeqHash->elCount);
    struct cacheTwoBitSeq *ctbSeq;
    for (ctbSeq = cachedUrl->seqList; ctbSeq != NULL; ctbSeq = ctbSeq->next)
	{
	fprintf(f, "  %s %c strand\n", ctbSeq->seqName, ctbSeq->doRc ? '-' : '+');
	totalSeq += 1;
	totalRanges += ctbSeq->rangeTree->n;
	basesQueried += ctbSeq->basesQueried;
	basesRead += ctbSeq->basesRead;
	queryCount += ctbSeq->queryCount;
	struct range *range = rangeTreeList(ctbSeq->rangeTree);
	for ( ; range != NULL; range = range->next)
	    {
	    fprintf(f, "    %d start %d size\n", range->start, range->end - range->start);
	    }
	}
    }
fprintf(f, "total sequences cached %d in %d ranges covering %d queries\n", 
    totalSeq, totalRanges, queryCount);
fprintf(f, "basesRead %ld %3.1f%% of bases queried %ld\n", basesRead, 
    100.0*basesRead/basesQueried, basesQueried);
}

