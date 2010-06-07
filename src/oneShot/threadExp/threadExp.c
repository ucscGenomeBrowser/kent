/* threadExp - Some pthread experiments. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "pthreadWrap.h"
#include "synQueue.h"
#include "cacheQueue.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "threadExp - Some pthread experiments\n"
  "usage:\n"
  "   threadExp size jobs threads\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

double squa(double a)
{
return a*a;
}

int incer;


#define rtRefIncLock(obj) \
	asm("lock ;" \
		"incl %1;" \
		:"=m" (obj) \
		:"m" (obj) \
		:"memory")

#define rtRefXaddLock(obj) \
        asm("mov $1,%%eax;" \
                "lock; xadd %%eax,%1;" \
                :"=m" (obj) \
                :"m" (obj) \
                :"memory", "%eax")

void bigCalc(double *output, int outSize, int startNum, int endNum)
/* Do a big calculation. */
{
int i;
double d;
assert(outSize == endNum-startNum);
for (i=startNum; i<endNum; ++i)
    {
    int j;
    rtRefXaddLock(incer);
    /*
    rtRefIncLock(incer);
    ++incer;
    d = sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(i))))))));
    d = squa(squa(squa(squa(squa(squa(squa(squa(d))))))));
    *output++ = d;
    */
    }
}

struct synQueue *toDo;
struct synQueue *done;

struct calcJob
/* A calculation job. */
    {
    struct calcJob *next;
    double *output;
    int start, end;
    };

struct calcJob *makeJobs(double *results, int size, int jobCount)
/* Make up jobs. */
{
int part;
int start=0, end=0;
struct calcJob *jobList = NULL, *job;
for (part=0; part<jobCount; ++part)
    {
    end = (part+1)*size/jobCount;
    AllocVar(job);
    job->output = results;
    job->start = start;
    job->end = end;
    slAddHead(&jobList, job);
    results += (end - start);
    start = end;
    }
assert(end == size);
return jobList;
}

void *workerBee(void *v)
/* Wait for  jobs, and then eat them. */
{
struct calcJob *job;
for (;;)
    {
    job = synQueueGet(toDo);
    bigCalc(job->output, job->end - job->start, job->start, job->end);
    synQueuePut(done, job);
    }
}

void doJobs(struct calcJob *jobList)
{
struct calcJob *job;
for (job = jobList; job != NULL; job = job->next)
    synQueuePut(toDo, job);
uglyTime("To load syn queue");
for (job = jobList; job != NULL; job = job->next)
    synQueueGet(done);
}

void threadExp(char *asciiSize, char *asciiJobCount, char *asciiThreadCount)
/* threadExp - Some pthread experiments. */
{
int size = atoi(asciiSize);
int jobCount = atoi(asciiJobCount);
int threadCount = atoi(asciiThreadCount);
pthread_t *threads;
int i;

double *results;
double sumErr = 0;
struct calcJob *jobList, *job;
if (size <= 0)
   usage();
AllocArray(results, size);
AllocArray(threads, threadCount);
toDo = cacheQueueAlloc();
done = cacheQueueAlloc();
jobList = makeJobs(results, size, jobCount);
uglyTime("To init and alloc %d\n", size);
for (i=0; i<threadCount; ++i)
    pthreadCreate(&threads[i], NULL, workerBee, NULL);
uglyTime("Thread spwan time");
doJobs(jobList);
uglyTime("job time");
for (i=0; i<size; ++i)
    sumErr += (results[i]-i);
uglyTime("summing err");
printf("Total error = %e\n", sumErr);
printf("Total incer = %d\n", incer);
}

int main(int argc, char *argv[])
/* Process command line. */
{
uglyTime(NULL);
optionHash(&argc, argv);
if (argc != 4)
    usage();
threadExp(argv[1], argv[2], argv[3]);
return 0;
}
