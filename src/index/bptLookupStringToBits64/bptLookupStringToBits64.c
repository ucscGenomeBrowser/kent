/* bptLookupStringToBits64 - Given a string value look up and return associated 64 bit value if 
 * any. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bPlusTree.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "bptLookupStringToBits64 - Given a string value look up and return associated 64 bit value if\n"
  "any.\n"
  "usage:\n"
  "   bptLookupStringToBits64 index.bpt string\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void bptLookupStringToBits64(char *indexFile, char *string)
/* bptLookupStringToBits64 - Given a string value look up and return associated 64 bit value if 
 * any */
{
struct bptFile *bpt = bptFileOpen(indexFile);
bits64 val;
if (bptFileFind(bpt, string, strlen(string), &val, sizeof(val)) )
    printf("%lld\n", val);
else
    errAbort("%s not found", string);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
bptLookupStringToBits64(argv[1], argv[2]);
return 0;
}
