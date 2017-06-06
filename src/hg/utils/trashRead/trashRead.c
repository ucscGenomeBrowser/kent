/* trashRead - test read time for files in trash/ directory. */
#include <limits.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "portable.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trashRead - test read time for files in trash/ directory\n"
  "usage:\n"
  "   trashRead [options] <testDir> <numFiles>\n"
  "options:\n"
  "   -xxx=XXX - no options defined yet\n\n"
  "Reading specified number of files from the specified test directory.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

size_t readBufferSize = 400000000;  /* maximum size of file to read */

static void seedDrand()
/* seed the drand48 function with bytes from /dev/random */
{
long int seed = -1;
FILE *fd = mustOpen("/dev/random", "r");
fread((unsigned char *)&seed, sizeof(long int), 1, fd);
carefulClose(&fd);
verbose(2, "#seed: 0x%0lx = %ld\n", seed, seed);
srand48(seed);
}

static void trashRead(char *testDir, int numFiles)
/* trashRead - test read time for files in trash/ directory. */
{
int i = 0;
int entriesInDir = 0;
char **fileNames;
long startTime = clock1000();
struct fileInfo *file, *fileList = listDirX(testDir, "*", FALSE);
entriesInDir = slCount(fileList);  /* count limit 2^31 = 2,147,483,648 */
fileNames = (char **)needMem(sizeof(char *) * entriesInDir);
long dirReadTime = clock1000();
int filesCounted = 0;
for (file = fileList; file != NULL; file = file->next)
  {
  if (! file->isDir)
    {
    fileNames[filesCounted++] = file->name;
    }
  }
long elapsedTime = dirReadTime - startTime;
if (elapsedTime < 1)
    elapsedTime = 1;
verbose(1, "# directory listing of %d files in %.6f seconds\n", filesCounted, (float)elapsedTime/1000.0);
if (verboseLevel() > 1)
    {
    verbose(2, "# sample file names:\n");
    for (i = 0; i < filesCounted && i < 3; ++i)
        verbose(2, "%d\t%s\n", i+1, fileNames[i]);
    i = 3;
    if ((filesCounted - 3) > 3)
        i = filesCounted - 3;
    if (i > 3)
       verbose(2, " ...\n");
    for (; i < filesCounted; ++i)
        verbose(2, "%d\t%s\n", i+1, fileNames[i]);
    }
startTime = clock1000();
unsigned long long bytesRead = 0;
char *readBuffer = (char *)needMem(sizeof(char) * readBufferSize);
char filePath[PATH_MAX];
for (i = 0; i < numFiles; ++i)
    {
    /* double drand48(void); range: [0.0 : 1.0) */
    int index = filesCounted * drand48();
    safef(filePath, sizeof(filePath), "%s/%s", testDir, fileNames[index]);
    verbose(3, "# %d\t%s\n", index, filePath);
    off_t fileBytes = fileSize(filePath);
    bytesRead += fileBytes;
    FILE *fh = mustOpen(filePath, "r");
    mustRead(fh, readBuffer, fileBytes);
    carefulClose(&fh);
    }
elapsedTime = clock1000() - startTime;
if (elapsedTime < 1)
    elapsedTime = 1;
printf("# read %d files, %llu bytes in %.6f seconds\n", numFiles, bytesRead, (float)elapsedTime/1000.0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
seedDrand();
int numFiles = sqlSigned(argv[2]);
if (numFiles < 0)
    {
    verbose(1, "ERROR: numFiles must be >= 0, given: %d\n", numFiles);
    usage();
    }
trashRead(argv[1], numFiles);
return 0;
}
