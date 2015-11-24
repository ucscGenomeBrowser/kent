/* PatCount - Counts up the number of occurences of each
 * oligo of a fixed size (up to 13) in input sequence.
 * Writes all patterns that are overrepresented according
 * to a threshold to output. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dnaseq.h"
#include "fa.h"
#include "sig.h"


void usage()
{
errAbort("patCount - counts up the number of occurrences of each\n"
         "oligo of a fixed size (up to 13) in input.  Writes out\n"
         "all patterns that are overrepresented by at least factor\n"
         "to output, which is a binary .ooc file used by patSpace\n"
         "nucleotide homology algorithms.\n"
         "usage:\n"
         "   patCount out.ooc oligoSize overFactor faFiles(s)\n"
         "example:\n"
         "   patCount hgt.ooc 13 6 hgt1.fa hgt2.fa hgt3.fa");
}

unsigned long powerOfFour(int oligoSize)
/* Return integer power of four. */
{
unsigned long p = 1;
int i;
for (i=0; i<oligoSize; ++i)
    p *= 4;
return p;
}


boolean fastFaReadNext(FILE *f, DNA **retDna, int *retSize, char **retName)
/* Read in next FA entry as fast as we can. Return FALSE at EOF. */
{
static int bufSize = 0;
static DNA *buf;
int c;
int bufIx = 0;
static char name[256];
int nameIx = 0;
boolean gotSpace = FALSE;

/* Seek to next '\n' and save first word as name. */
name[0] = 0;
for (;;)
    {
    if ((c = fgetc(f)) == EOF)
        {
        *retDna = NULL;
        *retSize = 0;
        *retName = NULL;
        return FALSE;
        }
    if (!gotSpace && nameIx < ArraySize(name)-1)
        {
        if (isspace(c))
            gotSpace = TRUE;
        else
            {
            name[nameIx++] = c;
            }
        }
    if (c == '\n')
        break;
    }
name[nameIx] = 0;
/* Read until next '>' */
for (;;)
    {
    c = fgetc(f);
    if (isspace(c) || isdigit(c))
        continue;
    if (c == EOF || c == '>')
        c = 0;
    if (bufIx >= bufSize)
        {
        if (bufSize == 0)
            {
            bufSize = 1024 * 1024;
            buf = needMem(bufSize);
            }
        else
            {
            int newBufSize = bufSize + bufSize;
            buf = needMoreMem(buf,  bufSize, newBufSize);
            uglyf("Increasing buffer from %d to %d bytes\n", bufSize, newBufSize);
            bufSize = newBufSize;
            }
        }
    buf[bufIx++] = c;
    if (c == 0)
        {
        *retDna = buf;
        *retSize = bufIx-1;
        *retName = name;
        return TRUE;
        }
    }
}



unsigned addPatCounts(DNA *dna, int dnaSize, int oligoSize, bits16 *patCounts, int mask)
/* Count oligomers in sequence. */
{
int i;
int bits = 0;
int bVal;
unsigned count = 0;
bits16 useCount;
bits16 bigEnough = 0x3fff;

for (i=0; i<oligoSize-1; ++i)
    {
    bVal = ntValNoN[(int)dna[i]];
    bits <<= 2;
    bits += bVal;
    }
for (i=oligoSize-1; i<dnaSize; ++i)
    {
    bVal = ntValNoN[(int)dna[i]];
    bits <<= 2;
    bits += bVal;
    bits &= mask;
    useCount = patCounts[bits];
    if (useCount < bigEnough)
        patCounts[bits] = useCount + 1;
    ++count;
    }
return count;
}

int main(int argc, char *argv[])
{
char *s;
int oligoSize;
size_t patSpaceSize;
size_t patSpaceByteSize;
bits32 patIx;
double overFactor;
double threshold;
int thresh;
char *inName, *outName;
FILE *in, *out;
int i;
unsigned totalCount = 0;
bits16 *patCounts;
int count, maxCount=0, distinctCount=0, overCount = 0;

dnaUtilOpen();

/* Process command line. */
if (argc < 5)
    usage();
outName = argv[1];
s = argv[2];
if (!isdigit(s[0]))
    usage();
oligoSize = atoi(s);
if (oligoSize <= 0 || oligoSize > 13)
    usage();
s = argv[3];
if (!isdigit(s[0]))
    usage();
overFactor = atof(s);
if (overFactor < 1.1)
    usage();

/* Allocate an integer array to store frequency counts of
 * each oligomer. */
patSpaceSize = powerOfFour(oligoSize);
patSpaceByteSize = patSpaceSize * sizeof(patCounts[0]);
uglyf("Allocating pattern space of %zu bytes\n", patSpaceByteSize);
patCounts = needLargeMem(patSpaceByteSize);
memset(patCounts, 0, patSpaceByteSize);

/* Loop through input files. */
out = mustOpen(outName, "wb");
for (i=4; i<argc; ++i)
    {
    int seqCount = 0;
    int dotCount = 0;
    DNA *dna;
    int dnaSize;
    char *dnaName;

    inName = argv[i];
    in = mustOpen(inName, "rb");
    printf("Processing %s\n", inName);
    while (fastFaReadNext(in, &dna, &dnaSize, &dnaName) )
        {
        if ((++seqCount & 0xf) == 0)
            {
            printf(".");
            fflush(stdout);
            if ((++dotCount & 0x3f) == 0)
                printf("\n");
            }
//        printf("%d %s %d %d\n", ++seqCount, dnaName, dnaSize, totalCount);
        totalCount += addPatCounts(dna, dnaSize, oligoSize, patCounts, patSpaceSize-1);
        }
    putchar('\n');
    }
threshold = 1 + overFactor * totalCount / patSpaceSize ;
uglyf("Total count %u overFactor %f threshold %f\n", totalCount, overFactor, threshold);
thresh = round(threshold);

/* Write out output and gather a few stats. */
    {
    bits32 oSig = oocSig;
    bits32 oSize = oligoSize;
    writeOne(out, oSig);
    writeOne(out, oSize);
    for (patIx=0; patIx<patSpaceSize; ++patIx)
        {
        count = patCounts[patIx];
        if (count > 0)
            {
            ++distinctCount;
            if (count > maxCount)
                maxCount = count;
            if (count >= thresh)
                {
                ++overCount;
                writeOne(out, patIx);
                }
            }
        }
    fclose(out);
    }

/* Write statistics to console. */
printf("Statistic for oligos of size %d (patSpaceSize %zu)\n", oligoSize, patSpaceSize);
printf("Total oligos %d\n", totalCount);
printf("Distinct oligos %d\n", distinctCount);
printf("Maximum occurrences of single oligo %d\n", maxCount);
printf("OverFactor %4.2f yeilds threshold %d\n", overFactor, thresh);
printf("Oligos filtered out as too common %d (%2.1f%%)\n", overCount,
    100.0*overCount/distinctCount);

return 0;
}
