/* paraMd5 - Make md5 sums for files in parallel.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "md5.h"
#include "pthreadDoList.h"

int threads = 5;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paraMd5 - Make md5 sums for files in parallel.\n"
  "usage:\n"
  "   paraMd5 file(s)\n"
  "options:\n"
  "   -threads=N - number of threads - default %d\n"
  , threads
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"threads", OPTION_INT},
   {NULL, 0},
};

struct paraMd5
/* Keep track of a number and it's Nth power in parallel */
    {
    struct paraMd5 *next;
    char *fileName; /* Name of file */
    char *md5;	    /* Md5sum */
    };

void doOneMd5(void *item, void *context)
/* This routine does the actual work on each paraMd5 item. */
{
struct paraMd5 *p = item; // Convert item to known type
p->md5 = md5HexForFile(p->fileName);
verbose(1, ".");
}


void paraMd5(int fileCount, char *files[])
/* paraMd5 - Make md5 sums for files in parallel.. */
{
/* Make a list of things we want to process in parallel */
struct paraMd5 *list = NULL, *el;
int i;
for (i=0; i<fileCount; ++i)
    {
    AllocVar(el);
    el->fileName = files[i];
    slAddHead(&list, el);
    }
slReverse(&list);

/* Do the parallel execution */
pthreadDoList(threads, list, doOneMd5, NULL);
verbose(1, "\n");

/* Print out results */
for (el = list; el != NULL; el = el->next)
    {
    printf("%s  %s\n", el->md5, el->fileName);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 2)
    usage();
threads = optionInt("threads", threads);
paraMd5(argc-1, argv+1);
return 0;
}
