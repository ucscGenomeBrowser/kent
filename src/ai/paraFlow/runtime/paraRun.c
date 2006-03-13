/* paraRun - stuff to help manage para operations on collections.  */
/* The overall design of this system involves a number of threads,
 * all synchronized on synQueues. The threads come in three sorts,
 * a manager, workers, and customers.  The customers send a
 * run-request message to the manager. This message includes a
 * description of a collection and the job to be run on each
 * item in the collection. The manager breaks up the job into
 * bundles which typically contain more than a single item, and
 * assigns bundles to workers. The workers send a bundle-done message
 * containing the bundle and information on the time they spent on
 * it back to the manager when they are done. When the job is finished
 * the manager sends a run done message back to the client.
 *
 * There are three types of message queues:
 *     manager's queue:
 *        reader: manager
 *        writers: workers (bundle message)
 *                 customers (run message)
 *     workers's queue:
 *        reader: workers
 *        writer: manager (bundle message)
 *     customer's queue:
 *        reader: customer
 *        writer: manager (run message)
 * Each of the three types of threads will wait on their respective
 * queues.  The customer queue is transient though.  It only exists
 * for the single run done message.
 *
 * A wrinkle in all of this is that the worker threads can become
 * customers when they encounter another para statement during the
 * course of their execution. To keep the CPUs occupied, the manager
 * will add a new worker thread to the workers queue when a customer
 * comes in, which corresponds to the CPU that the customer used to
 * be using. When a worker comes in with the final bundle to finish
 * the customer request, the manager removes that worker thread.
 * The removed threads aren't actually destroyed, but put in a
 * free thread pool, since creating threads does actually take a 
 * little time. */

#include "common.h"
#include "hash.h"
#include "dlist.h"
#include "../compiler/pfPreamble.h"
#include "pthreadWrap.h"
#include "synQueue.h"
#include "cacheQueue.h"

enum messageType
/* Different message types. */
    {
    mRun = 111,
    mBundle = 222,
    };

enum collectionType
/* Different types of collections. */
    {
    cDir,
    cArray,
    cRange,
    };

struct messageHeader
/* Shared start of message */
    {
    enum messageType type;
    };

struct paraRun
/* Information on a job to do to all items in a collection, and
 * some stuff to help us manage the work. */
    {
    enum messageType messageType;	/* Always mRun */
    long itemCount;			/* Element count */
    int itemSize;				/* Size each element */
    double totalRunTime;	/* Total job run time (finished only). */
    long itemsSubmitted;	/* Number of items submitted to threads. */
    long itemsFinished;		/* Number of items finished by threads. */
    struct synQueue *customerQueue;	/* Where customer waits. */
    void *localVars;		/* Local variables in caller. */
    void (*process)(void *item, void *localVars);  /* Process function */
    struct dlNode *node;	/* Node in managers activeRun list */
    enum collectionType collectionType;	   /* Collection type. */

    /* The rest of this depends on the type.  A union here would be
     * slightly more efficienct in terms of memory, but not enough to
     * justify the increased lines of code. */
    struct _pf_dir *dir;			/* Dir for dirs. */
    struct hashCookie hashCookie;		/* Position in dir */
    struct _pf_array *array;			/* Array for arrays */
    long arrayIx;				/* Position in array */
    long rangeStart;				/* Range info */
    long rangeEnd;				/*   "    "   */
    long rangeIx;				/* Position in range */
    };

struct jobBundle
/* A bundle of jobs that can be taken by our worker threads. */
    {
    enum messageType messageType;	/* Always mBundle */
    int itemCount;	/* Count of items. */
    int itemSize;	/* Size of each item. */
    void *items;	/* An array of items to process. */
    void *itemsToFree;	/* For locally allocated items, we'll free these */
    void *localVars;	/* Local variables in caller. */
    void (*process)(void *item, void *localVars);  
    	/* A function that processes one item. */
    struct timeval startTime;	/* Start time (wall clock) */
    struct timeval endTime;		/* End time (wall clock). */
    time_t startCpu;		/* Start CPU time. */
    time_t endCpu;		/* End CPU time. */
    struct worker *worker;	/* The worker on this one. */
    struct paraRun *run;	/* Run this is part of. */
    };

