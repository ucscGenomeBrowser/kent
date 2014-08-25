/* freen - My Pet Freen. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "hacTree.h"
#include "synQueue.h"
#include "pthreadWrap.h"

typedef void *PthreadWorker(void *);  /* A pthread start_routine. */

struct pthreadWorkList
/* Manage threads to work on a list of items in parallel. */
    {
    struct pthreadWorkList *next;   /* Next in list */
    int threadCount;		    /* Number of threads to use */
    struct slList *itemList;	    /* Input list - not allocated here. */
    PthreadWorker *worker;   /* Routine that does work in a single thread */
    pthread_t *pthreads;		    /* Array of pthreads */
    struct synQueue *inQueue;	    /* Put all inputs here */
    int itemCount;		    /* Number of items in itemList */
    int toDoCount;		    /* Number of items left to process. */
    pthread_mutex_t finishMutex;    /* Mutex to prevent simultanious access to finish. */
    pthread_cond_t finishCond;	    /* Conditional to allow waiting until non-empty. */
    };

void pthreadWorkListFree(struct pthreadWorkList **pPwl)
/* Free up memory resources associated with work list */
{
struct pthreadWorkList *pwl = *pPwl;
if (pwl != NULL)
    {
    synQueueFree(&pwl->inQueue);
    freez(&pwl->pthreads);
    freez(pPwl);
    pthreadCondDestroy(&pwl->finishCond);
    pthreadMutexDestroy(&pwl->finishMutex);
    }
}

void *pthreadWorker(void *vWorkList)
/* The worker-bee for a workList thread.  This grabs next input, processes it, and
 * puts result on output queue.  It returns when no more input. */
{
struct pthreadWorkList *pwl = vWorkList;
uglyf("pthreadWorker - pwl = %p, pwl->inQueue %p\n", pwl, pwl->inQueue);
void *item;
while ((item = synQueueGrab(pwl->inQueue)) != NULL)
    {
    /* Do work on the item. */
    pwl->worker(item);

    /* Decrement toDoCount, a protected variable, and wake up manager. */
    pthreadMutexLock(&pwl->finishMutex);
    pwl->toDoCount -= 1;
    pthreadCondSignal(&pwl->finishCond);
    pthreadMutexUnlock(&pwl->finishMutex);
    }
return NULL;
}

void pthreadWorkList(void *workList,  PthreadWorker *worker, int threadCount)
/* Work through list with threadCount workers each in own thread. */
{
/* Allocate basic structure */
struct pthreadWorkList *pwl;
AllocVar(pwl);
pwl->threadCount = threadCount;
pwl->itemList = workList;
pwl->worker = worker;
AllocArray(pwl->pthreads, threadCount);
pwl->inQueue = synQueueNew();

/* Loop through all items in list and add them to inQueue */
struct slList *item;
int count = 0;
for (item = pwl->itemList; item != NULL; item = item->next)
    {
    synQueuePut(pwl->inQueue, item);
    ++count;
    }

/* Initialize item counting stuff */
pwl->itemCount = pwl->toDoCount = count;
pthreadMutexInit(&pwl->finishMutex);
pthreadCondInit(&pwl->finishCond);

/* Start up pthreads */
int i;
for (i=0; i<threadCount; ++i)
    pthreadCreate(&pwl->pthreads[i], NULL, pthreadWorker, pwl);

/* Wait until nothing more to do */
pthreadMutexLock(&pwl->finishMutex);
while (pwl->toDoCount > 0)
    pthreadCondWait(&pwl->finishCond, &pwl->finishMutex);
pthreadMutexUnlock(&pwl->finishMutex);

/* Wait for threads to finish*/
for (i=0; i<threadCount; ++i)
    {
    pthread_join(pwl->pthreads[i], NULL);
    }

/* Clean up */
pthreadWorkListFree(&pwl);
}


typedef double (hacPairingFunction)(struct dlList *list, struct hash *distanceHash, 
    hacDistanceFunction *distF, void *extraData, struct dlNode **retNodeA, struct dlNode **retNodeB);

