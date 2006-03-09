/* threadExp - Some pthread experiments. */
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
  "threadExp - Some pthread experiments\n"
  "usage:\n"
  "   threadExp size threads\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

double squa(double a)
{
return a*a;
}

void bigCalc(double *output, int outSize, int startNum, int endNum)
/* Do a big calculation. */
{
int i;
double d;
uglyf("bigCount %d %d %d\n", outSize, startNum, endNum);
assert(outSize == endNum-startNum);
for (i=startNum; i<endNum; ++i)
    {
    int j;
    d = sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(sqrt(i))))))));
    d = squa(squa(squa(squa(squa(squa(squa(squa(d))))))));
    *output++ = d;
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
    pthread_t thread;
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
toDo = synQueueNew();
done = synQueueNew();
for (job = jobList; job != NULL; job = job->next)
    synQueuePut(toDo, job);
for (job = jobList; job != NULL; job = job->next)
    pthreadCreate(&job->thread, NULL, workerBee, NULL);
for (job = jobList; job != NULL; job = job->next)
    synQueueGet(done);
}

void threadExp(char *asciiSize, char *asciiJobCount)
/* threadExp - Some pthread experiments. */
{
int size = atoi(asciiSize);
int jobCount = atoi(asciiJobCount);
int i;
double *results;
double sumErr = 0;
struct calcJob *jobList, *job;
if (size <= 0)
   usage();
AllocArray(results, size);
jobList = makeJobs(results, size, jobCount);
uglyTime("To init and alloc %d\n", size);
doJobs(jobList);
uglyTime("job time");
for (i=0; i<size; ++i)
    sumErr += (results[i]-i);
uglyTime("summing err");
printf("Total error = %e\n", sumErr);
}

int main(int argc, char *argv[])
/* Process command line. */
{
uglyTime(NULL);
optionHash(&argc, argv);
if (argc != 3)
    usage();
threadExp(argv[1], argv[2]);
return 0;
}