struct worker
/* Information on one worker. */
    {
    struct synQueue *queue;	/* Worker waits on this. */
    pthread_t thread;	/* Worker's thread. */
    struct dlNode *node;	/* Node in double linked list. */
    };
static struct worker *workerNew();

struct manager
/* Information on the manager. */ 
    {
    struct synQueue *queue;	/* Can contain two types of message */
    pthread_t thread;	/* Manager's thread. */
    struct dlList *activeWorkers;	/* Workers in action. */
    struct dlList *readyWorkers; 	/* Workers wanting a job. */
    struct dlList *reserveWorkers;	/* Workers in reserve. */
    struct dlList *activeRuns;		/* Currently active runs. */
    int cpuCount;
    };
static struct manager *managerNew(int cpuCount);

static struct manager *manager;
static int paraCpuCount = 2;


void _pf_paraRunInit()
/* Initialize job scheduler. */
{
/* TODO - figure out actual CPU count. */
manager = managerNew(paraCpuCount);
}

void jobBundleRun(struct jobBundle *job)
/* Run job on all items. */
{
char *item = job->items;
int i;
job->startCpu = clock();
gettimeofday(&job->startTime, NULL);
for (i=0; i<job->itemCount; ++i)
    {
    job->process(item, job->localVars);
    item += job->itemSize;
    }
gettimeofday(&job->endTime, NULL);
job->endCpu = clock();
}

static void *workWork(void *v)
/* Work away at queue endlessly. */
{
struct worker *w = v;
for (;;)
    {
    struct jobBundle *job = synQueueGet(w->queue);
    uglyf("Got work type %d, jobs %d\n", (int)(job->messageType), job->itemCount);
    jobBundleRun(job);
    synQueuePut(manager->queue, job);
    }
}

struct worker *workerNew()
/* Create a new worker */
{
struct worker *worker;
AllocVar(worker);
worker->queue = synQueueNew();
pthreadCreate(&worker->thread, NULL, workWork, worker);
AllocVar(worker->node);
worker->node->val = worker;
return worker;
}


void *getDirJobs(struct _pf_dir *dir, struct hashCookie cookie, int size)
/* Allocate an array of size, and fill it with next bunch of items from
 * hash. */
{
struct _pf_base *base = dir->elType->base;
int itemSize = base->size, i;
void *buf = needLargeMem(size*itemSize);
if (base->needsCleanup)
    {
    struct _pf_object **pt = buf;
    for (i=0; i<size; ++i)
        {
	struct hashEl *hel = hashNext(&cookie);
	*pt++ = hel->val;
	}
    }
else
    {
    char *pt = buf;
    for (i=0; i<size; ++i)
        {
	struct hashEl *hel = hashNext(&cookie);
	memcpy(pt, hel->val, itemSize);
	pt += itemSize;
	}
    }
return buf;
}

struct jobBundle *paraRunGetNextJob(struct paraRun *run, struct worker *worker)
/* Make up next job structure looking at average run times and
 * available threads */
{
struct jobBundle *job;
long itemsInBundle;
long itemsLeft = run->itemCount - run->itemsSubmitted;

if (itemsLeft == 0)
    return NULL;
if (run->itemsFinished == 0)
    {
    uglyf("paraRunGetNextJob no one finished yet.\n");
    itemsInBundle = run->itemCount/(paraCpuCount*10);
    }
else if (run->totalRunTime > 0.0001)
    {
    uglyf("total run time is %f, aveTime %f\n", run->totalRunTime, run->totalRunTime/run->itemsFinished);
    itemsInBundle = 0.005*run->itemsFinished/run->totalRunTime;
    }
else
    {
    uglyf("Short items, assigning a bunch\n");
    itemsInBundle = run->itemCount/paraCpuCount;
    }
if (itemsInBundle < 1)
    itemsInBundle = 1;
if (itemsInBundle > itemsLeft)
    itemsInBundle = itemsLeft;

uglyf("paraRunGetNextJob itemsInBundle %ld, itemsLeft %ld\n", itemsInBundle, itemsLeft);
AllocVar(job);
job->messageType = mBundle;
job->itemCount = itemsInBundle;
job->itemSize = run->itemSize;
job->localVars = run->localVars;
job->process = run->process;
job->worker = worker;
job->run = run;
switch (run->collectionType)
    {
    case cDir:
	job->items = job->itemsToFree = 
		getDirJobs(run->dir, run->hashCookie, itemsInBundle);
        break;
    case cArray:
	job->items = run->array->elements + run->itemSize*run->arrayIx;
	run->arrayIx += itemsInBundle;
        break;
    case cRange:
	{
	_pf_Long i, *pt, end;
	job->items = job->itemsToFree = pt = 
		needLargeMem(itemsInBundle*sizeof(_pf_Long));
	end = run->rangeIx + itemsInBundle;
	for (i=run->rangeIx; i<end; ++i)
	    *pt++ = i;
        break;
	}
    }
run->itemsSubmitted += itemsInBundle;
return job;
}

