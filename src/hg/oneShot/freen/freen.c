/* freen - My Pet Freen. */
#include <sys/statvfs.h>
#include <uuid/uuid.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "net.h"
#include "paraFetch.h"
#include "obscure.h"
#include "cheapcgi.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *source, char *dest, char *asciiCount)
{
int count = atoi(asciiCount);
if (count == 0)
    {
    puts("copy");
    int fd = netUrlMustOpenPastHeader(source);
    int localFd = mustOpenFd(dest, O_WRONLY | O_CREAT | O_TRUNC);
    cpFile(fd, localFd);
    }
else
    {
    if (!parallelFetch(source, dest, count, 3, FALSE, FALSE))
        errAbort("parallel fetch of %s failed", source);
    }
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
freen(argv[1], argv[2], argv[3]);
return 0;
}
