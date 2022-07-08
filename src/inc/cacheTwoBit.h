#ifndef CACHETWOBIT_H
#define CACHETWOBIT_H

/* cacheTwoBit - system for caching open two bit files and ranges of sequences in them */


struct cacheTwoBitRanges
/* Cache open two bit files and sequences */
    {
    struct cacheTwoBitRanges *next;	/* Next in list of such caches if you need. */
    struct hash *urlHash;	/* Hash of cacheTwoBitUrl structs */
    struct cacheTwoBitUrl *urlList;  /* List of values in urlHash */
    boolean doUpper;	/* If true then force all sequence to upper case. */
    };

struct cacheTwoBitRanges *cacheTwoBitRangesNew(boolean doUpper);
/* Create a new cache for ranges or complete sequence in two bit files */

void cacheTwoBitRangesPrintStats(struct cacheTwoBitRanges *cache, FILE *f);
/* print cache statistics - Debugging routine */

struct dnaSeq *cacheTwoBitRangesFetch(struct cacheTwoBitRanges *cacheAll, 
	char *url, char *seqName, int start, int end, boolean doRc, int *retOffset);
/* Fetch a sequence from a twoBit cache. The result in retOffset is where the return dnaSeq
 * sits within the named sequence, the whole of which is stored in the subtracted 
 * associated twoBit file. Do not free the returned sequence. Complains and aborts if
 * url not found, or if seqName not found in URL */

struct dnaSeq *cacheTwoBitRangesMayFetch(struct cacheTwoBitRanges *cacheAll, 
	char *url, char *seqName, int start, int end, boolean doRc, int *retOffset);
/* Fetch a sequence from a twoBit cache. The result in retOffset is where the return dnaSeq
 * sits within the named sequence, the whole of which is stored in the subtracted 
 * associated twoBit file. Do not free the returned sequence. Returns NULL if sequence not
 * found in any of the files we are caching without complaint. */

#endif /* CACHETWOBIT_H */
