/* freen - My Pet Freen. */
#include <sys/statvfs.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen input\n");
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void freen(char *inFile, char *outFile)
{
struct statvfs fi;
int err = statvfs(outFile,&fi);
printf("err %d, errno %d, strerror(errno) %s\n", err, errno, strerror(errno));
printf("f_bsize=%lld, f_bavail=%lld, total=%lld\n",
    (long long)fi.f_bsize, (long long)fi.f_bavail, (long long)fi.f_bsize * fi.f_bavail);
int s = mustOpenFd(inFile, O_RDONLY);
int d = mustOpenFd(outFile, O_CREAT | O_WRONLY);
cpFile(s, d);
mustCloseFd(&s);
mustCloseFd(&d);
}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
freen(argv[1], argv[2]);
return 0;
}
