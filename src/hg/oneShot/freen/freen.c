/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "pipeline.h"

static char const rcsid[] = "$Id: freen.c,v 1.48 2004/09/24 15:20:32 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen something");
}

struct pipeline *gzipPipe = NULL;

void gzipPipeOpen()
/* Redirect stdout to go through a pipe to gzip. */
{
static char *gzip[] = {"gzip", "-c", "-f", NULL};
gzipPipe = pipelineCreateWrite1(gzip, pipelineInheritFd, NULL);
if (dup2(pipelineFd(gzipPipe), 1) < 0)
   errnoAbort("dup2 to stdout failed");
}


void gzipPipeClose()
/* Finish up gzip pipeline. */
{
if (gzipPipe != NULL)
    {
    fclose(stdout);  /* must close first or pipe will hang */
    pipelineWait(&gzipPipe);
    }
}

   

void freen(char *a)
/* Test some hair-brained thing. */
{
gzipPipeOpen();
uglyf("This is a message from the world famous freen program!\n");
uglyf("Will it compress, who knows?\n");
gzipPipeClose();
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