struct hacTree *hacTreeVirtualPairing(struct slList *itemList, struct lm *localMem,
				 hacDistanceFunction *distF, hacMergeFunction *mergeF,
				 void *extraData, hacPairingFunction *pairingF);
/* Construct hacTree using a method that will minimize the number of calls to
 * the distance and merge functions, assuming they are expensive.  Do a lmCleanup(localMem)
 * to free the returned tree. */

int gThreadCount = 10; /* NUmber of threads */

#define distKeySize 64

void calcDistKey(void *a, void *b, char key[distKeySize])
/* Put key for distance in key */
{
safef(key, distKeySize, "%p %p", a, b);
}

struct hctPair
/* A pair of hacTree nodes and the distance between them */
    {
    struct hctPair *next;  /* Next in list */
    struct hacTree *a, *b;	   /* The pair of nodes */
    double distance;	   /* Distance between pair */
    };


hacDistanceFunction *gDistF;

void *distanceWorker(void *item)
/* fill in distance for pair */
{
struct hctPair *pair = item;
struct hacTree *aHt = pair->a, *bHt = pair->b;
pair->distance = gDistF((struct slList *)(aHt->itemOrCluster), 
    (struct slList *)(bHt->itemOrCluster), NULL);
return NULL;
}


static void precacheMissingDistances(struct dlList *list, struct hash *distanceHash)
/* Force computation of all distances not already in hash */
{
/* Make up list of all missing distances in pairList */
struct synQueue *sq = synQueueNew();
struct hctPair *pairList = NULL, *pair;
struct dlNode *aNode;
for (aNode = list->head; !dlEnd(aNode); aNode = aNode->next)
    {
    struct hacTree *aHt = aNode->val;
    struct dlNode *bNode;
    for (bNode = aNode->next; !dlEnd(bNode); bNode = bNode->next)
        {
	struct hacTree *bHt = bNode->val;
	char key[64];
	calcDistKey(aHt, bHt, key);
	double *pd = hashFindVal(distanceHash, key);
	if (pd == NULL)
	     {
	     AllocVar(pair);
	     pair->a = aHt;
	     pair->b = bHt;
	     slAddHead(&pairList, pair);
	     synQueuePut(sq, pair);
	     }
	}
    }

/* Parallelize distance calculations */
pthreadWorkList(pairList, distanceWorker, gThreadCount);

for (pair = pairList; pair != NULL; pair = pair->next)
    {
    char key[64];
    calcDistKey(pair->a, pair->b, key);
    double *pd = needMem(sizeof(double));
    *pd = pair->distance;
    hashAdd(distanceHash, key, pd);
    }
slFreeList(&pairList);
}

static double pairParallel(struct dlList *list, struct hash *distanceHash, 
    hacDistanceFunction *distF, void *extraData, struct dlNode **retNodeA, struct dlNode **retNodeB)
/* Loop through list returning closest two nodes */
{
gDistF = distF;
precacheMissingDistances(list, distanceHash);
struct dlNode *aNode;
double closestDistance = BIGDOUBLE;
struct dlNode *closestA = NULL, *closestB = NULL;
for (aNode = list->head; !dlEnd(aNode); aNode = aNode->next)
    {
    struct hacTree *aHt = aNode->val;
    struct dlNode *bNode;
    for (bNode = aNode->next; !dlEnd(bNode); bNode = bNode->next)
        {
	struct hacTree *bHt = bNode->val;
	char key[64];
	safef(key, sizeof(key), "%p %p", aHt, bHt);
	double *pd = hashMustFindVal(distanceHash, key);
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
return closestDistance;
}


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
double d = abs(i1->val - i2->val);
uglyf("dblDistance %g %g = %g\n", i1->val, i2->val, d);
return d;
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
#ifdef OLD
struct hacTree *ht = hacTreeForCostlyMerges((struct slList *)list, lm, dblDistance, dblMerge, 
    NULL);
struct hacTree *ht = hacTreeFromItems((struct slList *)list, lm, dblDistance, dblMerge, NULL, NULL);
#endif /* OLD */

struct hacTree *ht = hacTreeVirtualPairing((struct slList *)list, lm, dblDistance, dblMerge, 
    NULL, pairParallel);

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
