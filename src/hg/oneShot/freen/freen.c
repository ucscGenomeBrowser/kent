/* freen - My Pet Freen. */
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
         "usage:  freen input output stderr\n");
}


void freen(char *input, char *output, char *errOutput)
/* Test some hair-brained thing. */
{
FILE *f = mustOpen(output, "w");
char *progAndOpts[] = {"wordLine", "-xxx", "stdin", NULL};
struct pipeline *pl = pipelineOpen1(progAndOpts, pipelineRead|pipelineNoAbort, input,  errOutput);
struct lineFile *lf = lineFileAttach(input, TRUE, pipelineFd(pl));
char *line;
int count = 0;
while (lineFileNext(lf, &line, NULL))
    {
    count += 1;
    }
fprintf(f, "count is %d\n", count);
uglyf("Seem to be done\n");
int err = pipelineWait(pl);
uglyf("Past the wait err = %d\n", err);
pipelineFree(&pl);
uglyf("Past the free\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
    usage();
freen(argv[1], argv[2], argv[3]);
return 0;
}
