/* bwtMake - Create a bwt transformed version of a file. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: bwtMake.c,v 1.3 2008/11/07 23:54:52 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bwtMake - Create a bwt transformed version of dna sequence\n"
  "usage:\n"
  "   bwtMake in.txt out.bwt\n"
  "The out.bwt will be mostly text, but will have a zero in it.\n"
  "   -suffix=file Output suffix tree to file.\n"
  );
}

static struct optionSpec options[] = {
   {"suffix", OPTION_STRING},
   {NULL, 0},
};

char *inBuf;

int cmpInBufAtOffset(const void *va, const void *vb)
{
int a = *((const int *)va), b = *((const int *)vb);
return strcmp(inBuf+a, inBuf+b);
}

void writeUpTo(FILE *f,  char *s,  int maxSize)
/* Write string to file, up to either end of string of maxSize chars */
{
int i;
for (i=0; i<maxSize; ++i)
    {
    char c = *s++;
    if (c == 0)
       break;
    if (c == '\n')
       fprintf(f, "\\n");
    else
	fputc(c, f);
    }
if (i == maxSize)
    fprintf(f, "...");
fputc('$', f);
}

void bwtMake(char *in, char *out)
/* This makes the bwt in a straightforward but inefficient way. */
{
size_t inSize;
readInGulp(in, &inBuf, &inSize);
if (inSize >= 1LL<<32LL)
   errAbort("%s is too big, (%zd bytes), can only handle up to %lld",  in, inSize, (1LL<<32LL));
bits32 *offsets;
bits32 i, size=inSize+1;
AllocArray(offsets, size);
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
carefulClose(&f);

char *suffixFile = optionVal("suffix", NULL);
if (suffixFile)
   {
   FILE *f = mustOpen(suffixFile, "w");
   for (i=0; i<size; ++i)
       {
       int offset = offsets[i];
       char c;
       if (offset == 0)
          c = '$';
       else
          c = inBuf[offset-1];
       if (c == '\n')
           fprintf(f, "%u \\n", i);
       else
	   fprintf(f, "%u %c ", i, c);
	   writeUpTo(f, inBuf +offset,  60);
	   fprintf(f, "\n");
       }
   carefulClose(&f);
   }
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
