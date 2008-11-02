/* sufaFind - Find sequence by searching suffix array with a standard binary search.
 * Theoretically allowes mismatches, and did at one point, but is broken now. */
/* Copyright 2008 Jim Kent all rights reserved. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "sufa.h"

static char const rcsid[] = "$Id: sufaFind.c,v 1.9 2008/11/02 20:18:05 kent Exp $";

// boolean uglyOne;

boolean mmap;
int maxMismatch = 2;
int maxRepeat = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sufaFind - Find sequence by searching suffix array.\n"
  "usage:\n"
  "   sufaFind target.sufa query.fa output\n"
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

int upperBoundCount = 0;
long binaryBranches = 0;

int sufaUpperBound(char *allDna, bits32 *array, int arraySize, char *queryDna, int querySize,
	int prefixSize)
/* Find something that matches query, otherwise the place it should be inserted in array. */
{
#ifdef DEBUG
if (uglyOne) {
	     char *q = cloneStringZ(queryDna, querySize);
	     tolowers(q);
	     toUpperN(q+prefixSize, querySize-prefixSize);
	     uglyf("sufaUpperBound queryDna=%s querySize=%d prefixSize=%d arraySize=%d\n", q, querySize, prefixSize, arraySize);
             }
#endif /* DEBUG */
++upperBoundCount;
int startIx=0, endIx=arraySize-1, midIx;
int offset;
queryDna += prefixSize;
querySize -= prefixSize;
for (;;)
    {
    ++binaryBranches;
    if (startIx == endIx)
	{
	offset = array[startIx];
	if (memcmp(allDna+offset+prefixSize, queryDna, querySize) == 0)
	    return startIx;
	else
	    return -1;
	}
    midIx = ((startIx + endIx)>>1);
    offset = array[midIx];
    if (memcmp(allDna+offset+prefixSize, queryDna, querySize) < 0)
        startIx = midIx+1;
    else
        endIx = midIx;
    }
}

int sufaBinarySearch(char *allDna, bits32 *array, int arraySize, char *queryDna, int querySize)
/* Find first index in array that matches query, or -1 if none such. */
{
int ix = sufaUpperBound(allDna, array, arraySize, queryDna, querySize, 0);
int offset = array[ix];
if (memcmp(allDna+offset, queryDna, querySize) == 0)
    return ix;
else
    return -1;
}

int sufaCountIdenticalPrefix(DNA *dna, int tileSize, bits32 *array, int arraySize)
/* Count up number of places in a row in array, where the referenced DNA is the
 * same up to tileSize bases.  You do this a lot since generally the suffix tree
 * search just gives you the first place in the array that matches. */
{
int i;
DNA *first = dna + array[0];
for (i=1; i<arraySize; ++i)
    {
    if (memcmp(first, dna+array[i], tileSize) != 0)
        break;
    }
return i;
}


char *bases4 = "ACGT";

void dumpDnaAtIx(char *allDna, bits32 *array, int ix, int size, FILE *f)
/* Write out ix, and first size letters of dna at that ix. */
{
fprintf(f, "%d ", ix);
mustWrite(f, allDna + array[ix], size);
fprintf(f, "\n");
}

void sufaFindOneOff(int depth, char *allDna, bits32 *array, int arraySize,
	int sliceOffset, int sliceSize, 
	char *prefix, int prefixSize,
	char *query, int querySize, int subCount, int maxSubs, 
	struct slInt **pHitList)
