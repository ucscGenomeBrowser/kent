/* bbTest - a program to test bigBed is compatible with normal bed. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "options.h"
#include "localmem.h"
#include "bits.h"
#include "basicBed.h"


int numTests = 1000;
char *testChrom = NULL;
int seed = 0;
boolean bed12 = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bbTest - a program to test bigBed is compatible with normal bed\n"
  "usage:\n"
  "   bbTest chrom.sizes file.bed file.bb\n"
  "arguments:\n"
  "   chrom.sizes  file with chrom names and sizes\n"
  "   file.bed     bed3 file\n"
  "   file.bb      bigBed file\n"
  "options:\n"
  "   -bed12       read a bed12 instead of bed3\n"
  "   -seed=N      use N for seed (default time())\n"
  "   -numTests=N  do N samples (default %d) \n"
  "   -chrom=chr1  test on just chr1 (default: all chroms in chrom.sizes\n"
  ,numTests
  );
}

static struct optionSpec options[] = {
   {"bed12", OPTION_BOOLEAN},
   {"numTests", OPTION_INT},
   {"seed", OPTION_INT},
   {"chrom", OPTION_STRING},
   {NULL, 0},
};


struct bedHead 
{
struct bed *list;
};


struct hash *readBed(char *fileName, int nFields)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[20];
int wordsRead;
struct bedHead *head;
struct hash *headHash = newHash(10);

while( (wordsRead = lineFileChopNext(lf, words, sizeof(words)/sizeof(char *)) ))
    {
    struct bed *bed;

    if (wordsRead < nFields)
	errAbort("bed file %s has %d fields on line %d which is less than %d\n",
	    wordsRead, lf->lineIx, nFields);

    bed = bedLoadN(words, wordsRead);

    if ((head = hashFindVal(headHash, words[0])) == NULL)
	{
	AllocVar(head);
	hashAdd(headHash, words[0], head);
	}

    slAddTail(&head->list, bed);
    }

lineFileClose(&lf);

return headHash;
}

extern int ptToInt(void *pt);

void getRegion(struct hash *chromHash, int numChroms, char **chrom, int *size, int *start, int *end)
{
if (testChrom)
    {
    *size = hashIntVal(chromHash, testChrom);
    *chrom = testChrom;
    }
else
    {
    int chromOff = rand() % numChroms;
    struct hashCookie cookie = hashFirst(chromHash);
    struct hashEl *hel = hashNext(&cookie);

    for(; chromOff--; hel = hashNext(&cookie))
	;

    *size = ptToInt(hel->val);
    *chrom = hel->name;
    }
*start = rand() % (*size);
*end = rand() % (*size);

if (*start > *end)
    {
    int temp = *start;
    *start = *end;
    *end = temp;
    }
verbose(2, "getRegion: %s:%d-%d\n", *chrom, *start, *end);
}


void setIntersection(Bits *bits, int s1, int e1, int s2, int e2)
{
if (positiveRangeIntersection(s1, e1, s2, e2))
    {
    verbose(3, "intersect bed has %d %d select is %d %d\n", 
	s1, e1, s2, e2);
    int interStart, interEnd;

    interStart = s1;
    if (s2 > interStart)
	interStart = s2;

    interEnd = e1;
    if (e2 < interEnd)
	interEnd = e2;
    verbose(3, "bedSet %d %d\n", interStart, interEnd);

    bitSetRange(bits, interStart, interEnd - interStart);
    }
}


void doSetBedBits(Bits *bits, struct bed *bed, int start, int end)
{
for(; bed; bed = bed->next)
    {
    if (bed->blockCount > 1)
	{
	int *starts = bed->chromStarts;
	int *lastStart = &starts[bed->blockCount];
	int *size = bed->blockSizes;
	int count = 0;

	for(; starts < lastStart; starts++, size++)
	    {
	    setIntersection(bits, bed->chromStart + *starts, 
		bed->chromStart + *starts + *size, start, end);
	    }
	}
    else
	setIntersection(bits, bed->chromStart, bed->chromEnd, start, end);
    }
}

void setBigBedBits(char *fileName, Bits *bits, char *chrom, int start, int end)
{
struct lm *lm = lmInit(0);
struct bbiFile *bbi = bigBedFileOpen(fileName);
struct bigBedInterval *bbiList = bigBedIntervalQuery(bbi, chrom, start, end, 0, lm);
struct bigBedInterval *bbInt = bbiList;

for(; bbInt; bbInt = bbInt->next)
    {
    //verbose(2, "bigBed set %d %d\n", bbInt->start, bbInt->end);
    if (bbInt->rest == NULL)
	setIntersection(bits, bbInt->start, bbInt->end, start, end);
    else
	{
	char *bedRow[32];
	char startBuf[16], endBuf[16];
	int fieldCount = 12;

	bigBedIntervalToRow(bbInt, chrom, startBuf, endBuf, 
	    bedRow, ArraySize(bedRow));
	struct bed *bed = bedLoadN(bedRow, fieldCount);
	doSetBedBits(bits, bed, start, end);
	}
    }

bbiFileClose(&bbi);
lmCleanup(&lm);
}

void setBedBits(struct hash *bedHash, Bits *bits, char *chrom, int start, int end)
{
struct bedHead *head = hashFindVal(bedHash, chrom);
if (head == NULL)
    return;

struct bed *bed = head->list;

doSetBedBits(bits, bed, start, end);
}

void compareBits(Bits *bbBits, Bits *bedBits, int size)
{

int bbCount = bitCountRange(bbBits, 0, size);
int bedCount = bitCountRange(bedBits, 0, size);

verbose(2, "bbCount %d bedCount %d\n", bbCount, bedCount);
if (bbCount != bedCount)
    errAbort("set count differs bed %d bb %d\n", bedCount, bbCount);

bitNot(bbBits, size);
bitAnd(bbBits, bedBits, size);
int off;
if ((off = bitFindSet(bbBits, 0, size)) != size)
    {
    errAbort("different at offset %d\n",off);
    }
}


void bbTest(char *sizesName, char *bedName, char *bbName)
/* bbTest - a program to test bigBed is compatible with normal bed. */
{
struct hash *chromSizesHash = bbiChromSizesFromFile(sizesName);
int numChroms = hashNumEntries(chromSizesHash);
int ii;
int start, end;
char *chrom;
int size;
struct hash *bedHash = readBed(bedName, bed12 ? 12 : 3);
int maxSize = 10;

Bits *bbBits = bitAlloc(maxSize);
Bits *bedBits = bitAlloc(maxSize);

for(ii=0; ii < numTests; ii++)
    {
    getRegion(chromSizesHash, numChroms, &chrom, &size, &start, &end);

    verbose(3, "size %d maxSize %d\n", size, maxSize);
    if (size > maxSize)
	{
	verbose(2, "growing bits\n");
	bbBits = bitRealloc(bbBits, maxSize, size);
	bedBits = bitRealloc(bedBits, maxSize, size);
	maxSize = size;
	}

    bitClear(bbBits, size);
    bitClear(bedBits, size);

    setBigBedBits(bbName, bbBits, chrom, start, end);

    setBedBits(bedHash, bedBits, chrom, start, end);

    compareBits(bbBits, bedBits, size);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
seed = optionInt("seed", time(NULL));
srand(seed);
printf("seed is %d\n", seed);
bed12 = optionExists("bed12");
numTests = optionInt("numTests", numTests);
testChrom = optionVal("chrom", testChrom);
bbTest(argv[1], argv[2], argv[3]);
return 0;
}
