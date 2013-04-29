/* freen - My Pet Freen. */
#include <sys/wait.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "jksql.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *inFile)
{
int childId = mustFork();
if (childId == 0)
    {
    int err = system(inFile);
    printf("child says: err = %d\n", err);
    if (err != 0)
	exit(-1);
        // errAbort("system call '%s' had problems", inFile);
    else
	exit(0);
    }
else
    {
    int status = 0;
    printf("parent got childId=%d\n", childId);
    int child = waitpid(-1, &status, 0);
    printf("parent says after waitPid: child=%d, status=%d\n", child, status);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
