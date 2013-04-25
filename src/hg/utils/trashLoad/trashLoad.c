/* trashLoad - generate trash file activity load test. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "sqlNum.h"
#include "options.h"
#include "trashDir.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "trashLoad - generate trash file activity load test\n"
  "usage:\n"
  "   trashLoad <numFiles> <averageSize>\n"
  "options:\n"
  "   numFiles - generate this number of files, positive integer\n"
  "   averageSize - files of this average size, positive integer"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
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

void trashLoad(int argc, char *argv[])
/* trashLoad - generate trash file activity load test. */
{
long long bytesWritten = 0;
long long numFiles = sqlLongLong(argv[1]);
long long averageSize = sqlLongLong(argv[2]);
if (numFiles < 1)
  errAbort("ERROR: numFiles must be a positive integer, given: %lld\n", numFiles);
if (averageSize < 1)
  errAbort("ERROR: averageSize must be a positive integer, given: %lld\n", averageSize);
long long maxSize = averageSize * 5;
long long i;
verbose(1, "# trash load test begin, numFiles: %lld, averageSize: %lld, maximum size: %lld\n", numFiles, averageSize, maxSize);
struct tempName tn;

/* testing normalDistribution, print out a million numbers, send into
 * textHistorgram to view
 */
// for ( i = 0; i < 1000000; ++i)
//    printf("%lld\n", (long long)normalDistribution(averageSize, 1, maxSize));

long beginLoadTest = clock1000();
for ( i = 0; i < numFiles; ++i)
  {
  char filePrefix[1024];
  safef(filePrefix, sizeof(filePrefix), "lt_%lld", i);
  trashDirFile(&tn, "loadTest", filePrefix, ".txt");
  FILE *fh = mustOpen(tn.forCgi, "w");
  int j;
  char *buf = needMem(maxSize+1);
  for ( j = 0; j < maxSize+1; ++j )
     buf[j] =  (char)(0xff & j);
  size_t writeSize = normalDistribution(averageSize, 1, maxSize);
  bytesWritten += writeSize;
  fwrite(buf, writeSize, 1, fh);
  fclose(fh);
  }
verbose(1, "# trash load test total run time: %ld millis, %lld total bytes written in %lld files\n", clock1000() - beginLoadTest, bytesWritten, numFiles);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
trashLoad(argc, argv);
return 0;
}
