/* bwtReverse - Reverse Burrows Wheeler Transform of a text file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: bwtReverse.c,v 1.2 2008/11/07 23:55:21 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bwtReverse - Reverse Burrows Wheeler Transform of a text file.\n"
  "usage:\n"
  "   bwtReverse XXX\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void unbwt(UBYTE *in, UBYTE *out, bits32 size)
/* Reverse Burrows Wheeler transform of string. */
{
/* Empty case is easy here (and a pain if we have to deal with it later) */
if (size == 0)
    return;

/* Make array that will store the count of occurences of each character up to a given position. */
bits32 *occ;
AllocArray(occ, size);

/* Count up occurences of all characters into offset array.   */
bits32 i, counts[256], offsets[256];
for (i=0; i<256; ++i)
    counts[i] = 0;
for (i=0; i<size; ++i)
    {
    int c = in[i];
    occ[i] = counts[c];
    counts[c] += 1;
    }

/* Convert counts to offsets. */
bits32 total = 0;
for (i=0; i<ArraySize(offsets); ++i)
    {
    offsets[i] = total;
    total += counts[i];
    }

/* Main decompression loop - goes backwards*/
int endPos = 0;
i = size-1;
for (;;)
    {
    int c = out[i] = in[endPos];
    verbose(2, "i=%u, c=%c(%d), endPos=%u\n", i, c, c, endPos);
    verbose(2, "   offsets[%c] = %u, occ[%c@%d] = %u\n", c, offsets[c], c, endPos, occ[endPos]);
    endPos = offsets[c] + occ[endPos];
    counts[c] -= 1;
    if (i == 0)
        break;
    i -= 1;
    }
}

void bwtReverse(char *in, char *out)
/* bwtReverse - Reverse Burrows Wheeler Transform of a text file.. */
{
char *bwtText;
size_t size;
readInGulp(in, &bwtText, &size);
UBYTE *plainText = needHugeMem(size);
unbwt((UBYTE*)bwtText, plainText, size);
writeGulp(out, (char *)plainText+1, size-1);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bwtReverse(argv[1], argv[2]);
return 0;
}
