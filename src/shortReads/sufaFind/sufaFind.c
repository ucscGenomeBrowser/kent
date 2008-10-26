/* sufaFind - Find sequence by searching suffix array.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "sufa.h"

static char const rcsid[] = "$Id: sufaFind.c,v 1.5 2008/10/26 20:10:18 kent Exp $";

boolean mmap;
int maxMismatch = 2;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sufaFind - Find sequence by searching suffix array.\n"
  "usage:\n"
  "   sufaFind target.sufa query.fa output\n"
  "options:\n"
  "   -mmap - Use memory mapping. Faster just a few reads, but much slower for many reads\n"
  "   -maxMismatch - maximum number of mismatches allowed.  Default %d\n"
  , maxMismatch
  );
}

static struct optionSpec options[] = {
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
++upperBoundCount;
int startIx=0, endIx=arraySize-1, midIx;
int offset;
queryDna += prefixSize;
querySize -= prefixSize;
for (;;)
    {
    ++binaryBranches;
    if (startIx == endIx)
	return startIx;
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

void sufaFindOneOff(char *allDna, bits32 *array, int arraySize,
	int sliceOffset, int sliceSize, 
	char *prefix, int prefixSize,
	char *query, int querySize, int subCount, int maxSubs, 
	struct slInt **pHitList)
/* Find all array position that match off by one.  
 *    This is a recursive function*/
{
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
    	query, querySize, prefixSize) + sliceOffset;
    int offset = array[ix];
       {
       char *t = allDna + offset;
       char *q = query;
       }
    if (memcmp(allDna+offset+prefixSize, query+prefixSize, querySize-prefixSize) == 0)
	{
	struct slInt *hit = slIntNew(ix);
	slAddHead(pHitList, hit);
	}
    return;
    }

/* If not at end, then break up suffix array into quadrants, each starting with the
 * prefix with four varients for each of the bases. */
int quadrantIx;
int quadrants[5];
for (quadrantIx=0; quadrantIx<4; ++quadrantIx)
    {
    prefix[prefixSize] = bases4[quadrantIx];
    quadrants[quadrantIx] = sufaUpperBound(allDna, array+sliceOffset, sliceSize, 
    	prefix, prefixSize+1, prefixSize) + sliceOffset;
    }
quadrants[4] = sliceOffset + sliceSize;
int quadrantEnd = quadrants[0];

/* Further explore within each of the non-empty quadrants */
char queryBase = query[prefixSize]; 
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
	sufaFindOneOff(allDna, array, arraySize, quadrantStart, quadrantSize, 
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
int qSeqCount = 0;
while ((qSeq = dnaLoadNext(qLoad)) != NULL)
    {
    struct slInt *hit, *hitList = NULL;
    verbose(2, "Processing %s\n", qSeq->name);
    toUpperN(qSeq->dna, qSeq->size);
    char *prefix = needMem(qSeq->size+1);
    sufaFindOneOff(sufa->allDna, sufa->array, arraySize, 0, arraySize, prefix, 0,
        qSeq->dna, qSeq->size, 0, maxMismatch, &hitList);
    if (hitList == NULL)
        fprintf(f, "%s miss\n", qSeq->name);
    else
	{
	for (hit = hitList; hit != NULL; hit = hit->next)
	    {
	    int hitIx = hit->val;
	    int count = sufaCountIdenticalPrefix(sufa->allDna, qSeq->size, 
		    sufa->array + hitIx, arraySize - hitIx);
	    int i;
	    for (i=0; i<count; ++i)
		fprintf(f, "%s hits offset %d\n", qSeq->name, sufa->array[hitIx+i]);
	    }
	}
    ++qSeqCount;
    dnaSeqFree(&qSeq);
    }
carefulClose(&f);
verbose(1, "%d queries, %d binary searches, %ld binary branches\n", qSeqCount, upperBoundCount,
	binaryBranches);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
mmap = optionExists("mmap");
maxMismatch = optionInt("maxMismatch", maxMismatch);
dnaUtilOpen();
sufaFind(argv[1], argv[2], argv[3]);
return 0;
}
