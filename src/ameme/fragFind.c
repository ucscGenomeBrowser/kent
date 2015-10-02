/* fragFind - fast way of finding short patterns in data. */

#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "ameme.h"
#include "fragFind.h"

static int oligoVal(DNA *oligo, int oligoSize)
/* Return oligo converted to a number - 2 bits per base.
 * returns -1 if oligo has 'N's or other non-nt characters. */
{
int i;
int acc = 0;
int baseVal;

for (i=0; i<oligoSize; ++i)
    {
    if ((baseVal = ntVal[(int)oligo[i]]) < 0)
        return -1;
    acc <<= 2;
    acc += baseVal;
    }
return acc;
}

static void unpackVal(int val, int oligoSize, char *out)
/* Unpack value from 2-bits-per-nucleotide to one character per. */
{
int i;
out[oligoSize] = 0;
for (i = oligoSize-1; i>=0; --i)
    {
    out[i] = valToNt[val&3];
    val>>=2;
    }
}

int *makeRcTable(int oligoSize)
/* Make a table for doing reverse complement of packed oligos. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int tableByteSize = tableSize * sizeof(int);
int *table = needLargeMem(tableByteSize);
char oligo[17];
int i;

for (i=0; i<tableSize; ++i)
    {
    unpackVal(i, oligoSize, oligo);
    reverseComplement(oligo, oligoSize);
    table[i] = oligoVal(oligo, oligoSize);
    }
return table;
}

static boolean masked(int *softMask, int size)
/* Return TRUE if mask is non-zero */
{
int i;
for (i=0;i<size;++i)
    if (softMask[i] != 0)
        return TRUE;
return FALSE;
}

static void makeOligoHistogram(char *fileName, struct seqList *seqList, 
    int oligoSize, int **retTable, int *retTotal)
/* Make up table of oligo occurences. Either pass in an FA file or a seqList.
 * (the other should be NULL). */
{
FILE *f = NULL;
int tableSize = (1<<(oligoSize+oligoSize));
int tableByteSize = tableSize * sizeof(int);
int *table = needLargeMem(tableByteSize);
struct dnaSeq *seq;
struct seqList *seqEl = seqList;
int *softMask = NULL;
int total = 0;

if (seqList == NULL)
    f = mustOpen(fileName, "rb");

memset(table, 0, tableByteSize);
for (;;)
    {
    DNA *dna;
    int size;
    int endIx;
    int i;
    int oliVal;
    if (seqList != NULL)
        {
        if (seqEl == NULL)
            break;
        seq = seqEl->seq;
        softMask = seqEl->softMask;
        seqEl = seqEl->next;
        }
    else
        {
        seq = faReadOneDnaSeq(f, "", TRUE);
        if (seq == NULL)
            break;
        }
    dna = seq->dna;
    size = seq->size;
    endIx = size-oligoSize;
    for (i=0; i<=endIx; ++i)
        {
        if (softMask == NULL || !masked(softMask+i, oligoSize) )
            {
            if ((oliVal = oligoVal(dna+i, oligoSize)) >= 0)
                {
                table[oliVal] += 1;
                ++total;
                }
            }
        }
    if (seqList == NULL)
        freeDnaSeq(&seq);
    }
carefulClose(&f);
*retTable = table;
*retTotal = total;
}

