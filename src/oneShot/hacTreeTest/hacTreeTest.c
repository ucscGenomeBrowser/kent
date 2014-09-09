/* hacTreeTest - Test out various hacTree code. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hacTree.h"
#include "pthreadDoList.h"

char* clHacTree = "fromItems";
int clThreads = 10;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hacTreeTest - Test out various hacTree code\n"
  "usage:\n"
  "   hacTreeTest XXX\n"
  "options:\n"
  "   -hacTree = Dictates how the tree is generated;  multiThreads or fromItems. fromItems is default \n"
  "   -threads - number of threads to use for multiThread default %d\n"
  , clThreads
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"threads", OPTION_INT},
   {"hacTree", OPTION_STRING},
   {NULL, 0},
};

void rDump(struct hacTree *ht, int level, FILE *f)
/* Help dump out results */
{
spaceOut(f, level*2);
struct slDouble *el = (struct slDouble *)ht->itemOrCluster;
if (ht->left || ht->right)
    {
    fprintf(f, "(%g %g)\n", el->val, ht->childDistance);
    rDump(ht->left, level+1, f);
    rDump(ht->right, level+1, f);
    }
else
    fprintf(f, "%g\n", el->val);
}

double dblDistance(const struct slList *item1, const struct slList *item2, void *extraData)
{
struct slDouble *i1 = (struct slDouble *)item1;
struct slDouble *i2 = (struct slDouble *)item2;
double d = fabs(i1->val - i2->val);
verbose(2, "dblDistance %g %g = %g\n", i1->val, i2->val, d);
return d;
}

struct slList *dblMerge(const struct slList *item1, const struct slList *item2, 
    void *extraData)
{
struct slDouble *i1 = (struct slDouble *)item1;
struct slDouble *i2 = (struct slDouble *)item2;
double d = 0.5 * (i1->val + i2->val);
verbose(2, "dblMerge %g %g = %g\n", i1->val, i2->val, d);
return (struct slList *)slDoubleNew(d);
}

void hacTreeTest(char *output)
/* hacTreeTest - Test out various hacTree code. */
{
FILE *f = mustOpen(output, "w");
int i;

/* Make up list of random numbers */
struct slDouble *list = NULL;
double data[] = {1,2, 4,5,  6,7,  11,15,  22,24};
for (i=0; i<10; ++i)
    {
    struct slDouble *el = slDoubleNew(data[i]);
    slAddHead(&list, el);
    }
struct lm *lm = lmInit(0);
struct hacTree *ht = NULL;

if (sameString(clHacTree, "multiThreads"))
    {
    ht = hacTreeMultiThread(clThreads, (struct slList *)list, lm, dblDistance, dblMerge, 
	NULL, NULL);
    }
else if (sameString(clHacTree, "fromItems"))
    {
    ht = hacTreeFromItems((struct slList *)list, lm, dblDistance, dblMerge, NULL, NULL);
    }
else 
    {
    uglyAbort("Unrecognized input option: %s", clHacTree);
    }
rDump(ht, 0, f);
carefulClose(&f);
}



int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clHacTree = optionVal("hacTree", clHacTree);
clThreads = optionInt("threads", clThreads);
hacTreeTest(argv[1]);
return 0;
}