static double tvToDouble(struct timeval *tv)
/* Return timeval in seconds as a double-precision floating point number. */
{
double res = tv->tv_usec * 0.000001;
res += tv->tv_sec;
return res;
}

void jobBundleFree(struct jobBundle **pJob)
/* Free up a job. */
{
struct jobBundle *job = *pJob;
if (job != NULL)
    {
    freeMem(job->itemsToFree);
    freez(pJob);
    }
}

static void scheduleRun(struct manager *manager, struct paraRun *run)
/* Add run to active list. Pull processor off of reserve list to
 * start it off.  Also grab any workers on the ready list. */
{
struct dlNode *workerNode;
struct worker *worker;
struct jobBundle *job;

/* Set up job on active run list. */
AllocVar(run->node);
run->node->val = run;
dlAddTail(manager->activeRuns, run->node);

workerNode = dlPopHead(manager->reserveWorkers);
if (workerNode == NULL)
     {
     worker = workerNew();
     workerNode = worker->node;
     }
else
    worker = workerNode->val;
dlAddTail(manager->activeWorkers, workerNode);
job = paraRunGetNextJob(run, worker);
synQueuePut(worker->queue, job);

/* Loop through putting any other ready workers on the job. */
while ((workerNode = dlPopHead(manager->readyWorkers)) != NULL)
    {
    worker = workerNode->val;
    job = paraRunGetNextJob(run, worker);
    if (job)
	{
	dlAddTail(manager->activeWorkers, workerNode);
	synQueuePut(worker->queue, job);
	}
    else
        {
	dlAddHead(manager->readyWorkers, workerNode);
	break;
	}
    }
}

void foldBundleIntoRun(struct paraRun *run, struct jobBundle *job)
/* Add information from bundle into run. */
{
double dt = tvToDouble(&job->endTime) - tvToDouble(&job->startTime);
if (dt < 0)
    {	/* We wrapped - it's a new day! */
    dt += 24*3600;
    if (dt > 1000 || dt < 0)  /* Sanity check. */
	dt = 0.01;	      /* Default to our "optimal" time */
    }
run->totalRunTime += dt;
run->itemsFinished += job->itemCount;
}

static void checkWork(struct manager *manager, struct jobBundle *job)
/* See if it's the last job in the bundle.  If so then move the
 * worker to reserve, and report job done to customer.  Otherwise
 * do more work on this run if there is work left to start. Otherwise
 * see if there's another active run worker can go to. Otherwise put
 * worker on ready list. */
{
struct paraRun *run = job->run;
struct worker *worker = job->worker;

uglyf("checkWork %d, %d\n", job->messageType, job->itemCount);
foldBundleIntoRun(run, job);
jobBundleFree(&job);
if (run->itemsFinished == run->itemCount)
    {
    uglyf("finished one run of %d\n", (int)run->itemCount);
    dlRemove(run->node);	/* Remove node from active run list */
    freez(&run->node);
    synQueuePut(run->customerQueue, run);
    dlRemove(worker->node);
    dlAddTail(manager->reserveWorkers, worker->node);
    }
else if (run->itemsSubmitted < run->itemCount)
    {
    uglyf("Submitted %ld of %ld, putting worker back on same run\n", run->itemsSubmitted, run->itemCount);
    job = paraRunGetNextJob(run, worker);
    synQueuePut(worker->queue, job);
    }
else
    {
    struct dlNode *runNode;
    struct paraRun *newRun = NULL;
    for (runNode = manager->activeRuns->head; !dlEnd(runNode); 
    	runNode = runNode->next)
        {
	if (runNode->val != run)
	    {
	    newRun = runNode->val;
	    break;
	    }
	}
    if (newRun)
        {
	uglyf("Putting worker on another run\n");
	job = paraRunGetNextJob(newRun, worker);
	synQueuePut(worker->queue, job);
	}
    else
        {
	uglyf("Idling worker\n");
	dlRemove(worker->node);
	slAddTail(manager->readyWorkers, worker->node);
	}
    }
}

