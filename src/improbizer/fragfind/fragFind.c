/* fragFind - fast, sloppy way of finding patterns.  Doesn't need to be exact. */
#include "common.h"
#include "portable.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"

int oligoVal(DNA *oligo, int oligoSize)
/* Return oligo converted to a number - 2 bits per base.
 * returns -1 if oligo has 'N's or other non-nt characters. */
{
int i;
int acc = 0;
int baseVal;

for (i=0; i<oligoSize; ++i)
    {
    if ((baseVal = ntVal[oligo[i]]) < 0)
        return -1;
    acc <<= 2;
    acc += baseVal;
    }
return acc;
}

void unpackVal(int val, int oligoSize, char *out)
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

void makeOligoHistogram(char *fileName, int oligoSize, int **retTable, int *retTotal)
/* Make up table of oligo occurences. */
{
FILE *f = mustOpen(fileName, "rb");
int tableSize = (1<<(oligoSize+oligoSize));
int tableByteSize = tableSize * sizeof(int);
int *table = needLargeMem(tableByteSize);
struct dnaSeq *seq;
int total = 0;

memset(table, 0, tableByteSize);
while ((seq = faReadOneDnaSeq(f, "", TRUE)) != NULL)
    {
    DNA *dna = seq->dna;
    int size = seq->size;
    int endIx = size-oligoSize;
    int i;
    int oliVal;

    for (i=0; i<endIx; ++i)
        {
        if ((oliVal = oligoVal(dna+i, oligoSize)) >= 0)
            {
            table[oliVal] += 1;
            ++total;
            }
        }
    freeDnaSeq(&seq);
    }
fclose(f);
*retTable = table;
*retTotal = total;
}

int bestTableIx(int *table, int oligoSize)
/* Return index of highest count in table. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int bestVal = -1;
int bestIx = 0;
int i;
int val;

for (i=0; i<tableSize; ++i)
    {
    if ((val = table[i]) > bestVal)
        {
        bestVal = val;
        bestIx = i;
        }
    }
return bestIx;
}

int fuzzValOne(int *table, int oligoSize)
/* Return value of table entry with most number of 
 * matches that are off by no more than one. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int tableByteSize = tableSize * sizeof(int);
int tableIx;
int bestVal = -0x3ffffff;
int bestIx;

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
        bestIx = tableIx;
        }
    }
return tableIx;
}

int fuzzValTwo(int *table, int oligoSize)
/* Return value of table entry with most number of 
 * matches that are off by no more than one. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int tableByteSize = tableSize * sizeof(int);
int tableIx;
int bestVal = -0x3ffffff;
int bestIx;

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
            int maskOne = 0x3;
            int incOne = 1;
            int baseOne;
            for (baseOne = 0; baseOne<oligoSize; ++baseOne)
                {
                if (baseOne != baseTwo)
                    {
                    ixOne = (ixTwo&(~maskOne));
                    acc += table[ixOne];
                    ixOne += incOne;
                    acc += table[ixOne];
                    ixOne += incOne;
                    acc += table[ixOne];
                    ixOne += incOne;
                    acc += table[ixOne];
                    }
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

int fuzzValThree(int *table, int oligoSize)
/* Return value of table entry with most number of 
 * matches that are off by no more than one. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int tableByteSize = tableSize * sizeof(int);
int tableIx;
int bestVal = -0x3ffffff;
int bestIx;

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
            int maskTwo = 0x3;
            int incTwo = 1;
            int baseTwo;
            int j;
            for (baseTwo = 0; baseTwo<oligoSize; ++baseTwo)
                {
                if (baseTwo != baseThree)
                    {
                    ixTwo = (ixThree&(~maskTwo));
                    for (j=0; j<4; ++j)
                        {
                        int ixOne;
                        int maskOne = 0x3;
                        int incOne = 1;
                        int baseOne;
                        for (baseOne = 0; baseOne<oligoSize; ++baseOne)
                            {
                            if (baseOne != baseTwo && baseOne != baseThree)
                                {
                                ixOne = (ixTwo&(~maskOne));
                                acc += table[ixOne];
                                ixOne += incOne;
                                acc += table[ixOne];
                                ixOne += incOne;
                                acc += table[ixOne];
                                ixOne += incOne;
                                acc += table[ixOne];
                                }
                            maskOne <<= 2;
                            incOne <<= 2;
                            }
                        ixTwo += incTwo;
                        }
                    }
                maskTwo <<= 2;
                incTwo <<= 2;
                }
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

int fuzzVal(int *table, int oligoSize, int mismatchesAllowed)
/* Return value of table entry with most number of 
 * matches that are off by no more than mismatchesAllowed. */
{
if (mismatchesAllowed == 0)
    return bestTableIx(table, oligoSize);
else if (mismatchesAllowed == 1)
    return fuzzValOne(table, oligoSize);
else if (mismatchesAllowed == 2)
    return fuzzValTwo(table, oligoSize);
else if (mismatchesAllowed == 3)
    return fuzzValThree(table, oligoSize);
else
    errAbort("Sorry, fuzzVal can only handle 0-3 mismatches.");
}

