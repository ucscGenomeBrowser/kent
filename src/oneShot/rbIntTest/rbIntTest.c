/* rbIntTest - Test rbIntTree. */

/* Results of this for trees of 100,000 are:
 *   rbIntTree: 656 millis
 *   rbTree: 795 millis
 *   hash: 2927 millis
 * So in all it looks like it's not worth adding the special case
 * rbIntTree, especially since it's currently crashing for counts of
 * 200,000.  It does look worthwhile to use rbTree rather than a hash
 * though. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "memalloc.h"
#include "localmem.h"
#include "rbTree.h"
#include "rbIntTree.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "rbIntTest - Test rbIntTree\n"
  "usage:\n"
  "   rbIntTest count\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void rbIntTest(int count)
/* Fill up rbIntTree with count # of nodes and then search for those
 * nodes and then free it up. */
{
int i, j;
struct rbIntTree *tree = rbIntTreeNew();
struct lm *lm = tree->lm;
for (i=0; i<count; ++i)
    {
    int *pt;
    lmAllocVar(lm, pt);
    *pt = i;
    rbIntTreeAdd(tree, i, pt);
    }
for (j=0; j<10; ++j)
    for (i=0; i<count; ++i)
	if (!rbIntTreeFind(tree, i))
	    errAbort("Couldnt' find %d", i);
rbIntTreeFree(&tree);
}

int rbTreeCmpInt(void *va, void *vb)
/* Compare integers in rbTree. */
{
int *a = va, *b = vb;
return *a-*b;
}

void rbTest(int count)
/* Fill up rbTree with count # of nodes and then search for those
 * nodes and then free it up. */
{
int i, j;
struct rbTree *tree = rbTreeNew(rbTreeCmpInt);
struct lm *lm = tree->lm;
for (i=0; i<count; ++i)
    {
    int *pt;
    lmAllocVar(lm, pt);
    *pt = i;
    rbTreeAdd(tree, pt);
    }
for (j=0; j<10; ++j)
    for (i=0; i<count; ++i)
	if (!rbTreeFind(tree, &i))
	    errAbort("Couldnt' find %d", i);
rbTreeFree(&tree);
}

void hashTest(int count)
/* Do hash-based version. */
{
char name[8];
int i, j;
struct hash *hash = hashNew(16);
struct lm *lm = hash->lm;

for (i=0; i<count; ++i)
    {
    int *pt;
    lmAllocVar(lm, pt);
    *pt = i;
    sprintf(name, "%x", i);
    hashAdd(hash, name, pt);
    }
for (j=0; j<10; ++j)
    {
    for (i=0; i<count; ++i)
        {
	sprintf(name, "%x", i);
	if (!hashLookup(hash, name))
	    errAbort("Couldn't find %s in hash", name);
	}
    }
}

void test(int count)
/* test - Test rbTree and rbIntTree. */
{
uglyTime(NULL);
rbIntTest(count);
uglyTime("rbIntTree");
rbTest(count);
uglyTime("rbTree");
hashTest(count);
uglyTime("hash");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
pushCarefulMemHandler(100000000);
if (argc != 2)
    usage();
test(atoi(argv[1]));
return 0;
}