static void *manageManage(void *v)
/* Manage away on our message queue. */
{
struct manager *manger = v;
for (;;)
    {
    enum messageType type;
    struct messageHeader *message = synQueueGet(manager->queue);
    uglyf("Manager got message %d\n", (int)message->type);
    if (message->type == mRun)
	scheduleRun(manager, (struct paraRun *)message);
    else
        checkWork(manager, (struct jobBundle *)message);
    }
}

static struct manager *managerNew(int cpuCount)
/* Create a new manager for a number of CPUs. */
{
struct manager *manager;
struct worker *worker;
int i;
AllocVar(manager);
manager->queue = synQueueNew();
manager->activeRuns = dlListNew();
manager->activeWorkers = dlListNew();
manager->readyWorkers = dlListNew();
manager->reserveWorkers = dlListNew();
manager->cpuCount = cpuCount;

/* Make one less worker ready than we have CPUs.  This is
 * because one CPU is already occupied.  We'll get it back
 * when it submits a batch of jobs to us though... */
for (i=1; i<cpuCount; ++i)
    {
    worker = workerNew();
    dlAddTail(manager->readyWorkers, worker->node);
    }
pthreadCreate(&manager->thread, NULL, manageManage, manager);
return manager;
}

static void paraDoRun(struct paraRun *run)
/* Send run to manager and wait for results. */
{
synQueuePut(manager->queue, run);
synQueueGet(run->customerQueue);
}

static void paraRunFree(struct paraRun **pRun)
/* Free up a paraRun. */
{
struct paraRun *run = *pRun;
if (run != NULL)
    {
    cacheQueueFree(&run->customerQueue);
    freez(pRun);
    }
}

static struct paraRun *paraRunNew(void *localVars, 
	void (*process)(void *item, void *localVars))
/* Set up shared parts of paraRun structure. */
{
struct paraRun *run;
AllocVar(run);
run->messageType = mRun;
run->customerQueue = cacheQueueAlloc();
run->localVars = localVars;
run->process = process;
return run;
}

void _pf_paraRunArray(struct _pf_array *array, 
	void *localVars, void (*process)(void *item, void *localVars))
/* Build up run structure on array. */
{
struct paraRun *run = paraRunNew(localVars, process);
run->collectionType = cArray;
run->itemCount = array->size;
run->itemSize = array->elSize;
run->array = array;
paraDoRun(run);
paraRunFree(&run);
}

void _pf_paraRunDir(struct _pf_dir *dir,
	void *localVars, void (*process)(void *item, void *localVars))
/* Build up run structure on array. */
{
struct paraRun *run = paraRunNew(localVars, process);
run->collectionType = cDir;
run->itemCount = dir->hash->elCount;
run->itemSize = dir->elType->base->size;
run->dir = dir;
run->hashCookie = hashFirst(dir->hash);
paraDoRun(run);
paraRunFree(&run);
}

void _pf_paraRunRange(int start, int end,
	void *localVars, void (*process)(void *item, void *localVars))
/* Build up run structure on range. */
{
struct paraRun *run = paraRunNew(localVars, process);
run->collectionType = cRange;
run->itemCount = end - start;
run->itemSize = sizeof(_pf_Long);
run->rangeIx = run->rangeStart = start;
run->rangeEnd = end;
paraDoRun(run);
paraRunFree(&run);
}

