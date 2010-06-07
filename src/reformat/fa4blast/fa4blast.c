/* fa4blast - Blast doesn't handle really long contigs in it's FA files. 
 * This program breaks up the contigs into 100000 base (non-overlapping)
 * units. */
#include "common.h"
#include "dnaseq.h"
#include "fa.h"
#include "dystring.h"

void usage()
/* Print usage instructions. */
{
errAbort(
    "fa4blast - Blast doesn't handle really long contigs in it's FA files.\n"
    "This program breaks up the contigs into 100000 base (non-overlapping)\n"
    "units.\n"
    "usage:\n"
    "   fa4blast output.fa input(s).fa");
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *inName, *outName, **inNames;
FILE *in, *out;
int i, inCount;
DNA *dna;
int inSize, outSize;
int dnaOff;
char *seqName;
struct dyString *subSeqName = newDyString(512);
int maxSize = 100000;

if (argc < 3)
    usage();
outName = argv[1];
inNames = &argv[2];
inCount = argc-2;
out = mustOpen(outName, "w");
for (i=0; i<inCount; ++i)
    {
    inName = inNames[i];
    printf("processing %s", inName);
    in = mustOpen(inName, "r");
    while (faFastReadNext(in, &dna, &inSize, &seqName))
	{
	for (dnaOff = 0; dnaOff < inSize; dnaOff += outSize)
	    {
	    printf(".");
	    fflush(stdout);
	    outSize = inSize - dnaOff;
	    if (outSize > maxSize) outSize = maxSize;
	    dyStringClear(subSeqName);
	    dyStringPrintf(subSeqName, "%s.%d", seqName, dnaOff);
	    faWriteNext(out, subSeqName->string, dna+dnaOff, outSize);
	    }
	}
    fclose(in);
    printf("\n");
    }
}