/* Find all array position that match off by one.  
 *    This is a recursive function*/
{
// uglyOne = sameString(prefix, "TCCAGA");
// spaceOut(uglyOut, depth*2);
// uglyf("sufaFindOneOff prefix/suffix %s/%s subCount=%d sliceOffset=%d sliceSize=%d\n", prefix, query+prefixSize, subCount, sliceOffset, sliceSize);
// if (uglyOne)
    // uglyf(">>Got you %s\n", prefix);
/* Start recursion with end conditions.  First is that we've matched the whole thing. */
if (prefixSize == querySize)
    {
    struct slInt *hit = slIntNew(sliceOffset);
    slAddHead(pHitList, hit);
    return;
    }
/* If we have used up all of our substitutions, then look for an exact match to
 * what is left and add it if it exists. This is another end condition. */
if (subCount == maxSubs)
    {
    int ix = sufaUpperBound(allDna, array+sliceOffset, sliceSize, 
    	query, querySize, prefixSize);
    if (ix >= 0)
	{
	ix += sliceOffset;
	int offset = array[ix];
	// spaceOut(uglyOut, depth*2);
	// uglyf("--ix=%d offset=%d  memcmp ", ix, offset);
	// mustWrite(uglyOut, allDna+offset+prefixSize, querySize-prefixSize);
	// uglyf(" ");
	// mustWrite(uglyOut, query+prefixSize, querySize-prefixSize);
	// uglyf("\n");
	struct slInt *hit = slIntNew(ix);
	slAddHead(pHitList, hit);
	}
    return;
    }

/* GT^GG
 * GT A
 * GT C
 * GT G
 * GT T */
/* If not at end, then break up suffix array into quadrants, each starting with the
 * prefix with four varients for each of the bases. */
int quadrantIx;
int quadrants[5];
char queryBase = query[prefixSize]; 
// if (uglyOne) uglyf("sloceOffset %d, sliceSize %d\n", sliceOffset, sliceSize);
for (quadrantIx=0; quadrantIx<4; ++quadrantIx)
    {
    prefix[prefixSize] = bases4[quadrantIx];
    int bounds = sufaUpperBound(allDna, array+sliceOffset, sliceSize, 
    	prefix, prefixSize+1, prefixSize);
    // if (uglyOne) uglyf("quad %d, bounds=%d\n", quadrantIx, bounds);
    if (bounds >= 0) bounds += sliceOffset;
    quadrants[quadrantIx] = bounds;
    }
quadrants[4] = sliceOffset + sliceSize;
prefix[prefixSize] = 0;

/* Make misses just empty. */
for (quadrantIx=0; quadrantIx < 4; ++quadrantIx)
    {
    if (quadrants[quadrantIx] == -1)
        {
	int realIx;
	for (realIx = quadrantIx+1; realIx <= 4; ++realIx)
	    {
	    if (quadrants[realIx] != -1)
		{
	        quadrants[quadrantIx] = quadrants[realIx];
		break;
		}
	    }
	}
    // if (uglyOne) uglyf("%s %c bounds=%d\n", prefix, bases4[quadrantIx], quadrants[quadrantIx]);
    }
// if (uglyOne) uglyf("%s $ bounds=%d\n", prefix, quadrants[4]);

/* Further explore within each of the non-empty quadrants */
int quadrantEnd = quadrants[0];
for (quadrantIx=0; quadrantIx < 4; ++quadrantIx)
    {
    int quadrantStart = quadrantEnd;
    quadrantEnd = quadrants[quadrantIx+1];
    int quadrantSize = quadrantEnd - quadrantStart;
    if (quadrantSize != 0)
        {
	char quadBase = bases4[quadrantIx];
	prefix[prefixSize] = quadBase;
	int extraSub = (queryBase == quadBase ? 0 : 1);
	sufaFindOneOff(depth+1, allDna, array, arraySize, quadrantStart, quadrantSize, 
		prefix, prefixSize+1, query, querySize, subCount+extraSub, maxSubs,
		pHitList);
	prefix[prefixSize+1] = 0;
	}
    }
}



void sufaFind(char *sufaFile, char *queryFile, char *outputFile)
/* sufaFind - Find sequence by searching suffix array.. */
{
struct sufa *sufa = sufaRead(sufaFile, mmap);
struct dnaLoad *qLoad = dnaLoadOpen(queryFile);
int arraySize = sufa->header->arraySize;
FILE *f = mustOpen(outputFile, "w");
struct dnaSeq *qSeq;
int queryCount = 0, hitCount = 0, missCount=0;
while ((qSeq = dnaLoadNext(qLoad)) != NULL)
    {
    struct slInt *hit, *hitList = NULL;
    verbose(2, "Processing %s\n", qSeq->name);
    toUpperN(qSeq->dna, qSeq->size);
    char *prefix = needMem(qSeq->size+1);
    sufaFindOneOff(0, sufa->allDna, sufa->array, arraySize, 0, arraySize, prefix, 0,
        qSeq->dna, qSeq->size, 0, maxMismatch, &hitList);
    if (hitList != NULL)
	++hitCount;
    else
	++missCount;
    for (hit = hitList; hit != NULL; hit = hit->next)
	{
	int hitIx = hit->val;
	int count = sufaCountIdenticalPrefix(sufa->allDna, qSeq->size, 
		sufa->array + hitIx, arraySize - hitIx);
	int i;
	if (count > maxRepeat)
	    {
	    }
	else
	    {
	    for (i=0; i<count; ++i)
		{
		fprintf(f, "%s hits offset %d\n", qSeq->name, sufa->array[hitIx+i]);
		fprintf(f, "q %s\n", qSeq->dna);
		fprintf(f, "t ");
		mustWrite(f, sufa->allDna + sufa->array[hitIx+i], qSeq->size);
		fprintf(f, "\n");
		}
	    }
	}
    freeMem(prefix);
    ++queryCount;
    dnaSeqFree(&qSeq);
    }
carefulClose(&f);
verbose(1, "%d binary searches, %ld binary branches\n", upperBoundCount,
	binaryBranches);
verbose(1, "%d queries. %d hits (%5.2f%%). %d misses (%5.2f%%).\n", queryCount, 
    hitCount, 100.0*hitCount/queryCount, missCount, 100.0*missCount/queryCount);
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
sufaFind(argv[1], argv[2], argv[3]);
return 0;
}
