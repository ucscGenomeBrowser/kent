/* freen - My Pet Freen. */
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <netdb.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file/dir");
}

void freen(char *host)
/* Test some hair-brained thing. */
{
long start, end;
int i;
start = clock1000();
gethostbyname(host); 
end = clock1000(); 
printf("time 1 = %ld\n", end - start); 
start = clock1000();
for (i=0; i<10; ++i)
    gethostbyname(host);
end = clock1000();
printf("time 10 = %ld\n", end - start);
start = clock1000();
for (i=0; i<100; ++i)
    gethostbyname(host);
end = clock1000();
printf("time 100 = %ld\n", end - start);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
