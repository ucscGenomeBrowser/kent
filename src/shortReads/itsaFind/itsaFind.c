/* itsaFind - Find sequence by searching indexed traversable suffix array.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "itsa.h"

static char const rcsid[] = "$Id: itsaFind.c,v 1.1 2008/10/30 04:34:09 kent Exp $";

boolean mmap;
int maxMismatch = 2;
int maxRepeat = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "itsaFind - Find sequence by searching indexed traversable suffix array.\n"
  "usage:\n"
  "   itsaFind target.itsa query.fa output\n"
  "options:\n"
  "   -maxRepeat=N  - maximum number of alignments to output on one query sequence. Default %d\n"
  "   -mmap - Use memory mapping. Faster just a few reads, but much slower for many reads\n"
  "   -maxMismatch - maximum number of mismatches allowed.  Default %d\n"
  , maxRepeat, maxMismatch
  );
}

static struct optionSpec options[] = {
   {"maxRepeat", OPTION_INT},
   {"mmap", OPTION_BOOLEAN},
   {"maxMismatch", OPTION_INT},
   {NULL, 0},
};

/* Table to convert letters to ITSA_A, ITSA_C, ITSA_G, ITSA_T . */
int baseToVal[256];

void finalSearch(DNA *tDna, bits32 *suffixArray, bits32 searchStart, bits32 searchEnd, 
	DNA *qDna, int qSize, int alreadyMatched, struct slInt **pHitList)
/* Our search has been narrowed to be between searchStart and searchEnd.
 * We know within the interval a prefix of size alreadyMatched is already
 * the same.  Here we check if anything in this interval to see if there is
 * a full match to anything. If so we add it to hitList.  Due to the peculiarities
 * of the array seach with each successive item in the window we've checked one
 * more letter already. */
{
// uglyf("finalSearch. q=%s, searchStart=%d, searchEnd=%d, alreadyMatched=%d\n", qDna, searchStart, searchEnd, alreadyMatched);
bits32 searchIx;
for (searchIx = searchStart; searchIx < searchEnd; ++searchIx)
    {
    int diff = memcmp(qDna+alreadyMatched, tDna+alreadyMatched+suffixArray[searchIx], 
    	qSize-alreadyMatched);
    /* Todo - break without a hit when diff is the wrong sign. */
//      uglyf("q "); mustWrite(uglyOut, qDna+alreadyMatched, qSize-alreadyMatched); uglyf("\n");
//      uglyf("t "); mustWrite(uglyOut, tDna+suffixArray[searchIx]+alreadyMatched, qSize-alreadyMatched); uglyf("\n");
    if (diff == 0)
        {
	struct slInt *hit = slIntNew(searchIx);
	slAddHead(pHitList, hit);
// 	uglyf("Hit!\n");
	break;
	}
    ++alreadyMatched;
    }
}

void itsaFindWithin(DNA *tDna, bits32 *suffixArray, bits32 *traverseArray, 
	DNA *qDna, int qSize, int cursor, bits32 searchStart, bits32 searchEnd, 
	struct slInt **pHitList)
/* Search the part of the suffix array between searchStart and searchEnd for a match.
 * The searchStart/searchEnd and cursor position must agree. */
{
bits32 arrayPos = searchStart;
/* We step through each base of the query */
// uglyf("qDna=%s\n", qDna);
for (; cursor<qSize; ++cursor)
    {
    bits32 nextOffset = traverseArray[arrayPos];
    DNA qBase = qDna[cursor];
    DNA tBase = tDna[suffixArray[arrayPos]+cursor];
    // uglyf("cursor=%d, qBase/tBase=%c/%c, arrayPos=%d\n", cursor, qBase, tBase, arrayPos);

    /* Skip to next matching base. */
    if (qBase != tBase)
        {
	bits32 nextPos = arrayPos;
	// uglyf("mismatch\n");
	for (;;)
	    {
	    if ((nextPos += nextOffset) >= searchEnd)
		{
		finalSearch(tDna, suffixArray, searchStart, arrayPos, qDna, qSize, 
			cursor-(arrayPos-searchStart), pHitList);
		return;   /* Ran through all variations of letters at this position. */
		}
	    nextOffset = traverseArray[nextPos];
	    tBase = tDna[suffixArray[nextPos]+cursor];
	    // uglyf("  %c at %d\n", tBase, nextPos);
	    if (qBase == tBase)
		{
		// uglyf("   match %c at %d\n", tBase, nextPos);
		searchStart = arrayPos = nextPos;
		break;
		}
	    } 
	}
    searchEnd = arrayPos + nextOffset;  

    /* We are going to advance to next position in query and in array.
     * This will only possibly yield a match if the next item in the suffix
     * array has a prefix that matches the current position up to our current
     * offset.  Happily this is encoded in the traverse array in a subtle way.
     * The step to find another letter at this position has to be greater than
     * one for the prefix to be shared. */
    if (nextOffset <= 1)
	{
	finalSearch(tDna, suffixArray, searchStart, searchEnd, qDna, qSize, 
		cursor - (arrayPos-searchStart), pHitList);
	return;  /* No match since prefix of next position doesn't match us. */
	}
    ++arrayPos;
    }
/* If we got here we matched the whole query sequence, and actually there's multiple
 * matches to it.  It's actually rare to get to here on query sequences larger than
 * 20 bases or so. */
finalSearch(tDna, suffixArray, searchStart, searchEnd, qDna, qSize,
	qSize - (arrayPos-searchStart-1), pHitList);  // TODO - check -1
}

