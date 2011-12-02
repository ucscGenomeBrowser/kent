/* Just a little one shot thing to add file sizes. */
#include "common.h"
#include "portable.h"


int main()
{
char *rootDir = "/projects/cc/hg/gs.2/g2g";
struct fileInfo *hiList, *loList, *hi, *lo;
double size = 0;
int count = 0, countOne;
char pslDir[512];

hiList = listDirX(rootDir, "g2g.*", TRUE);
for (hi = hiList; hi != NULL; hi = hi->next)
    {
    sprintf(pslDir, "%s/psl", hi->name);
    printf("Scanning %s. ", pslDir);
    fflush(stdout);
    loList = listDirX(pslDir, "*_*", FALSE);
    countOne = slCount(loList);
    printf("%d files.\n", countOne);
    count += countOne;
    for (lo = loList; lo != NULL; lo = lo->next)
	size += lo->size;
    }
printf("Total %f bytes in %d files\n", size, count);
}
