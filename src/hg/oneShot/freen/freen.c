/* freen - My Pet Freen. */
#include <sys/wait.h>
#include <sys/types.h>
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "pipeline.h"

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
/* Need to do I/O that will wait on file to have something */
FILE *f = mustOpen(inFile, "r");
int dummyFd = mustOpenFd(inFile, O_WRONLY);
char buf[1024];
for (;;)
    {
    char *s = fgets(buf, sizeof(buf), f);
    if (s != NULL)
	fputs(buf, stdout);
    else
	{
        fprintf(stderr, "s = NULL, rats\n");
	sleep(1);
	}
    }
carefulClose(&f);
close(dummyFd);
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
