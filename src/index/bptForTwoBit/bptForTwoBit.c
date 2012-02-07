/* bptForTwoBit - Create a b+ tree index for a .2bit file.  Key is the sequence name. 
 * Value is the position of the start of the compressed DNA in the .2bit file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "twoBit.h"
#include "bPlusTree.h"


int blockSize = 256;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bptForTwoBit - Create a b+ tree index for a .2bit file.  Key is the sequence name. Value \n"
  "is the position of the start of the compressed DNA in the .2bit file.\n"
  "usage:\n"
  "   bptForTwoBit in.2bit out.bpt\n"
  "options:\n"
  "   -blockSize=N - number of children per node in b+ tree. Default %d\n"
  , blockSize
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {NULL, 0},
};

int twoBitIndexCmp(const void *va, const void *vb)
/* Compare to sort on name. */
{
const struct twoBitIndex *a = *((struct twoBitIndex **)va);
const struct twoBitIndex *b = *((struct twoBitIndex **)vb);
return strcmp(a->name, b->name);
}

void twoBitIndexKey(const void *va, char *keyBuf)
/* Get key field. */
{
const struct twoBitIndex *a = *((struct twoBitIndex **)va);
strcpy(keyBuf, a->name);
}

void *twoBitIndexVal(const void *va)
/* Get key field. */
{
const struct twoBitIndex *a = *((struct twoBitIndex **)va);
return (void*)(&a->offset);
}

void bptForTwoBit(char *twoBitIn, char *indexOut)
/* bptForTwoBit - Create a b+ tree index for a .2bit file.  Key is the sequence name. 
 * Value is the position of the start of the compressed DNA in the .2bit file. */
{
/* Read two bit file, and convert linked list index to array. */
struct twoBitFile *tbf = twoBitOpen(twoBitIn);
struct twoBitIndex *tbi, **tbiArray;
int elCount = tbf->hash->elCount;
AllocArray(tbiArray, elCount);
int i;
for (i=0, tbi=tbf->indexList; i < elCount; ++i, tbi=tbi->next)
    tbiArray[i] = tbi;

/* Calculate longest name. */
int maxSize = 0;
for (tbi = tbf->indexList; tbi != NULL; tbi = tbi->next)
    {
    int size = strlen(tbi->name);
    if (maxSize < size)
        maxSize = size;
    }

/* Create index. */
bptFileCreate(tbiArray, sizeof(tbiArray[0]), elCount, blockSize,
    twoBitIndexKey, maxSize, twoBitIndexVal, sizeof(tbi->offset), indexOut);
verbose(1, "Created index of %d sequences in %s\n", elCount, indexOut);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
blockSize = optionInt("blockSize", blockSize);
bptForTwoBit(argv[1], argv[2]);
return 0;
}
