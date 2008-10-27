/* sufxFind - Find sequence by searching suffix array.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "sufx.h"

static char const rcsid[] = "$Id: sufxFind.c,v 1.3 2008/10/27 06:26:24 kent Exp $";

boolean mmap;
int maxMismatch = 2;
int maxRepeat = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "sufxFind - Find sequence by searching suffix array.\n"
  "usage:\n"
  "   sufxFind target.sufx query.fa output\n"
  "options:\n"
  "   -maxRepeat=N  - maximum number of alignments to output on one query sequence. Default %d\n"
  "   -maxMismatch - maximum number of mismatches allowed.  Default %d. NOT IMPLEMENTED\n"
  "   -mmap - Use memory mapping. Faster just a few reads, but much slower for many reads\n"
  , maxRepeat, maxMismatch
  );
}

static struct optionSpec options[] = {
   {"maxRepeat", OPTION_INT},
   {"maxMismatch", OPTION_INT},
   {"mmap", OPTION_BOOLEAN},
   {NULL, 0},
};

int sufxCountIdenticalPrefix(DNA *dna, int tileSize, bits32 *array, int arraySize)
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

void sufxFindExact(DNA *tDna, bits32 *suffixArray, bits32 *traverseArray, int arraySize,
	DNA *qDna, int qSize, struct slInt **pHitList)
/* Search for all exact matches to qDna in suffix array.  Return them in pHitList. */
{
int qDnaOffset, tDnaOffset;
bits32 arrayPos = 0;
/* We step through each base of the query */
for (qDnaOffset=0; qDnaOffset<qSize; ++qDnaOffset)
    {
    DNA qBase = qDna[qDnaOffset];
    /* Loop until we find a matching base in the target at this position - up to 4 times. */
    for (;;)
        {
	tDnaOffset = suffixArray[arrayPos];
	DNA tBase = tDna[tDnaOffset+qDnaOffset];
	if (tBase == qBase)
	    break;
	int nextOffset = traverseArray[arrayPos];
	if (nextOffset == 0)
	    return;	/* End of the line, no match. */
	arrayPos += nextOffset;
	}

    /* Check to see if rest of query string matches current position, if so we're done. */
    int qNext = qDnaOffset+1;
    if (qNext == qSize || memcmp(qDna+qNext, tDna+tDnaOffset+qNext, qSize-qNext) == 0)
        {
	/* Got hit!  In fact it may be first of many matching hits that start at arrayPos. 
	 * We'll take of this later.  Keeps data structures lighter weight not to repeat
	 * all of these hits at this point though. */
	struct slInt *hit = slIntNew(arrayPos);
	slAddHead(pHitList, hit);
	return;
	}

    /* Otherwise just move forward in array one. */
    arrayPos += 1;
    if (memcmp(qDna, tDna+suffixArray[arrayPos], qDnaOffset+1) != 0)
       return;		/* Prefix changed, no match. */
    }
}


void sufxFind(char *sufxFile, char *queryFile, char *outputFile)
/* sufxFind - Find sequence by searching suffix array.. */
{
struct sufx *sufx = sufxRead(sufxFile, mmap);
struct dnaLoad *qLoad = dnaLoadOpen(queryFile);
int arraySize = sufx->header->arraySize;
FILE *f = mustOpen(outputFile, "w");
struct dnaSeq *qSeq;
int qSeqCount = 0;

while ((qSeq = dnaLoadNext(qLoad)) != NULL)
    {
    struct slInt *hit, *hitList = NULL;
    verbose(2, "Processing %s\n", qSeq->name);
    toUpperN(qSeq->dna, qSeq->size);
    sufxFindExact(sufx->allDna, sufx->array, sufx->traverse, arraySize, qSeq->dna, qSeq->size, &hitList);
    for (hit = hitList; hit != NULL; hit = hit->next)
	{
	int hitIx = hit->val;
	int count = sufxCountIdenticalPrefix(sufx->allDna, qSeq->size, 
		sufx->array + hitIx, arraySize - hitIx);
	int i;
	if (count > maxRepeat)
	    {
	    }
	else
	    {
	    for (i=0; i<count; ++i)
		{
		fprintf(f, "%s hits offset %d\n", qSeq->name, sufx->array[hitIx+i]);
		fprintf(f, "q %s\n", qSeq->dna);
		fprintf(f, "t ");
		mustWrite(f, sufx->allDna + sufx->array[hitIx+i], qSeq->size);
		fprintf(f, "\n");
		}
	    }
	}
    ++qSeqCount;
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
maxRepeat = optionInt("maxRepeat", maxRepeat);
maxMismatch = optionInt("maxMismatch", maxMismatch);
mmap = optionExists("mmap");
dnaUtilOpen();
sufxFind(argv[1], argv[2], argv[3]);
return 0;
}
