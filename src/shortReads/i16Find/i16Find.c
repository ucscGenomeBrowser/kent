/* i16Find - Find sequence by searching with 16 base index.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "dnaLoad.h"
#include "verbose.h"
#include "i16.h"

static char const rcsid[] = "$Id: i16Find.c,v 1.1 2008/10/31 02:54:36 kent Exp $";

boolean mmap;
boolean noOverflow;
int maxMismatch = 0;
int maxRepeat = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "i16Find - Find sequence by searching with 16 base index.\n"
  "   i16Find target.i16 query.fa output\n"
  "options:\n"
  "   -maxRepeat=N  - maximum number of alignments to output on one query sequence. Default %d\n"
  "   -mmap - Use memory mapping. Faster just a few reads, but much slower for many reads\n"
  "   -maxMismatch - maximum number of mismatches allowed.  Default %d. NOT IMPLEMENTED\n"
  "   -noOverflow - Don't explore index overflow regions (usually just repeats anyway)\n"
  , maxRepeat, maxMismatch
  );
}

static struct optionSpec options[] = {
   {"maxRepeat", OPTION_INT},
   {"mmap", OPTION_BOOLEAN},
   {"maxMismatch", OPTION_INT},
   {"noOverflow", OPTION_BOOLEAN},
   {NULL, 0},
};

int i16OverflowIx(struct i16 *i16, bits32 slot)
/* Find index of hex that matches val, or -1 if none such. */
{
int startIx=0, endIx=i16->header->overflowSlotCount-1, midIx;
bits32 *slots = i16->overflowSlots;
bits32 posVal;

for (;;)
    {
    if (startIx == endIx)
        {
	posVal = slots[startIx];
	if (posVal == slot)
	    return startIx;
	else
	    return -1;
	}
    midIx = ((startIx + endIx)>>1);
    posVal = slots[midIx];
    if (posVal < slot)
        startIx = midIx+1;
    else
        endIx = midIx;
    }
}

bits64 slotsExplored = 0;

void i16FindExact(struct i16 *i16, DNA *qDna, int qSize, struct slInt **pHitList)
/* Put exact hits on hit list. */
{
bits32 slot = packDna16(qDna);
int slotSize = i16->slotSizes[slot];
bits32 *slotList;

if (slotSize == i16OverflowCount)
    {
    if (noOverflow)
	{
	return;
	}
    else
        {
	int overflowIx = i16OverflowIx(i16, slot);
	assert(overflowIx >= 0);
	slotSize = i16->overflowSizes[overflowIx];
	slotList = i16->overflowOffsets + i16->overflowStarts[overflowIx];
	}
    }
else
    {
    slotList = i16->slotOffsets + i16->slotStarts[slot];
    }
int i;
slotsExplored += slotSize;
for (i=0; i<slotSize; ++i)
    {
    bits32 tOffset = slotList[i];
    DNA *tDna = i16->allDna + tOffset;
    if (memcmp(qDna+16, tDna+16, qSize-16) == 0)
	{
	struct slInt *hit = slIntNew(tOffset);
	slAddHead(pHitList, hit);
	}
    }
}

void i16Find(char *i16File, char *queryFile, char *outputFile)
/* i16Find - Find sequence by searching indexed traversable suffix array.. */
{
struct i16 *i16 = i16Read(i16File, mmap);
verboseTime(1, "Loaded %s", i16File);
struct dnaLoad *qLoad = dnaLoadOpen(queryFile);
FILE *f = mustOpen(outputFile, "w");
struct dnaSeq *qSeq;
int queryCount = 0, hitCount = 0, missCount=0;

while ((qSeq = dnaLoadNext(qLoad)) != NULL)
    {
    struct slInt *hit, *hitList = NULL;
    verbose(2, "Processing %s\n", qSeq->name);
    toUpperN(qSeq->dna, qSeq->size);
    i16FindExact(i16, qSeq->dna, qSeq->size, &hitList);
    if (hitList != NULL)
	{
	int count = slCount(hitList);
	if (count < maxRepeat)
	    {
	    for (hit = hitList; hit != NULL; hit = hit->next)
		{
		bits32 offset = hit->val;
		fprintf(f, "%s hits offset %u\n", qSeq->name, offset);
		fprintf(f, "q %s\n", qSeq->dna);
		fprintf(f, "t ");
		mustWrite(f, i16->allDna + offset, qSeq->size);
		fprintf(f, "\n");
		}
	    }
	++hitCount;
	}
    else
	++missCount;
    ++queryCount;
    dnaSeqFree(&qSeq);
    }
verboseTime(1, "Alignment");
verbose(1, "%d queries. %d hits (%5.2f%%). %d misses (%5.2f%%), %lld slotsExplored.\n", queryCount, 
    hitCount, 100.0*hitCount/queryCount, missCount, 100.0*missCount/queryCount,
    slotsExplored);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
verboseTime(1, NULL);
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
maxRepeat = optionInt("maxRepeat", maxRepeat);
maxMismatch = optionInt("maxMismatch", maxMismatch);
mmap = optionExists("mmap");
noOverflow = optionExists("noOverflow");
dnaUtilOpen();
i16Find(argv[1], argv[2], argv[3]);
return 0;
}
