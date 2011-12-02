/* spSize - Count up number of bases in all sanger read files.. */
#include "common.h"
#include "portable.h"
#include "fa.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "spSize - Count up number of bases in all sanger read files.\n"
  "usage:\n"
  "   spSize rootDir faExt\n");
}

unsigned long totalSize = 0;
int totalCount = 0;

void spSize(char *rootDir, char *faExt, int level)
/* spSize - Count up number of bases in all sanger read files.. */
{
struct fileInfo *fileList, *file;

if (level <= 1)
    printf("%s\n", rootDir);
fileList = listDirX(rootDir, "*", TRUE);
for (file = fileList; file != NULL; file = file->next)
    {
    if (file->isDir)
        spSize(file->name, faExt, level+1);
    else if (endsWith(file->name, faExt))
        {
	FILE *f = mustOpen(file->name, "r");
	DNA *dna;
	int size;
	char *name;
	if (level <= 2)
	    {
	    printf(".");
	    fflush(stdout);
	    }
	while (faFastReadNext(f, &dna, &size, &name))
	    {
	    totalSize += size;
	    totalCount += 1;
	    }
	fclose(f);
	}
    }
slFreeList(&fileList);
if (level <= 1)
    printf("\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
spSize(argv[1], argv[2], 0);
printf("Total count %d\n", totalCount);
printf("Total size %lu\n", totalSize);
return 0;
}
