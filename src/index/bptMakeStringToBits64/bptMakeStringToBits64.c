/* bptMakeStringToBits64 - Create a B+ tree index with string keys and 64-bit-integer values. 
 * In practice the 64-bit values are often offsets in a file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "localmem.h"
#include "bPlusTree.h"


int blockSize = 1000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bptMakeStringToBits64 - Create a B+ tree index with string keys and unsigned 64-bit-integer \n"
  "values. In practice the 64-bit values are often offsets in a file.\n"
  "usage:\n"
  "   bptMakeStringToBits64 input.tab output.bpt\n"
  "where input.tab is two columns - name/value.\n"
  "options:\n"
  "   -blockSize=N (default %d) Size of block for index purposes\n"
  , blockSize
  );
}

static struct optionSpec options[] = {
   {"blockSize", OPTION_INT},
   {NULL, 0},
};

struct name64
/* Pair of a name and a 64-bit integer. */
    {
    struct name64 *next;
    char *name;
    bits64 val;
    };

int name64Cmp(const void *va, const void *vb)
/* Compare to sort on name. */
{
const struct name64 *a = *((struct name64 **)va);
const struct name64 *b = *((struct name64 **)vb);
return strcmp(a->name, b->name);
}

void name64Key(const void *va, char *keyBuf)
/* Get key field. */
{
const struct name64 *a = *((struct name64 **)va);
strcpy(keyBuf, a->name);
}

void *name64Val(const void *va)
/* Get key field. */
{
const struct name64 *a = *((struct name64 **)va);
return (void*)(&a->val);
}

void bptMakeStringToBits64(char *inFile, char *outIndex)
/* bptMakeStringToBits64 - Create a B+ tree index with string keys and 64-bit-integer values. 
 * In practice the 64-bit values are often offsets in a file.. */
{
/* Read inFile into a list in local memory. */
struct lm *lm = lmInit(0);
struct name64 *el, *list = NULL;
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *row[2];
while (lineFileRow(lf, row))
    {
    lmAllocVar(lm, el);
    el->name = lmCloneString(lm, row[0]);
    el->val = sqlLongLong(row[1]);
    slAddHead(&list, el);
    }
lineFileClose(&lf);

int count = slCount(list);
if (count > 0)
    {
    /* Convert list into sorted array */
    struct name64 **itemArray = NULL;
    AllocArray(itemArray, count);
    int i;
    for (i=0, el=list; i<count; i++, el = el->next)
        itemArray[i] = el;
    qsort(itemArray, count, sizeof(itemArray[0]), name64Cmp);

    /* Figure out max size of name field. */
    int maxSize = 0;
    for (i=0; i<count; ++i)
        {
	int size = strlen(itemArray[i]->name);
	if (maxSize < size)
	    maxSize = size;
	}

    bptFileCreate(itemArray, sizeof(itemArray[0]), count, blockSize,
    	name64Key, maxSize, name64Val, sizeof(bits64), outIndex);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
blockSize = optionInt("blockSize", blockSize);
int minBlockSize = 2, maxBlockSize = (1L << 16) - 1;
if (blockSize < minBlockSize || blockSize > maxBlockSize)
    errAbort("Block size (%d) not in range, must be between %d and %d",
    	blockSize, minBlockSize, maxBlockSize);
bptMakeStringToBits64(argv[1], argv[2]);
return 0;
}
