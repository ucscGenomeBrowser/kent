/* itsaDump - Dump out itsa file into a readable format.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "itsa.h"

static char const rcsid[] = "$Id: itsaDump.c,v 1.2 2008/10/31 05:45:05 kent Exp $";

int maxSize = 1000000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "itsaDump - Dump out itsa file into a readable format.\n"
  "usage:\n"
  "   itsaDump input.itsa output.txt\n"
  "options:\n"
  "   -index=indexOut.txt\n"
  "   -maxSize=N - maximum lines to write out, default %d\n"
  , maxSize
  );
}

static struct optionSpec options[] = {
   {"index", OPTION_STRING},
   {"maxSize", OPTION_INT},
   {NULL, 0},
};

void writeZeroSuppress(FILE *f, DNA *dna, int size)
/* Write out "dna" converting any non-dna chars to _ */
{
int i;
for (i=0; i<size; ++i)
    {
    int b = dna[i];
    if (b < 0 || ntVal[b] < 0)
        b = '_';
    fputc(b, f);
    }
}


char *binaryToDna13(int x, char dna[14])
/* Convert from binary to DNA format. */
{
int i;
for (i=12; i>=0; --i)
    {
    int lowBits = (x&3);
    dna[i] = "acgt"[lowBits];
    x >>= 2;
    }
return dna;
}

void itsaDump(char *input, char *output)
/* itsaDump - Dump out info on itsa.  Useful for debugging. */
{
struct itsa *itsa = itsaRead(input, FALSE);
FILE *f = mustOpen(output, "w");
int i;
int maxCount = maxSize;
if (maxCount > itsa->header->arraySize)
    maxCount = itsa->header->arraySize;
for (i=0; i<maxCount; ++i)
    {
    fprintf(f, "%4d %4d ", i, itsa->traverse[i]);
    writeZeroSuppress(f, itsa->allDna+itsa->array[i], 30);
    fprintf(f, " %d\n", itsa->array[i]);
    }
carefulClose(&f);

char *index = optionVal("index", NULL);
if (index)
    {
    char dna13[14];
    dna13[13] = 0;
    f = mustOpen(index, "w");
    for (i=0; i<maxCount; ++i)
       {
       binaryToDna13(i, dna13);
       int cursor = itsa->cursors13[i];
       dna13[cursor] = toupper(dna13[cursor]);
       fprintf(f, "%d %s ", i, dna13);
       bits32 suffixArrayPos = itsa->index13[i];
       if (suffixArrayPos == 0)
           fprintf(f, "n/a\n");
       else
	   fprintf(f, "%u\n", suffixArrayPos-1);
       }
    carefulClose(&f);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
dnaUtilOpen();
optionInit(&argc, argv, options);
maxSize = optionInt("maxSize", maxSize);
if (argc != 3)
    usage();
itsaDump(argv[1], argv[2]);
return 0;
}
