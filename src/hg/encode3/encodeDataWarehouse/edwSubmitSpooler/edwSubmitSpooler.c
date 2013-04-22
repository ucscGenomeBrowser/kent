/* edwSubmitSpooler - A little program that just handles reading a URL from a file and feeding it to edwSubmit. */
#include <sys/wait.h>
#include <sys/types.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "portable.h"
#include "log.h"

int maxThreadCount = 5;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwSubmitSpooler - A little program that just handles reading a URL from a file and feeding it to edwSubmit\n"
  "usage:\n"
  "   edwSubmitSpooler spoolFifo\n"
  "options:\n"
  "   threads=N (number of threads - default %d)\n"
  , maxThreadCount
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"threads", OPTION_INT},
   {NULL, 0},
};

void edwSubmitSpooler(char *inFile)
/* edwSubmitSpooler - A little program that just handles reading a URL from a file and 
 * feeding it to edwSubmit.  Keeps up to maxThreadCount edwSubmits going at once. */
{
/* Set up array with a slot for each simultaneous job. */
int runners[maxThreadCount];  /* Job table - 0 for open slots*/
memset(runners, 0, sizeof(runners));
int curThreads = 0;

/* Open our file, which really should be a named pipe. */
FILE *f = mustOpen(inFile, "r");
int dummyFd = mustOpenFd(inFile, O_WRONLY); // Keeps pipe from generating EOF when empty

for (;;)
    {
    char buf[2048];
    if (fgets(buf, sizeof(buf), f) == NULL)
        break;	// This actually doesn't happen with the named pipe, but does with regular file
    char *line = buf;
    char *user = nextWord(&line);
    char *url = trimSpaces(line);
    int i;
    if (curThreads >= maxThreadCount)
        {
	int status;
	int child = wait(&status);
	for (i=0; i<maxThreadCount; ++i)
	    {
	    if (runners[i] == child)
		{
	        runners[i] = 0;
		break;
		}
	    }
	--curThreads;
	}
    int child = mustFork();
    if (child == 0)
        {
	char *cmd[] = {"edwSubmit", url, user, NULL};
	execvp(cmd[0], cmd);
	errnoAbort("Couldn't exec edwSubmit");
	}
    for (i=0; i<maxThreadCount; ++i)
        {
	if (runners[i] == 0)
	    {
	    runners[i] = child;
	    break;
	    }
	}
    ++curThreads;
    }
close(dummyFd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
maxThreadCount = optionInt("threads", maxThreadCount);
if (argc != 2)
    usage();
edwSubmitSpooler(argv[1]);
return 0;
}
