/* bwtFind - Figure out if a string is in bwt transformed text.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: bwtFind.c,v 1.1 2008/11/08 10:46:05 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bwtFind - Figure out if a string is in bwt transformed text.\n"
  "usage:\n"
  "   bwtFind text.bwt string\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int _findOcc(UBYTE *bwt, UBYTE c, bits32 *occ,  bits32 *counts, int pos)
/* Use occ array to help find the number of characters c that
 * occur in bwt before position. */
{
if (pos <= 0)
   return -1;
if (counts[c] == 0)
    return 0;
while (pos > 0)
    {
    if (bwt[pos] == c)
        return occ[pos];
    --pos;
    }
return 0;
}

int findOcc(UBYTE *bwt, UBYTE c, int pos)
{
if (pos < 0)
   return 0;
int i;
int count = 0;
for (i=0; i<=pos; ++i)
    if (bwt[i] == c)
       ++count;
return count;
}

int bwtSearch(UBYTE *bwt, bits32 size, char *string)
/* Reverse Burrows Wheeler transform of string. */
{
/* Empty case is easy here (and a pain if we have to deal with it later) */
if (size == 0)
    return 0;

/* Make array that will store the count of occurences of each character up to a given position. */
bits32 *occ;
AllocArray(occ, size+1);

/* Count up occurences of all characters into offset array.   */
bits32 i, counts[256], offsets[256];
for (i=0; i<256; ++i)
    counts[i] = 0;
for (i=0; i<size; ++i)
    {
    int c = bwt[i];
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

i = strlen(string)-1;
int start = 0, end = size-1;
while (start <= end)
    {
    UBYTE c = string[i];
    int startOcc = findOcc(bwt, c, start-1);
    int endOcc = findOcc(bwt, c, end) - 1;
    uglyf("%u  start=%u  end=%u  c=%c(%d) offsets[%c]=%d startOcc=%d endOcc=%d\n", i, start, end, c, c, c, offsets[c], startOcc, endOcc);
    start = offsets[c] + startOcc;
    end = offsets[c] + endOcc;
    if (i == 0)
        break;
    i = i - 1;
    }
uglyf("final i=%d start=%u end=%u\n", i, start, end);
if (i != 0)
    return 0;
return end - start + 1;
}

void bwtFind(char *bwtFile, char *string)
/* bwtFind - Figure out if a string is in bwt transformed text.. */
{
char *bwtText;
size_t size;
readInGulp(bwtFile, &bwtText, &size);
printf("%s occurs %d times in %s\n", string, bwtSearch((UBYTE*)bwtText, size, string), bwtFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bwtFind(argv[1], argv[2]);
return 0;
}