void normalizeTable(int *table, int oligoSize)
/* Normalize table so that all entries are between 0 and 1000000 */
{
int tableSize = (1<<(oligoSize+oligoSize));
int i = bestTableIx(table, oligoSize);
int maxVal = table[i];
double scaleBy = 1000000.0/maxVal;

for (i=0; i<tableSize; i += 1)
    {
    table[i] = round(scaleBy*table[i]);
    }
}

void diffTables(int *a, int *b, int oligoSize)
/* Subtract table b from table a, leaving results in b. */
{
int tableSize = (1<<(oligoSize+oligoSize));
int i;
for (i=0; i<tableSize; i += 1)
    {
    b[i] = a[i] - b[i];
    }
}

boolean allGoodBases(DNA *oligo, int oligoSize)
/* Returns TRUE if all bases in oligo are nucleotides. */
{
int i;
for (i=0; i<oligoSize; ++i)
    if (ntVal[oligo[i]] < 0)
        return FALSE;
return TRUE;
}

int mismatchCount(DNA *a, DNA *b, int size)
/* Count up number of mismatches between a and b */
{
int count = 0;
int i;

for (i=0; i<size; ++i)
    if (a[i] != b[i])
        ++count;
return count;
}

void makeProfile(DNA *oligo, int oligoSize, int mismatchesAllowed, 
    char *faName, double profile[16][4])
/* Scan through file counting up things that match oligo to within
 * mismatch tolerance, and use these counts to build up a profile. */
{
FILE *f = mustOpen(faName, "rb");
struct dnaSeq *seq;
int counts[16][4];
int total = 0;
double invTotal;
int i,j;
int seqCount = 0;

zeroBytes(counts, sizeof(counts));
while ((seq = faReadOneDnaSeq(f, "", TRUE)) != NULL)
    {
    DNA *dna = seq->dna;
    int size = seq->size;
    int endIx = size-oligoSize;

    ++seqCount;
    for (i=0; i<endIx; ++i)
        {
        DNA *target = dna+i;
        if (allGoodBases(target, oligoSize) && mismatchCount(oligo, target, oligoSize) <= mismatchesAllowed)
            {
            ++total;
            for (j=0; j<oligoSize; ++j)
                counts[j][ntVal[target[j]]] += 1;
            } 
        }
    freeDnaSeq(&seq);
    }
fclose(f);
invTotal = 1.0/total;
printf("Got %d matches to %s with %d or less mismatches in %d sequences\n",
    total, oligo, mismatchesAllowed, seqCount);
for (i=0; i<oligoSize; ++i)
    {
    for (j=0; j<4; ++j)
        {
        profile[i][j] = invTotal * counts[i][j];
        }
    }
}

void fragFind(char *goodName, char *badName, int fragSize, int mismatchesAllowed)
/* Chop DNA into fragments.  Look them up. */
{
int *goodTable, *badTable = NULL;
int goodCount, badCount = 0;
int goodIx;
double profile[16][4];
int i, j;
int alphabatize[4] = {A_BASE_VAL, C_BASE_VAL, G_BASE_VAL, T_BASE_VAL};
long startTime;
DNA unpacked[17];

startTime = clock1000();
makeOligoHistogram(goodName, fragSize, &goodTable, &goodCount);
if (badName)
    makeOligoHistogram(badName, fragSize, &badTable, &badCount);
printf("%4.3f seconds to load data and build histograms\n", 0.001*(clock1000()-startTime));

startTime = clock1000();
if (badName)
    {
    normalizeTable(goodTable, fragSize);
    normalizeTable(badTable, fragSize);
    diffTables(goodTable, badTable, fragSize);
    goodIx = fuzzVal(badTable, fragSize, mismatchesAllowed);
    }
else
    {
    goodIx = fuzzVal(goodTable, fragSize, mismatchesAllowed);
    }
unpackVal(goodIx, fragSize, unpacked);
makeProfile(unpacked, fragSize, mismatchesAllowed, goodName, profile);
printf("%4.3f seconds to fuzz things up\n", 0.001*(clock1000()-startTime));
printf("\n%s\n", unpacked);
for (i=0; i<4; ++i)
    {
    int baseVal = alphabatize[i];
    printf("%c ", valToNt[baseVal]);
    for (j=0; j<fragSize; ++j)
        printf("%1.3f ", profile[j][baseVal]);
    printf("\n");
    }
}


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

dnaUtilOpen();
if (argc < 2)
    usage();
goodName = argv[1];
if (argc >= 3)
    badName = argv[2];
fragFind(goodName, badName, 7, 2);
return 0;
}
