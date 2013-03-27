/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "meta.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input output\n");
}


void freen(char *input, char *output)
/* Test some hair-brained thing. */
{
uglyTime(NULL);
struct meta *metaList = metaLoadAll(input, "meta", "parent", FALSE, FALSE);
uglyTime("Got %d metas\n", slCount(metaList));
metaWriteAll(metaList, output, 3, FALSE);
uglyTime("Wrote %s\n", output);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
freen(argv[1], argv[2]);
return 0;
}
