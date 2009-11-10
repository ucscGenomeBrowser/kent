/* freen - My Pet Freen. */
#include "common.h"
#include "zlibFace.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"


static char const rcsid[] = "$Id: freen.c,v 1.91 2009/11/10 20:52:58 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file\n");
}


void freen(char *input, char *output, char *uncompressed)
/* Test some hair-brained thing. */
{
size_t uncompressedSize;
char *uncompressedBuf;
readInGulp(input, &uncompressedBuf, &uncompressedSize);
size_t compBufSize = zCompBufSize(uncompressedSize);
char *compBuf = needLargeMem(compBufSize);
size_t compressedSize = zCompress(uncompressedBuf, uncompressedSize, compBuf, compBufSize);
printf("uncompressedSize %d, compressedSize %d\n", (int)uncompressedSize, (int)compressedSize);
FILE *f = mustOpen(output, "wb");
mustWrite(f, compBuf, compressedSize);
carefulClose(&f);
f = mustOpen(uncompressed, "wb");
memset(uncompressedBuf, 0, uncompressedSize);
size_t uncSize = zUncompress(compBuf, compressedSize, uncompressedBuf, uncompressedSize);
printf("uncompressedSize %d\n", (int)uncSize);
mustWrite(f, uncompressedBuf, uncSize);
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
freen(argv[1], argv[2], argv[3]);
return 0;
}
