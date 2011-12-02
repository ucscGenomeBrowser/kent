#include "gbVerb.h"
#include "common.h"
#include "errabort.h"
#include "jksql.h"
#include <time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/time.h>


struct times
/* used to record time */
{
    double real;
    double selfUserCpu;
    double selfSysCpu;
    double childUserCpu;
    double childSysCpu;
};

#define INDENT_AMT 2  /* characters per indent level */

unsigned gbVerbose = 0;  /* verbose level */ 
static boolean initialized = FALSE;
static unsigned indent = 0;      /* indentation level */
static double progStartTime;         /* prog start time */
static struct times startTimes[20];    /* start times for each level */

static double timevalToSec(struct timeval tval)
/** Convert timeval to seconds. */
{
return tval.tv_sec + (tval.tv_usec / 1000000.0);
}

static double getTimeOfDay()
/* Get current real time, in seconds */
{
struct timeval realTime;

gettimeofday(&realTime, NULL);
return timevalToSec(realTime);
}

static double getRealSec()
/* Get elapsed real time, in seconds */
{
return getTimeOfDay() - progStartTime;
}

static struct times timesGetCur()
/* get timeRec with current times */
{
struct times tr;
struct rusage ruSelf, ruChild;

getrusage(RUSAGE_SELF, &ruSelf);
getrusage(RUSAGE_CHILDREN, &ruChild);

tr.real = getTimeOfDay();
tr.selfUserCpu = timevalToSec(ruSelf.ru_utime);
tr.selfSysCpu = timevalToSec(ruSelf.ru_stime);
tr.childUserCpu = timevalToSec(ruChild.ru_utime);
tr.childSysCpu = timevalToSec(ruChild.ru_utime);
return tr;
}

struct times timesElapsed(struct times *start)
/* compute elapsed times since start */
{
struct times end = timesGetCur();
struct times tr;

tr.real = end.real - start->real;
tr.selfUserCpu = end.selfUserCpu - start->selfUserCpu;
tr.selfSysCpu = end.selfSysCpu - start->selfSysCpu;
tr.childUserCpu = end.childUserCpu - start->childUserCpu;
tr.childSysCpu = end.childSysCpu - start->childSysCpu;
return tr;
}

static char *dateTimeStr()
/* get string with date/time.  WARNING: static return */
{
static char buf[64];
time_t epoc = time(NULL);
strftime(buf, sizeof(buf), "%Y-%m-%dT%T", localtime(&epoc));
return buf;
}

#if 0
static double getVMSize()
/* Return the process virtual memory, in floating point megabytes */
{
/* parse size out of /proc stat file */
unsigned long vsize = 0;
FILE *fh = fopen("/proc/self/stat", "r");

if (fh != NULL)
    {
    char buf[1024];
    if (fgets(buf, sizeof(buf), fh) != NULL)
        {
        char* words[30];
        int numWords = chopLine(buf, words);
        if (numWords > 22)
            {
            char* stop;
            unsigned long num = strtoul(words[22], &stop, 10);
            if ((stop != buf) && (*stop == '\0'))
                vsize = num;
            }
        }
    fclose(fh);
    }
return ((double)vsize)/(1024.0*1024.0);
}
#endif

static void prIndent()
/* Print indentation */
{
assert(initialized);
fprintf(stderr, "%.*s", indent*INDENT_AMT,
        "                                               ");
}

void gbVerbInit(int level)
/* Set verbose level and initialize start time */
{
progStartTime = getTimeOfDay();
gbVerbose = level;
fflush(stderr);
setlinebuf(stderr);
initialized = TRUE;
}

void gbVerbEnter(int level, char* msg, ...)
/* If the current verbose level is level or greater, print message.  Increase
 * indentation amount, record start time */
{
va_list args;

if (gbVerbose >= level)
    {
    prIndent();
    fprintf(stderr, "->");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, ": tod=%s\n", dateTimeStr());
    assert(indent < ArraySize(startTimes));
    }
indent++;
sqlMonitorSetIndent(indent*INDENT_AMT);
assert(indent < ArraySize(startTimes));
startTimes[indent] = timesGetCur();
}

void gbVerbLeave(int level, char* msg, ...)
/* If the current verbose level is level or greater, print message.  Decrease
 *  indentation amount.  Must be matched with gbVerbEnter */ 
{
va_list args;
assert((indent > 0) && (indent < ArraySize(startTimes)));
indent--;
sqlMonitorSetIndent(indent*INDENT_AMT);

if (gbVerbose >= level)
    {
    struct times tm = timesElapsed(&startTimes[indent+1]);
    double waitTime = tm.real - (tm.selfUserCpu + tm.selfSysCpu);
    prIndent();
    fprintf(stderr, "<-");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr,
            ": real=%0.2f cpu=%0.2f sys=%0.2f wait=%0.2f childCpu=%0.2f childSys=%0.2f tod=%s\n",
            tm.real, tm.selfUserCpu, tm.selfSysCpu, waitTime,
            tm.childUserCpu, tm.childSysCpu, dateTimeStr());
    }
}

void gbVerbMsg(int level, char* msg, ...)
/* If the current verbose level is level or greater, print message. */ 
{
va_list args;
if (gbVerbose >= level)
    {
    prIndent();
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, ": real=%0.2f\n", getRealSec());
    }
}

void gbVerbPr(int level, char* msg, ...)
/* If the verbose level is level or greater, print message, with no time
 * or resource info included. */ 
{
va_list args;

if (gbVerbose >= level)
    {
    prIndent();
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    }
}

void gbVerbPrStart(int level, char* msg, ...)
/* like gbVerbPr, only doesn't print a newline. */
{
va_list args;

if (gbVerbose >= level)
    {
    prIndent();
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    }
}
void gbVerbPrMore(int level, char* msg, ...)
/* output more after gbVerbPrStart, with no newline */
{
va_list args;

if (gbVerbose >= level)
    {
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

