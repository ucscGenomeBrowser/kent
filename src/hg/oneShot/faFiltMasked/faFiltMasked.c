/* faFiltMasked - Filter out reads that have less than 100 base pairs unmasked. */
#include "common.h"
#include "dnautil.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "faFiltMasked - Filter out reads that have less than 100 base pairs unmasked\n"
  "usage:\n"
  "   faFiltMasked output.fa inFile(s).fa\n");
}

int uniqCount(DNA *dna, int size)
/* Count up  non-N bases. */
{
int count = 0;
int i;
for (i=0; i<size; ++i)
    if (ntChars[dna[i]] )
        ++count;
return count;
}

void faFiltMasked(char *outName, int inCount, char *inNames[])
/* faFiltMasked - Filter out reads that have less than 100 base pairs unmasked. */
{
char *inName;
FILE *out = mustOpen(outName, "w");
FILE *in;
DNA *dna;
int size;
char *name;
int i;
int allCount = 0, goodCount = 0;


for (i=0; i<inCount; ++i)
    {
    inName = inNames[i];
    in = mustOpen(inName, "r");
    while (faFastReadNext(in, &dna, &size, &name))
	{
	++allCount;
	if ((allCount&0x1fff) == 0)
	    {
	    printf(".");
	    fflush(stdout);
	    }
	if (uniqCount(dna, size) >= 100)
	    {
	    ++goodCount;
	    faWriteNext(out, name, dna, size);
	    }
	}
    fclose(in);
    }
fclose(out);
printf("\n");
printf("Wrote %d of %d sequences to %s\n", goodCount, allCount, outName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
dnaUtilOpen();
faFiltMasked(argv[1], argc-2, argv+2);
return 0;
}
