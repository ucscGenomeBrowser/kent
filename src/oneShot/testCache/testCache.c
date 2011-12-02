/* testCache - Experiments with file cacher.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "obscure.h"
#include "udc.h"



void usage()
/* Explain usage and exit. */
{
errAbort(
  "testCache - Experiments with file cacher.\n"
  "usage:\n"
  "   testCache sourceUrl offset size outFile\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void testCache(char *sourceUrl, bits64 offset, bits64 size, char *outFile)
/* testCache - Experiments with file cacher.. */
{
/* Open up cache and file. */
struct udcFile *file = udcFileOpen(sourceUrl, "cache");

/* Read data and write it back out */
void *buf = needMem(size);
udcSeek(file, offset);
size = udcRead(file, buf, size);
if (size >= 0)
    writeGulp(outFile, buf, size);

/* Clean up. */
udcFileClose(&file);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
testCache(argv[1], sqlUnsigned(argv[2]), sqlUnsigned(argv[3]), argv[4]);
return 0;
}
