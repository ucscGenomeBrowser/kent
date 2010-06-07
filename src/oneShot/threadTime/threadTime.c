/* threadTime - Measure some thread timing.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "pthreadWrap.h"
#include "synQueue.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "threadTime - Measure some thread timing.\n"
  "usage:\n"
  "   threadTime cpuCount\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct synQueue *inQueue, *outQueue;

struct worker
    {
    int _pf_refCount;
    pthread_t thread;
    };
    
void *workWork(void *v)
{
struct worker *worker = v;
for (;;)
    {
    void *message = synQueueGet(inQueue);
    synQueuePut(outQueue, message);
    }
}

#define rtRefIncLock(obj) \
	asm("lock ;" \
		"incl %1;" \
		:"=m" (obj->_pf_refCount) \
		:"m" (obj->_pf_refCount) \
		:"memory")

#define rtRefIncNoLock(obj) \
	asm( "incl %1;" \
		:"=m" (obj->_pf_refCount) \
		:"m" (obj->_pf_refCount) \
		:"memory")

#define rtRefXaddLock(obj) \
	asm("mov $1,%%eax;" \
		"xadd %%eax,%1;" \
		:"=m" (obj->_pf_refCount) \
		:"m" (obj->_pf_refCount) \
		:"memory", "%eax")


struct worker *worker = NULL;
int total = 0;

void threadTime(char *cpuCountAscii)
/* threadTime - Measure some thread timing.. */
{
pthread_mutex_t mutex;
int testSize = 10000;
int cpuCount = atoi(cpuCountAscii);
int i;
if (cpuCount <= 0)
    usage();
inQueue = synQueueNew();
outQueue = synQueueNew();
for (i=1; i<cpuCount; ++i)
    {
    AllocVar(worker);
    pthreadCreate(&worker->thread, NULL, workWork, worker);
    }
pthreadMutexInit(&mutex);
uglyTime("set up");
for (i=0; i<testSize; ++i)
    {
    char *message;
    synQueuePut(inQueue, "hello");
    message = synQueueGet(outQueue);
    }
uglyTime("sync time %d", testSize);
for (i=0; i<testSize; ++i)
    {
    char *message = needMem(64);
    freeMem(message);
    }
uglyTime("Alloc time");

for (i=0; i<1000; ++i)
    {
    struct worker *worker;
    AllocVar(worker);
    pthreadCreate(&worker->thread, NULL, workWork, worker);
    }
uglyTime("Creating 1000 threads");

for (i=0; i<1000000; ++i)
    {
    pthreadMutexLock(&mutex);
    pthreadMutexUnlock(&mutex);
    }
uglyTime("Locking and unlocking mutex w/o contention %d times", 1000000);
for (i=0; i<100000000; ++i)
    {
    total += 1;
    }
uglyTime("Incing total 100,000,000 times. Result is %d", total);

for (i=0; i<10000000; ++i)
    {
    worker->_pf_refCount += 1;
    }
uglyTime("Incing worker->_pf_refCount 10,000,000x, result is %d", worker->_pf_refCount);

worker->_pf_refCount = 0;
for (i=0; i<10000000; ++i)
    {
    rtRefIncLock(worker);
    }
uglyTime("rtRefIncLock worker->_pf_refCount 10,000,000x, result is %d", worker->_pf_refCount);

worker->_pf_refCount = 0;
for (i=0; i<10000000; ++i)
    {
    rtRefIncNoLock(worker);
    }
uglyTime("rtRefIncNoLock worker->_pf_refCount 10,000,000x, result is %d", worker->_pf_refCount);

worker->_pf_refCount = 0;
for (i=0; i<10000000; ++i)
    {
    rtRefXaddLock(worker);
    }
uglyTime("rtRefXaddLock worker->_pf_refCount 10,000,000x, result is %d", worker->_pf_refCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
uglyTime(NULL);
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
threadTime(argv[1]);
return 0;
}
