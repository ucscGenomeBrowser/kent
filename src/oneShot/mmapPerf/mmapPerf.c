/* mmapPerf - Test perfomance of mmap. */
#define _XOPEN_SOURCE 1
#define _BSD_SOURCE 1
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "verbose.h"
#include <math.h>
#include <time.h>
#include <float.h>
#include <sys/mman.h>

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mmapPerf - Test perfomance of mmap\n"
  "usage:\n"
  "   mmapPerf testFileList what outTsv\n"
  "\n"
  "  - testFileList is a file contained files to test.  The first is use to warm\n"
  "    up the system, the remainder are used to run the tests\n"
  "  - what - label to include in output file\n"
  "  - outTsv - results written to this TSV\n"
  "\n"
  "options:\n"
  "   -accesses=100000 - number of access per test round\n"
  "   -random - set madvise MADV_RANDOM on the mapped file range\n"
  "   -willneed - set madvise MADV_WILLNEED on the mapped file range\n"
  "   -clear - clear file cache\n"
  "   -verbose=n - set verbosity\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"accesses", OPTION_INT},
    {"clear", OPTION_BOOLEAN},
    {"random", OPTION_BOOLEAN},
    {"willneed", OPTION_BOOLEAN},
    {NULL, 0}
};

static double elapsed(clock_t start)
/* compute elapsed time since start in seconds */
{
return ((double)(clock() - start)) / CLOCKS_PER_SEC;
}

static void clearCaches(void)
/* clear caches */
{
const char *cmd = "sudo /root/clearCaches";
verbose(1, "Begin file cache clear\n");
verboseTimeInit();
if (system(cmd) != 0)
    errAbort("%s failed", cmd);
verboseTime(1, "End file cache clear");
}

static struct slName* loadTestFileList(char *testFileList)
/* load test file list */
{
struct slName *testFiles = NULL;
struct lineFile *lf = lineFileOpen(testFileList, TRUE);
char *line;
while (lineFileNextReal(lf, &line))
    {
    if (strlen(line) > 0)
        slAddHead(&testFiles, slNameNew(line));
    }
lineFileClose(&lf);
slReverse(&testFiles);
if (slCount(testFiles) <= 1)
    errAbort("must specify at least two test files");

return testFiles;
}

static unsigned randAccess(BYTE* fmem,
                           off_t testFileSize,
                           int accesses)
/* randomly access file memory */
{
unsigned accum = 0; // make sure not optimized out
for (int i = 0; i < accesses; i++)
    {
    off_t off = round(drand48() * testFileSize);
    accum |= *(fmem + off);
    }
return accum;
}

static void exerciseFile(char *testFile,
                         boolean advRandom,
                         boolean advWillNeed,
                         int accesses)
/* run one round of tests on a file */
{
off_t testFileSize = fileSize(testFile);
int fd = open(testFile, 0);
if (fd < 0)
    errnoAbort("can't open %s", testFile);

int flags = MAP_SHARED;
BYTE* fmem = mmap(NULL, testFileSize, PROT_READ, flags, fd, 0);
if (fmem == MAP_FAILED)
    errnoAbort("mmap failed: %s", testFile);
    
unsigned madvOpts = (advRandom ? MADV_RANDOM : 0)
    | (advWillNeed ? MADV_WILLNEED : 0);

if (madvOpts != 0)
    {
    if (madvise(fmem, testFileSize, madvOpts) < 0)
        errnoAbort("madvise failed: %s", testFile);
    }

randAccess(fmem, testFileSize, accesses);

if (munmap(fmem, testFileSize) < 0)
    errnoAbort("munmap failed");
if (close(fd) < 0)
    errnoAbort("can't close");
}

static void mmapPerf(char *testFileList,
                     char *what,
                     char *outTsv,
                     boolean advRandom, boolean advWillNeed,
                     int accesses, boolean clear) {
/* mmapPerf - Test perfomance of mmap. */
struct slName* testFiles = loadTestFileList(testFileList);

if (clear)
    clearCaches();

exerciseFile(testFiles->name, advRandom, advWillNeed, 10);
int trialCnt = 0;
float totalTime = 0.0, minTime = DBL_MAX, maxTime = 0.0;
for (struct slName* testFile = testFiles->next; testFile != NULL; testFile = testFile->next)
    {
    verbose(1, "Begin trial %d\n", trialCnt);
    verboseTimeInit();
    clock_t start = clock();
    exerciseFile(testFiles->name, advRandom, advWillNeed, accesses);
    double sec = elapsed(start);
    totalTime += sec;
    minTime = min(minTime, sec);
    maxTime = max(maxTime, sec);
    verboseTime(1, "End trial %d", trialCnt);
    trialCnt++;
    }
FILE *fh = mustOpen(outTsv, "w");
fprintf(fh, "what\ttrials\tadvRandom\tadvWillNeed\tclear\taccesses\ttotalTime\tmeanTime\tminTime\tmaxTime\n");
fprintf(fh, "%s\t%d\t%d\t%d\t%d\t%d\t%0.2f\t%0.2f\t%0.2f\t%0.2f\n", what,
        trialCnt, advRandom, advWillNeed, clear, accesses,
        totalTime, totalTime / trialCnt, minTime, maxTime);
carefulClose(&fh);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
mmapPerf(argv[1], argv[2], argv[3],
         optionExists("random"),
         optionExists("willneed"),
         optionInt("accesss", 100000),
         optionExists("clear"));
return 0;
}
