#include "gbVerb.h"
#include "common.h"
#include "errabort.h"
#include "jksql.h"
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

static char const rcsid[] = "$Id: gbVerb.c,v 1.1 2003/06/03 01:27:46 markd Exp $";

#define INDENT_AMT 2  /* characters per indent level */

unsigned verbose = 0;  /* verbose level */ 

static boolean initialized = FALSE;
static unsigned indent = 0;      /* indentation level */
static double startTime;         /* prog start time */
static double startTimes[20];    /* start times for each level */

static double timevalToSec(struct timeval* tval)
/** Convert timeval to seconds. */
{
return tval->tv_sec + (tval->tv_usec / 1000000.0);
}

static double getTimeOfDay()
/* Get current real time, in seconds */
{
struct timeval realTime;

gettimeofday(&realTime, NULL);
return timevalToSec(&realTime);
}

static double getRealSec()
/* Get elapsed real time, in seconds */
{
return getTimeOfDay() - startTime;
}

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
startTime = getTimeOfDay();
verbose = level;
fflush(stderr);
setlinebuf(stderr);
initialized = TRUE;
}

void gbVerbEnter(int level, char* msg, ...)
/* If the current verbose level is level or greater, print message.  Increase
 * indentation amount. */
{
va_list args;
double curTime = getRealSec();

if (verbose >= level)
    {
    prIndent();
    fprintf(stderr, "->");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, ": sec=%0.2f vm=%0.2fm\n", curTime, getVMSize());
    assert(indent < ArraySize(startTimes));
    }
indent++;
sqlTraceIndent = indent*INDENT_AMT;
assert(indent < ArraySize(startTimes));
startTimes[indent] = curTime;
}

void gbVerbLeave(int level, char* msg, ...)
/* If the current verbose level is level or greater, print message.  Decrease
 *  indentation amount.  Must be matched with gbVerbEnter */ 
{
va_list args;
double stepStart;
assert((indent > 0) && (indent < ArraySize(startTimes)));
stepStart = startTimes[indent];
indent--;
sqlTraceIndent = indent*INDENT_AMT;

if (verbose >= level)
    {
    double curTime = getRealSec();
    prIndent();
    fprintf(stderr, "<-");
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, ": sec=%0.2f step=%0.2f vm=%0.2fm\n",
            curTime, (curTime - stepStart), getVMSize());
    }
}

void gbVerbMsg(int level, char* msg, ...)
/* If the current verbose level is level or greater, print message. */ 
{
va_list args;
if (verbose >= level)
    {
    prIndent();
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, ": sec=%0.2f vm=%0.2fm\n", getRealSec(), getVMSize());
    }
}

void gbVerbPr(int level, char* msg, ...)
/* If the verbose level is level or greater, print message, with no time
 * or resource info included. */ 
{
va_list args;

if (verbose >= level)
    {
    prIndent();
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

