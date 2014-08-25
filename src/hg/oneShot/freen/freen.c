/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
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
double d = abs(i1->val - i2->val);
uglyf("dblDistance %g %g = %g\n", i1->val, i2->val, d);
return d;
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
double d = 0.5 * (i1->val + i2->val);
uglyf("dblMerge %g %g = %g\n", i1->val, i2->val, d);
return (struct slList *)slDoubleNew(d);
}


void findClosestPair(struct dlList *list, struct hash *distanceHash, 
    hacDistanceFunction *distF, void *extraData, struct dlNode **retNodeA, struct dlNode **retNodeB)
/* Loop through list returning closest two nodes */
{
struct dlNode *aNode;
double closestDistance = BIGDOUBLE;
struct dlNode *closestA = NULL, *closestB = NULL;
for (aNode = list->head; !dlEnd(aNode); aNode = aNode->next)
    {
    struct hacTree *aHt = aNode->val;
    struct slList *a = aHt->itemOrCluster;
    struct dlNode *bNode;
    for (bNode = aNode->next; !dlEnd(bNode); bNode = bNode->next)
        {
	struct hacTree *bHt = bNode->val;
	char key[64];
	safef(key, sizeof(key), "%p %p", aHt, bHt);
	double *pd = hashFindVal(distanceHash, key);
	if (pd == NULL)
	     {
	     lmAllocVar(distanceHash->lm, pd);
	     *pd = distF(a, bHt->itemOrCluster, extraData);
	     hashAdd(distanceHash, key, pd);
	     }
	double d = *pd;
	if (d < closestDistance)
	    {
	    closestDistance = d;
	    closestA = aNode;
	    closestB = bNode;
	    }
	}
    }
*retNodeA = closestA;
*retNodeB = closestB;
}

struct hacTree *hacTreeForCostlyMerges(struct slList *itemList, struct lm *localMem,
				 hacDistanceFunction *distF, hacMergeFunction *mergeF,
				 void *extraData)
/* Construct hacTree using a method that will minimize the number of calls to
 * the distance and merge functions, assuming they are expensive */
{
/* Make up a doubly-linked list in 'remaining' with all items in it */
struct dlList remaining;
dlListInit(&remaining);
struct slList *item;
int count = 0;
struct hash *distanceHash = hashNew(0);
for (item = itemList; item != NULL; item = item->next)
    {
    struct hacTree *ht;
    lmAllocVar(localMem, ht);
    ht->itemOrCluster = item;
    struct dlNode *node;
    lmAllocVar(localMem, node);
    node->val = ht;
    dlAddTail(&remaining, node);
    count += 1;
    }

/* Loop through finding closest and merging until only one node left on remaining. */
int i;
for (i=1; i<count; ++i)
    {
    /* Find closest pair and take them off of remaining list */
    struct dlNode *aNode, *bNode;
    findClosestPair(&remaining, distanceHash, distF, extraData, &aNode, &bNode);
    dlRemove(aNode);
    dlRemove(bNode);

    /* Allocated new hacTree item for them and fill it in with a merged value. */
    struct hacTree *ht;
    lmAllocVar(localMem, ht);
    struct hacTree *left = ht->left = aNode->val;
    struct hacTree *right = ht->right = bNode->val;
    left->parent = right->parent = ht;
    ht->itemOrCluster = mergeF(left->itemOrCluster, right->itemOrCluster, extraData);

    /* Put merged item onto remaining list. */
    struct dlNode *node;
    lmAllocVar(localMem, node);
    node->val = ht;
    dlAddTail(&remaining, node);
    }

/* Clean up and go home. */
hashFree(&distanceHash);
struct dlNode *lastNode = dlPopHead(&remaining);
return lastNode->val;
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
struct hacTree *ht = hacTreeForCostlyMerges((struct slList *)list, lm, dblDistance, dblMerge, 
    NULL);
    // dblCmp, NULL);
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
