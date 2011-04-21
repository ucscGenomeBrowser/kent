/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "jksql.h"
#include "trackDb.h"
#include "hui.h"
#include "correlate.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

void freen(char *track)
/* Test some hair-brained thing. */
{
uglyTime(NULL);
int fd = mustOpenFd(track, O_RDONLY);
uglyTime("Opened %s to %d\n", track, fd);
}

int main(int argc, char *argv[])
/* Process command line. */
{
// optionInit(&argc, argv, options);

if (argc != 2)
    usage();
freen(argv[1]);
return 0;
}
