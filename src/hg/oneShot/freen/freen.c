/* freen - My Pet Freen. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <limits.h>
#include <dirent.h>
#include <netdb.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"

static char const rcsid[] = "$Id: freen.c,v 1.35 2003/05/06 07:22:30 kate Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file/dir");
}

void freen(char *host)
/* Test some hair-brained thing. */
{
struct timeval origTime, lastTime, tv;
int count = 0, i;
int diffs[20];


gettimeofday(&origTime, NULL);
gettimeofday(&lastTime, NULL);
for (i=0; i<20; ++i)
    {
    for (;;)
	{
	++count;
	gettimeofday(&tv, NULL);
	if (tv.tv_sec != lastTime.tv_sec || tv.tv_usec != lastTime.tv_usec)
	    {
	    int diff = (lastTime.tv_sec - tv.tv_sec) * 1000000 +
	    		(lastTime.tv_usec - tv.tv_usec);
	    diffs[i] = -diff;
	    lastTime = tv;
	    break;
	    }
	}
    }
for (i=0; i<20; ++i)
    printf("%d\n", diffs[i]);
printf("Count %d, ave %f\n", count, count/20.0);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
