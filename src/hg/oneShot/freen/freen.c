/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "binRange.h"

static char const rcsid[] = "$Id: freen.c,v 1.67 2006/06/29 01:27:45 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

int level = 0;


void freen(char *fileName)
/* Test some hair-brained thing. */
{
FILE *f = mustOpen(fileName, "wb");
bits64 x = 0x123456789ABCDEF0;
uglyf("before = %llx\n", x);
writeBits64(f, x);
carefulClose(&f);
f = mustOpen(fileName, "rb");
x = readBits64(f);
uglyf("after = %llx\n", x);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
