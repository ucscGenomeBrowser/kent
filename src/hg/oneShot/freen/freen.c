/* freen - My Pet Freen. */
#include "common.h"
#include <zlib.h>
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"


static char const rcsid[] = "$Id: freen.c,v 1.90 2009/11/10 01:21:14 kent Exp $";

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
uLongf maxSize = uncompressedSize * 1.001 + 13;
uLongf compressedSize = maxSize;
char *compressedBuf = needLargeMem(maxSize);
int err = compress((Bytef*)compressedBuf, &compressedSize, (Bytef*)uncompressedBuf, uncompressedSize);
printf("uncompressedSize %d, compressedSize %d, err %d\n", (int)uncompressedSize, (int)compressedSize, err);
FILE *f = mustOpen(output, "wb");
mustWrite(f, compressedBuf, compressedSize);
carefulClose(&f);
f = mustOpen(uncompressed, "wb");
uLongf uncSize = uncompressedSize;
memset(uncompressedBuf, 0, uncompressedSize);
err = uncompress((Bytef*)uncompressedBuf,  &uncSize, (Bytef*)compressedBuf, compressedSize);
printf("uncompressedSize %d, err %d\n", (int)uncSize, err);
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