static int bestTableIx(int *table, int oligoSize, int *rcTable)
/* Return index of highest count in table. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int bestVal = -1;
int bestIx = 0;
int i;
int val;

if (rcTable != NULL)
    {
    for (i=0; i<tableSize; ++i)
        {
        if ((val = table[i] + table[rcTable[i]]) > bestVal)
            {
            bestVal = val;
            bestIx = i;
            }
        }
    }
else
    {
    for (i=0; i<tableSize; ++i)
        {
        if ((val = table[i]) > bestVal)
            {
            bestVal = val;
            bestIx = i;
            }
        }
    }
return bestIx;
}

static int fuzzValOne(int *table, int oligoSize, int *rcTable)
/* Return value of table entry with most number of 
 * matches that are off by no more than one. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int tableIx;
int bestVal = -0x3ffffff;

for (tableIx = 0; tableIx < tableSize; ++tableIx)
    {
    int acc = 0;
    int mask = 0x3;
    int maskedIx;
    int inc = 1;
    int baseIx;

    for (baseIx = 0; baseIx<oligoSize; ++baseIx)
        {
        maskedIx = (tableIx&(~mask));
        if (rcTable)
            {
            int mIx = maskedIx;
            acc += table[rcTable[mIx]];
            mIx += inc;
            acc += table[rcTable[mIx]];
            mIx += inc;
            acc += table[rcTable[mIx]];
            mIx += inc;
            acc += table[rcTable[mIx]];
            }
        acc += table[maskedIx];
        maskedIx += inc;
        acc += table[maskedIx];
        maskedIx += inc;
        acc += table[maskedIx];
        maskedIx += inc;
        acc += table[maskedIx];
        mask <<= 2;
        inc <<= 2;
        }
    if (acc > bestVal)
        {
        bestVal = acc;
        }
    }
return tableIx;
}

static int fuzzValTwo(int *table, int oligoSize, int *rcTable)
/* Return value of table entry with most number of 
 * matches that are off by no more than one. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int tableIx;
int bestVal = -0x3ffffff;
int bestIx = 0;

for (tableIx = 0; tableIx < tableSize; ++tableIx)
    {
    int acc = 0;
    int ixTwo;
    int maskTwo = 0x3;
    int incTwo = 1;
    int baseTwo;
    int j;
    for (baseTwo = 0; baseTwo<oligoSize; ++baseTwo)
        {
        ixTwo = (tableIx&(~maskTwo));
        for (j=0; j<4; ++j)
            {
            int ixOne;
            int preShift = baseTwo + baseTwo + 2;
            int maskOne = 0x3<<preShift;
            int incOne = 1<<preShift;
            int baseOne;
            for (baseOne = baseTwo+1; baseOne<oligoSize; ++baseOne)
                {
                ixOne = (ixTwo&(~maskOne));
                if (rcTable)
                    {
                    int mIx = ixOne;
                    acc += table[rcTable[mIx]];
                    mIx += incOne;
                    acc += table[rcTable[mIx]];
                    mIx += incOne;
                    acc += table[rcTable[mIx]];
                    mIx += incOne;
                    acc += table[rcTable[mIx]];
                    }
                acc += table[ixOne];
                ixOne += incOne;
                acc += table[ixOne];
                ixOne += incOne;
                acc += table[ixOne];
                ixOne += incOne;
                acc += table[ixOne];
                maskOne <<= 2;
                incOne <<= 2;
                }
            ixTwo += incTwo;
            }
        maskTwo <<= 2;
        incTwo <<= 2;
        }
    if (acc > bestVal)
        {
        bestVal = acc;
        bestIx = tableIx;
        }
    }
return bestIx;
}

static int fuzzValThree(int *table, int oligoSize, int *rcTable)
/* Return value of table entry with most number of 
 * matches that are off by no more than one. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int tableIx;
int bestVal = -0x3ffffff;
int bestIx = 0;

for (tableIx = 0; tableIx < tableSize; ++tableIx)
    {
    int acc = 0;
    
    int ixThree;
    int maskThree = 0x3;
    int incThree = 1;
    int baseThree;
    int k;
    for (baseThree=0; baseThree<oligoSize; ++baseThree)
        {
        ixThree = (tableIx&(~maskThree));
        for (k=0; k<4; ++k)
            {
            int ixTwo;
            int preShift = baseThree + baseThree + 2;
            int maskTwo = 0x3<<preShift;
            int incTwo = 1<<preShift;
            int baseTwo;
            int j;
            for (baseTwo = baseThree+1; baseTwo<oligoSize; ++baseTwo)
                {
                ixTwo = (ixThree&(~maskTwo));
                for (j=0; j<4; ++j)
                    {
                    int ixOne;
                    int preShift = baseTwo + baseTwo + 2;
                    int maskOne = 0x3<<preShift;
                    int incOne = 1<<preShift;
                    int baseOne;
                    for (baseOne = baseTwo+1; baseOne<oligoSize; ++baseOne)
                        {
                        ixOne = (ixTwo&(~maskOne));
                        if (rcTable)
                            {
                            int mIx = ixOne;
                            acc += table[rcTable[mIx]];
                            mIx += incOne;
                            acc += table[rcTable[mIx]];
                            mIx += incOne;
                            acc += table[rcTable[mIx]];
                            mIx += incOne;
                            acc += table[rcTable[mIx]];
                            }
                        acc += table[ixOne];
                        ixOne += incOne;
                        acc += table[ixOne];
                        ixOne += incOne;
                        acc += table[ixOne];
                        ixOne += incOne;
                        acc += table[ixOne];
                        maskOne <<= 2;
                        incOne <<= 2;
                        }
                    ixTwo += incTwo;
                    }
                }
            maskTwo <<= 2;
            incTwo <<= 2;
            ixThree += incThree;
            }
        maskThree <<= 2;
        incThree <<= 2;
        }
    if (acc > bestVal)
        {
        bestVal = acc;
        bestIx = tableIx;
        }
    }
return bestIx;
}

static int fuzzVal(int *table, int oligoSize, int mismatchesAllowed, boolean considerRc)
/* Return value of table entry with most number of 
 * matches that are off by no more than mismatchesAllowed. */
{
int *rcTable = NULL;
int ret = 0;
if (considerRc)
    rcTable = makeRcTable(oligoSize);
if (mismatchesAllowed == 0)
    ret =  bestTableIx(table, oligoSize, rcTable);
else if (mismatchesAllowed == 1)
    ret =  fuzzValOne(table, oligoSize, rcTable);
else if (mismatchesAllowed == 2)
    ret =  fuzzValTwo(table, oligoSize, rcTable);
else if (mismatchesAllowed == 3)
    ret =  fuzzValThree(table, oligoSize, rcTable);
else
    assert(FALSE);
freeMem(rcTable);
return ret;
}

