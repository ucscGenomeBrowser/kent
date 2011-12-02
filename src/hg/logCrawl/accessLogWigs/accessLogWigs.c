/* accessLogWigs - Create wiggle graphs of access logs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "portable.h"
#include "apacheLog.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "accessLogWigs - Create wiggle graphs of access logs\n"
  "usage:\n"
  "   accessLogWigs startTime 1.log 2.log ... N.log outDir\n"
  "Will create 1Hits.wig, 1Response.wig, etc in outDir.\n"
  "Start time will be the starting time of the graph in the\n"
  "format of 27/Aug/2009:09:25:32 (which is the same as the\n"
  "access log time format.)."
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct tickResponse
/* Tick and response time. */
    {
    struct tickResponse *next;
    time_t tick;
    int responseMillis;
    };

int tickResponseCmpTick(const void *va, const void *vb)
/* Compare to sort based on query start. */
{
const struct tickResponse *a = *((struct tickResponse **)va);
const struct tickResponse *b = *((struct tickResponse **)vb);
if (a->tick < b->tick)
    return -1;
else if (a->tick == b->tick)
    return 0;
else
    return 1;
}

struct tickResponse *tickResponseFromLine(struct lineFile *lf, char *line, struct lm *lm)
/* Read line from file and create tick/response from it. */
{
struct apacheAccessLog *a = apacheAccessLogParse(line, lf->fileName, lf->lineIx);
if (a != NULL)
    {
    struct tickResponse *tr;
    lmAllocVar(lm, tr);
    tr->tick = a->tick;
    tr->responseMillis = a->runTime;
    apacheAccessLogFree(&a);
    return tr;
    }
return NULL;
}

void printVarStepHead(FILE *f)
/* Print variable step header. */
{
fprintf(f, "variableStep chrom=chr1 span=1\n");
}

void tickResponseToResponseWig(struct tickResponse *list, time_t startTick, char *outName)
/* Create varStep wig file based on tickRespons list.  This contains average response
 * time seen at each second where there is data. */
{
FILE *f = mustOpen(outName, "w");
printVarStepHead(f);
struct tickResponse *el;
time_t curTick = 0;
int tickTotal = 0, sameTickCount = 0;
for (el = list; el != NULL; el = el->next)
   {
   if (curTick != el->tick)
       {
       if (sameTickCount > 0)
	   fprintf(f, "%ld\t%f\n", (long)curTick - startTick + 1, 0.001*tickTotal/sameTickCount);
       curTick = el->tick;
       tickTotal = 0;
       sameTickCount = 0;
       }
   tickTotal += el->responseMillis;
   sameTickCount += 1;
   }
if (sameTickCount > 0)
   fprintf(f, "%ld\t%f\n", (long)curTick - startTick + 1, 0.001*tickTotal/sameTickCount);
carefulClose(&f);
}

void tickResponseToHitWig(struct tickResponse *list, time_t startTick, char *outName)
/* Create varStep wig file based on tickResponse list.  This contains number of hits
 * per second. */
{
FILE *f = mustOpen(outName, "w");
printVarStepHead(f);
time_t curTick = startTick-1;
int sameTickCount = 0;
struct tickResponse *el;
for (el = list; el != NULL; el = el->next)
    {
    if (curTick != el->tick)
       {
       if (sameTickCount > 0)
	   fprintf(f, "%ld\t%d\n", curTick - startTick + 1, sameTickCount);
       sameTickCount = 0;
       time_t i;
       for (i=curTick+1; i<el->tick; ++i)
	   {
	   fprintf(f, "%ld\t%d\n", i - startTick + 1, 0);
	   }
       curTick = el->tick;
       }
    sameTickCount += 1;
    }
if (sameTickCount > 0)
   fprintf(f, "%ld\t%d\n", curTick - startTick + 1, sameTickCount);
carefulClose(&f);
}

void oneLogToTwoWigs(time_t startTick, char *logIn, char *hitOut, char *responseOut)
/* Read log file and based on it create hit and response time wigs. */
{
struct lineFile *lf = lineFileOpen(logIn, TRUE);
struct tickResponse *list = NULL, *el;
char *line;
struct lm *lm = lmInit(0);
while (lineFileNextReal(lf, &line))
    {
    el = tickResponseFromLine(lf, line, lm);
    if (el != NULL)
        slAddHead(&list, el);
    }
lineFileClose(&lf);
slReverse(&list);
slSort(&list, tickResponseCmpTick);

verbose(1, "Processing %d elements in %s\n", slCount(list), logIn);
tickResponseToResponseWig(list, startTick, responseOut);
tickResponseToHitWig(list, startTick, hitOut);
lmCleanup(&lm);
}

void accessLogWigs(char *timeString, int inCount, char *inFiles[], char *outDir)
/* accessLogWigs - Create wiggle graphs of access logs. */
{
struct tm tm;
char *res = strptime(timeString, "%d/%b/%Y:%T", &tm);
if (res == NULL)
    errAbort("Badly formatted time stamp %s", timeString);
time_t startTick = mktime(&tm);
char hitsOut[PATH_LEN], responseOut[PATH_LEN];
char dir[PATH_LEN], name[FILENAME_LEN], extension[FILEEXT_LEN];
int i;
makeDir(outDir);
for (i=0; i<inCount; ++i)
    {
    splitPath(inFiles[i], dir, name, extension);
    safef(hitsOut, sizeof(hitsOut), "%s/%sHits.wig", outDir, name);
    safef(responseOut, sizeof(responseOut), "%s/%sResponse.wig", outDir, name);
    oneLogToTwoWigs(startTick, inFiles[i], hitsOut, responseOut);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 4)
    usage();
accessLogWigs(argv[1], argc-3, argv+2, argv[argc-1]);
return 0;
}
