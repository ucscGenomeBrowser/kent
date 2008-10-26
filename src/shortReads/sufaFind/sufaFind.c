/* sufaFind - Find sequence by searching suffix array.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "sufa.h"

static char const rcsid[] = "$Id: sufaFind.c,v 1.1 2008/10/26 03:27:10 kent Exp $";

boolean mmap;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sufaFind - Find sequence by searching suffix array.\n"
  "usage:\n"
  "   sufaFind target.sufa query.fa output\n"
  "options:\n"
  "   -mmap - Use memory mapping. Faster just a few reads, but much slower for many reads\n"
  );
}

static struct optionSpec options[] = {
   {"mmap", OPTION_BOOLEAN},
   {NULL, 0},
};

int sufaUpperBound(char *allDna, bits32 *array, int arraySize, char *queryDna, int querySize)
/* Find something that matches query, otherwise the place it should be inserted in array. */
{
int startIx=0, endIx=arraySize-1, midIx;
int offset;
for (;;)
    {
    if (startIx == endIx)
	return startIx;
    midIx = ((startIx + endIx)>>1);
    offset = array[midIx];
    if (memcmp(allDna+offset, queryDna, querySize) < 0)
        startIx = midIx+1;
    else
        endIx = midIx;
    }
}

int sufaBinarySearch(char *allDna, bits32 *array, int arraySize, char *queryDna, int querySize)
/* Find first index in array that matches query, or -1 if none such. */
{
int ix = sufaUpperBound(allDna, array, arraySize, queryDna, querySize);
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


void sufaFindOneOff(char *allDna, bits32 *array, int arraySize, char *queryDna, int querySize,
	struct slInt **pHitList)
/* Find all array position that match off by one. */
{
}


void sufaFind(char *sufaFile, char *queryFile, char *outputFile)
/* sufaFind - Find sequence by searching suffix array.. */
{
struct sufa *sufa = sufaRead(sufaFile, mmap);
struct dnaLoad *qLoad = dnaLoadOpen(queryFile);
FILE *f = mustOpen(outputFile, "w");
struct dnaSeq *qSeq;
while ((qSeq = dnaLoadNext(qLoad)) != NULL)
    {
    verbose(2, "Processing %s\n", qSeq->name);
    toUpperN(qSeq->dna, qSeq->size);
    int exactIx = sufaBinarySearch(sufa->allDna, sufa->array, sufa->header->basesIndexed, 
        qSeq->dna, qSeq->size);
    if (exactIx < 0)
        fprintf(f, "%s miss\n", qSeq->name);
    else
	{
	int count = sufaCountIdenticalPrefix(sufa->allDna, qSeq->size, 
		sufa->array + exactIx, sufa->header->basesIndexed - exactIx);
	int i;
	for (i=0; i<count; ++i)
	    fprintf(f, "%s hits offset %d\n", qSeq->name, sufa->array[exactIx+i]);
	}
    dnaSeqFree(&qSeq);
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
mmap = optionExists("mmap");
dnaUtilOpen();
sufaFind(argv[1], argv[2], argv[3]);
return 0;
}