static void normalizeTable(int *table, int oligoSize)
/* Normalize table so that all entries are between 0 and 1000000 */
{
int tableSize = (1<<(oligoSize+oligoSize));
int i = bestTableIx(table, oligoSize, NULL);
int maxVal = table[i];
double scaleBy = 1000000.0/maxVal;

for (i=0; i<tableSize; i += 1)
    {
    table[i] = round(scaleBy*table[i]);
    }
}

static void diffTables(int *a, int *b, int oligoSize)
/* Subtract table b from table a, leaving results in b. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int i;
for (i=0; i<tableSize; i += 1)
    {
    b[i] = a[i] - b[i];
    }
}

static boolean allGoodBases(DNA *oligo, int oligoSize)
/* Returns TRUE if all bases in oligo are nucleotides. */
{
int i;
for (i=0; i<oligoSize; ++i)
    if (ntVal[(int)oligo[i]] < 0)
        return FALSE;
return TRUE;
}

static int mismatchCount(DNA *a, DNA *b, int size)
/* Count up number of mismatches between a and b */
{
int count = 0;
int i;

for (i=0; i<size; ++i)
    if (a[i] != b[i])
        ++count;
return count;
}

static void makeProfile(DNA *oligo, int oligoSize, int mismatchesAllowed, 
    struct seqList *seqList, boolean considerRc, double profile[16][4])
