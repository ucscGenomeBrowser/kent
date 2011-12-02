/* accessLogFilter - Pass items from input that meet filter criteria to output.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "apacheLog.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "accessLogFilter - Pass items from input that meet filter criteria to output.\n"
  "See accessLogFilter.c for filter criteria\n"
  "usage:\n"
  "   accessLogFilter input.log output.log\n"
  "You can use 'stdin' for input and 'stdout' for output to use with pipes.\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

time_t timeToTick(char *timeString)
/* Convert something like 27/Aug/2009:09:25:32 to Unix timestamp (seconds since 1970) */
{
time_t tick = apacheAccessLogTimeToTick(timeString);
if (tick == 0)
    errAbort("Badly formatted time stamp %s", timeString);
return tick;
}

time_t minTick,maxTick;

boolean filter(struct apacheAccessLog *a)
/* Return TRUE if a passes filter. */
{
return a->tick >= minTick && a->tick < maxTick && a->status == 500;
}

void accessLogFilter(char *input, char *output)
/* accessLogFilter - Pass items from input that meet filter criteria to output.. */
{
minTick = timeToTick("01/Sep/2009:14:00:00");
maxTick = timeToTick("01/Sep/2009:23:30:00");
struct lineFile *lf = lineFileOpen(input, TRUE);
FILE *f = mustOpen(output, "w");
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    struct apacheAccessLog *a = apacheAccessLogParse(line, lf->fileName, lf->lineIx);
    if (a != NULL)
	{
	if (filter(a))
	    fprintf(f, "%s\n", line);
	apacheAccessLogFree(&a);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
accessLogFilter(argv[1], argv[2]);
return 0;
}
