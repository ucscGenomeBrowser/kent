/* bwtMake - Create a bwt transformed version of a file. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: bwtMake.c,v 1.1 2008/11/07 18:16:31 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bwtMake - Create a bwt transformed version of dna sequence\n"
  "usage:\n"
  "   bwtMake in.txt out.bwt\n"
  "The out.bwt will be mostly text, but will have a zero in it.\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

char *inBuf;

int cmpInBufAtOffset(const void *va, const void *vb)
{
int a = *((const int *)va), b = *((const int *)vb);
return strcmp(inBuf+a, inBuf+b);
}


void bwtMake(char *in, char *out)
/* This makes the bwt in a straightforward but inefficient way. */
{
size_t inSize;
readInGulp(in, &inBuf, &inSize);
if (inSize >= 1LL<<32LL)
   errAbort("%s is too big, (%zd bytes), can only handle up to %lld",  in, inSize, (1LL<<32LL));
bits32 *offsets;
AllocArray(offsets, inSize);
bits32 i, size=inSize;
for (i=0; i<size; ++i)
    offsets[i] = i;
qsort(offsets, size, sizeof(offsets[0]), cmpInBufAtOffset);
FILE *f = mustOpen(out, "w");
for (i=0; i<size; ++i)
    {
    int offset = offsets[i];
    if (offset == 0)
	fputc(0, f);
    else
        fputc(inBuf[offset-1], f);
    }
fputc(inBuf[size-1], f);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bwtMake(argv[1], argv[2]);
return 0;
}