int itsaDnaToBinary(DNA *dna, int size)
/* Convert dna to binary representation. */
{
int i;
int val = 0;
for (i=0; i<size; ++i)
    {
    val <<= 2;
    val += baseToVal[(int)dna[i]];
    }
return val;
}

void itsaExactFind(struct itsa *itsa, DNA *qDna, int qSize, struct slInt **pHitList)
/* Search indexed traversable suffix tree for exact matches. */
{
int slot = itsaDnaToBinary(qDna, 13);
bits32 searchStart = itsa->index13[slot];
if (searchStart != 0)
    {
    searchStart -= 1;	/* Pesky thing to keep 0 meaning no data in slot. */
    itsaFindWithin(itsa->allDna, itsa->array, itsa->traverse, qDna, qSize,
    	itsa->cursors13[slot], searchStart, searchStart + itsa->traverse[searchStart], pHitList);
    }
}

int itsaCountIdenticalPrefix(DNA *dna, int prefixSize, bits32 *array, bits32 arraySize)
/* Count up number of places in a row in array, where the referenced DNA is the
 * same up to prefixSize bases.  You do this a lot since generally the suffix tree
 * search just gives you the first place in the array that matches. */
{
bits32 i;
DNA *first = dna + array[0];
for (i=1; i<arraySize; ++i)
    {
    if (memcmp(first, dna+array[i], prefixSize) != 0)
        break;
    }
return i;
}



void itsaFuzzyFind(struct itsa *itsa,
	bits32 sliceStart, bits32 sliceEnd, int cursor,
	char *qDna, int qSize, int subCount, int maxSubs, 
	struct slInt **pHitList)
{
/* For now, actually do exact match */
itsaExactFind(itsa, qDna, qSize, pHitList);
}

void itsaFind(char *itsaFile, char *queryFile, char *outputFile)
/* itsaFind - Find sequence by searching indexed traversable suffix array.. */
{
struct itsa *itsa = itsaRead(itsaFile, mmap);
uglyTime("Loaded %s", itsaFile);
struct dnaLoad *qLoad = dnaLoadOpen(queryFile);
bits32 arraySize = itsa->header->arraySize;
FILE *f = mustOpen(outputFile, "w");
struct dnaSeq *qSeq;
int queryCount = 0, hitCount = 0, missCount=0;

while ((qSeq = dnaLoadNext(qLoad)) != NULL)
    {
    struct slInt *hit, *hitList = NULL;
    verbose(2, "Processing %s\n", qSeq->name);
    toUpperN(qSeq->dna, qSeq->size);
    itsaFuzzyFind(itsa, 0, arraySize, 0, qSeq->dna, qSeq->size, 0, maxMismatch, &hitList);
    if (hitList != NULL)
	++hitCount;
    else
	++missCount;
    for (hit = hitList; hit != NULL; hit = hit->next)
	{
	bits32 hitIx = hit->val;
	bits32 count = itsaCountIdenticalPrefix(itsa->allDna, qSeq->size, 
		itsa->array + hitIx, arraySize - hitIx);
	bits32 i;
	if (count > maxRepeat)
	    {
	    }
	else
	    {
	    for (i=0; i<count; ++i)
		{
		fprintf(f, "%s hits offset %u\n", qSeq->name, itsa->array[hitIx+i]);
		fprintf(f, "q %s\n", qSeq->dna);
		fprintf(f, "t ");
		mustWrite(f, itsa->allDna + itsa->array[hitIx+i], qSeq->size);
		fprintf(f, "\n");
		}
	    }
	}
    ++queryCount;
    dnaSeqFree(&qSeq);
    }
uglyTime("Alignment");
verbose(1, "%d queries. %d hits (%5.2f%%). %d misses (%5.2f%%).\n", queryCount, 
    hitCount, 100.0*hitCount/queryCount, missCount, 100.0*missCount/queryCount);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
maxRepeat = optionInt("maxRepeat", maxRepeat);
maxMismatch = optionInt("maxMismatch", maxMismatch);
mmap = optionExists("mmap");
dnaUtilOpen();

/* Fill out baseToVal array - A is already done. */
baseToVal[(int)'C'] = baseToVal[(int)'c'] = ITSA_C;
baseToVal[(int)'G'] = baseToVal[(int)'g'] = ITSA_G;
baseToVal[(int)'T'] = baseToVal[(int)'t'] = ITSA_T;

itsaFind(argv[1], argv[2], argv[3]);
return 0;
}
