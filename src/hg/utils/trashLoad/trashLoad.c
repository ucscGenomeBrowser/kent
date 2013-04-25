/* trashLoad - generate trash file activity load test. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "options.h"
#include "trashDir.h"

char *testDir = "loadTest";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trashLoad - generate trash file activity load test\n"
  "usage:\n"
  "   trashLoad [options] <numFiles> <averageSize>\n"
  "   constructs a directory ./%s to write files into\n"
  "options:\n"
  "   numFiles - generate this number of files, positive integer\n"
  "   averageSize - files of this average size, positive integer\n"
  "   -testDir=<dirName> - specify a different directory than ./'%s'\n"
  "           note, this will always be relative: ./<dirName>\n"
  "           it can not be an explicit path.",
  testDir, testDir
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"testDir", OPTION_STRING},
   {NULL, 0},
};

/* generate normal distrubion, from information found at:
 *
  http://stackoverflow.com/questions/2325472/generate-random-numbers-following-a-normal-distribution-in-c-c

 * this formula seemed to generate a bi-normal distribution with peaks
 * at -0.5 and 0.5, so, flipping all the negative values turned it into
 * a single normal distribution with a mean near 0.5
 * the tail toward zero is a bit truncated since it doesn't go below zero
 */
#define RANDU ((double) rand()/RAND_MAX)
#define RANDN2(mu, sigma) (mu + (rand()%2 ? -1.0 : 1.0)*sigma*pow(-log(0.99999*RANDU), 0.5))
#define RANDN RANDN2(0, 1.0)

static size_t normalDistribution(long long mean, long long min, long long max)
/* return a random number from a normal distribution at the specified mean
 * within the min,max limits.  The limits distort it a bit, true.
 */
  {
  static int depth = 0;
  double randn = RANDN2(0, 1.0);
  if (randn < 0.0) { randn = -randn; }
  double meanValue = 2.0 * randn * (double)mean;
  size_t returnValue = (size_t)(round(meanValue + 0.5));
  ++depth;
  if (returnValue < min)
    returnValue = min;
  if (returnValue > max)
     {
     if (depth > 40)  /* do not recurse indefinately */
       {
       returnValue = max;
       depth = 1;
       }
     else
       return (normalDistribution(mean, min, max));
     }
  --depth;
  return (returnValue);
  }

void trashLoad(char *fileCount, char *aveSize)
/* trashLoad - generate trash file activity load test. */
{
int i;
long long bytesWritten = 0;
long long numFiles = sqlLongLong(fileCount);
long long averageSize = sqlLongLong(aveSize);
if (numFiles < 1)
  errAbort("ERROR: numFiles must be a positive integer, given: %lld\n", numFiles);
if (averageSize < 1)
  errAbort("ERROR: averageSize must be a positive integer, given: %lld\n", averageSize);
long long maxSize = averageSize * 5;
verbose(1, "# trash load test begin, numFiles: %lld, averageSize: %lld, maximum size: %lld\n", numFiles, averageSize, maxSize);
struct tempName tn;

/* testing normalDistribution, print out a million numbers, send into
 * textHistorgram to view
 */
// int i;
// for ( i = 0; i < 1000000; ++i)
//    printf("%lld\n", (long long)normalDistribution(averageSize, 1, maxSize));

/* generate a single large buffer to write from, and initialize with values */
char *buf = needMem(maxSize+1);
int j;
for ( j = 0; j < maxSize+1; ++j )
    buf[j] =  (char)(0xff & j);

long beginLoadTest = clock1000();
for ( i = 0; i < numFiles; ++i)
  {
  char nameBuffer[1024];
  safef(nameBuffer, sizeof(nameBuffer), "lt_%d", i);
  trashDirFile(&tn, testDir, nameBuffer, ".txt");
  char *fileName = tn.forCgi;
  FILE *fh = mustOpen(fileName, "w");
  size_t writeSize = normalDistribution(averageSize, 1, maxSize);
  bytesWritten += writeSize;
  fwrite(buf, writeSize, 1, fh);
  fclose(fh);
  }

long et = clock1000() - beginLoadTest;
double bytesPerSecond = (double)bytesWritten/(double)((double)et/1000.0);
verbose(1, "# %lld total bytes written in %lld files\n", bytesWritten, numFiles);
verbose(1, "# trash load test total run time: %ld millis, %.0f bytes per second\n", et, bytesPerSecond);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
testDir = optionVal("testDir", testDir);
trashLoad(argv[1], argv[2]);
return 0;
}
