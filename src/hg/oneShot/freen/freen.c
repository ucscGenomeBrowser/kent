/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hacTree.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen output\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void rDump(struct hacTree *ht, int level, FILE *f)
/* Help dump out results */
{
spaceOut(f, level*2);
struct slDouble *el = (struct slDouble *)ht->itemOrCluster;
if (ht->left || ht->right)
    {
    fprintf(f, "(%g)\n", el->val);
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
uglyf("dblDistance %g %g\n", i1->val, i2->val);
return abs(i1->val - i2->val);
}

int dblCmp(const struct slList *item1, const struct slList *item2, void *extraData)
{
struct slDouble *i1 = (struct slDouble *)item1;
struct slDouble *i2 = (struct slDouble *)item2;
uglyf("dblDistance %g %g\n", i1->val, i2->val);
double v1 = i1->val, v2 = i2->val;
double diff = v1 - v2;
if (diff < 0)
    return -1;
else if (diff > 0)
    return 1;
else 
    return 0;
}

struct slList *dblMerge(const struct slList *item1, const struct slList *item2, 
    void *extraData)
{
struct slDouble *i1 = (struct slDouble *)item1;
struct slDouble *i2 = (struct slDouble *)item2;
uglyf("dblMerge %g %g\n", i1->val, i2->val);
return (struct slList *)slDoubleNew((i1->val + i2->val) * 0.5);
}

void freen(char *output)
/* Do something, who knows what really */
{
FILE *f = mustOpen(output, "w");
int i;

/* Make up list of random numbers */
struct slDouble *list = NULL;
for (i=0; i<10; ++i)
    {
    struct slDouble *el = slDoubleNew(rand()%100);
    slAddHead(&list, el);
    }
struct lm *lm = lmInit(0);
struct hacTree *ht = hacTreeFromItems((struct slList *)list, lm, dblDistance, dblMerge, 
    dblCmp, NULL);
rDump(ht, 0, f);
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