/* Scan through file counting up things that match oligo to within
 * mismatch tolerance, and use these counts to build up a profile. */
{
int counts[16][4];
int total = 0;
double invTotal;
int i,j;
int seqCount = 0;
struct seqList *seqEl;
DNA rcOligo[17];

if (considerRc)
    {
    assert(oligoSize < sizeof(rcOligo));
    memcpy(rcOligo, oligo, oligoSize);
    reverseComplement(rcOligo, oligoSize);
    }

zeroBytes(counts, sizeof(counts));
for (seqEl = seqList; seqEl != NULL; seqEl = seqEl->next)
    {
    struct dnaSeq *seq = seqEl->seq;
    DNA *dna = seq->dna;
    int size = seq->size;
    int endIx = size-oligoSize;

    ++seqCount;
    for (i=0; i<=endIx; ++i)
        {
        DNA *target = dna+i;
        if (allGoodBases(target, oligoSize))
            {
            if (mismatchCount(oligo, target, oligoSize) <= mismatchesAllowed)
                {
                ++total;
                for (j=0; j<oligoSize; ++j)
                    counts[j][ntVal[(int)target[j]]] += 1;
                }
            if (considerRc && mismatchCount(rcOligo, target, oligoSize) <= mismatchesAllowed)
                {
                ++total;
                reverseComplement(target, oligoSize);
                for (j=0; j<oligoSize; ++j)
                    counts[j][ntVal[(int)target[j]]] += 1;
                reverseComplement(target, oligoSize);
                }
            } 
        }
    }
invTotal = 1.0/total;
for (i=0; i<oligoSize; ++i)
    {
    for (j=0; j<4; ++j)
        {
        profile[i][j] = invTotal * counts[i][j];
        }
    }
}

void fragFind(struct seqList *goodSeq, char *badName, int fragSize, int mismatchesAllowed, 
    boolean considerRc, double profile[16][4])
/* Do fast finding of patterns that are in FA file "goodName", but not in FA file
 * "badName."  BadName can be null.  Pass in the size of the pattern (fragSize) and
 * the number of mismatches to pattern you're willing to tolerate (mismatchesAllowed).
 * It returns the pattern in profile. */
{
int *goodTable, *badTable = NULL;
int goodCount, badCount = 0;
int goodIx;
DNA unpacked[17];

if (mismatchesAllowed > 3)
    errAbort("Sorry, fragFind can only handle 0-3 mismatches.");
if (fragSize > 10)
    errAbort("Sorry, fragFind can only handle fragments up to 10 bases.");

makeOligoHistogram(NULL, goodSeq, fragSize, &goodTable, &goodCount);
if (badName)
    makeOligoHistogram(badName, NULL, fragSize, &badTable, &badCount);
if (badName)
    {
    normalizeTable(goodTable, fragSize);
    normalizeTable(badTable, fragSize);
    diffTables(goodTable, badTable, fragSize);
    goodIx = fuzzVal(badTable, fragSize, mismatchesAllowed, considerRc);
    }
else
    {
    goodIx = fuzzVal(goodTable, fragSize, mismatchesAllowed, considerRc);
    }
freez(&goodTable);
freez(&badTable);
unpackVal(goodIx, fragSize, unpacked);
makeProfile(unpacked, fragSize, mismatchesAllowed, goodSeq, considerRc, profile);
}

#ifdef TESTING_STANDALONE

void usage()
/* Explain usage and exit. */
{
errAbort("fragFind - find patterns in DNA.\n"
         "usage:\n"
         "   fragFind good.fa");
}

int main(int argc, char *argv[])
{
char *goodName, *badName = NULL;
double profile[16][4];
int alphabatize[4] = {A_BASE_VAL, C_BASE_VAL, G_BASE_VAL, T_BASE_VAL};
int i;

dnaUtilOpen();
if (argc < 2)
    usage();
goodName = argv[1];
if (argc >= 3)
    badName = argv[2];
fragFind(goodName, badName, 7, 2, profile);
printf("\n%s\n", unpacked);
for (i=0; i<4; ++i)
    {
    int baseVal = alphabatize[i];
    printf("%c ", valToNt[baseVal]);
    for (j=0; j<fragSize; ++j)
        printf("%1.3f ", profile[j][baseVal]);
    printf("\n");
    }
return 0;
}

#endif /* TESTING_STANDALONE */
