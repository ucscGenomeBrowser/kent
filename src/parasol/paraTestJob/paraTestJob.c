/* paraTestJob - A good test job to run on Parasol.  Can be configured to take a long time or crash. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "paraTestJob - A good test job to run on Parasol.  Can be configured to take a long time or crash\n"
  "usage:\n"
  "   paraTestJob count\n"
  "Run a relatively time consuming algorithm count times.  This\n"
  "algorithm takes about 1/10 per second each time\n"
  "options:\n"
  "   -crash - Try to write to NULL when done.\n"
  "   -err   - Return -1 error code when done\n"
  "   -sleep=n Sleep for N seconds\n"
  );
}

double cumErr = 0.0;

void noteResults(double a, double b)
/* Don't really do anything. */
{
double diff = a - b;
if (diff < 0) diff = -diff;
cumErr += diff;
}

void compute()
/* Do a relatively time consuming calculation.
 * This should last about a second. */
{
int i;
double res;

for (i=0; i<1060000; ++i)
    {
    res = sqrt(i);
    noteResults(i, res*res);
    }
}

void paraTestJob(char *countString)
/* paraTestJob - A good test job to run on Parasol.  Can be configured to take a long time or crash. */
{
int i, count = atoi(countString);
for (i=0; i<count; ++i)
    compute();
printf("Cumulative error %f\n", cumErr);
if (optionExists("sleep"))
    sleep(optionInt("sleep", 1));
if (optionExists("err"))
    errAbort("Aborting with error");
if (optionExists("crash"))
    {
    char *s = NULL;
    s[0] = 0;
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 2)
    usage();
paraTestJob(argv[1]);
return 0;
}
